#include <vix/middleware/auth/session.hpp>
#include <vix/middleware/http/cookies.hpp>

#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <random>

namespace vix::middleware::auth
{
  void Session::set(std::string k, std::string v)
  {
    data[std::move(k)] = std::move(v);
    dirty = true;
  }

  std::optional<std::string> Session::get(const std::string &k) const
  {
    auto it = data.find(k);
    if (it == data.end())
      return std::nullopt;
    return it->second;
  }

  void Session::erase(const std::string &k)
  {
    auto it = data.find(k);
    if (it != data.end())
    {
      data.erase(it);
      dirty = true;
    }
  }

  void Session::destroy()
  {
    destroyed = true;
    dirty = true;
  }

  std::optional<Session> InMemorySessionStore::load(const std::string &sid)
  {
    auto it = map_.find(sid);
    if (it == map_.end())
      return std::nullopt;
    return it->second;
  }

  void InMemorySessionStore::save(const Session &s, std::chrono::seconds)
  {
    map_[s.id] = s;
  }

  void InMemorySessionStore::destroy(const std::string &sid)
  {
    map_.erase(sid);
  }

  static std::string b64url_encode(const unsigned char *data, size_t len)
  {
    std::string b64;
    b64.resize(4 * ((len + 2) / 3));

    int out_len = EVP_EncodeBlock(
        reinterpret_cast<unsigned char *>(&b64[0]),
        data,
        static_cast<int>(len));

    b64.resize(static_cast<size_t>(out_len));

    for (char &c : b64)
    {
      if (c == '+')
        c = '-';
      else if (c == '/')
        c = '_';
    }
    while (!b64.empty() && b64.back() == '=')
      b64.pop_back();
    return b64;
  }

  static std::optional<std::string> hmac_sha256_b64url(std::string_view msg, std::string_view secret)
  {
    unsigned int out_len = 0;
    unsigned char out[EVP_MAX_MD_SIZE];

    if (!HMAC(EVP_sha256(),
              secret.data(),
              static_cast<int>(secret.size()),
              reinterpret_cast<const unsigned char *>(msg.data()),
              msg.size(),
              out,
              &out_len))
    {
      return std::nullopt;
    }

    return b64url_encode(out, static_cast<size_t>(out_len));
  }

  static std::string random_sid()
  {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static const char *hex = "0123456789abcdef";

    std::string s;
    s.resize(32);
    for (size_t i = 0; i < 16; ++i)
    {
      uint8_t b = static_cast<uint8_t>(rng() & 0xFF);
      s[i * 2] = hex[(b >> 4) & 0xF];
      s[i * 2 + 1] = hex[b & 0xF];
    }
    return s;
  }

  static std::string sign_sid(const std::string &sid, const std::string &secret)
  {
    auto sig = hmac_sha256_b64url(sid, secret).value_or("");
    return sid + "." + sig;
  }

  static bool verify_sid(const std::string &cookie_value, const std::string &secret, std::string &out_sid)
  {
    auto dot = cookie_value.find('.');
    if (dot == std::string::npos)
      return false;

    std::string sid = cookie_value.substr(0, dot);
    std::string sig = cookie_value.substr(dot + 1);

    auto expected = hmac_sha256_b64url(sid, secret);
    if (!expected)
      return false;
    if (*expected != sig)
      return false;

    out_sid = std::move(sid);
    return true;
  }

  MiddlewareFn session(SessionOptions opt)
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      if (!opt.store)
        opt.store = std::make_shared<InMemorySessionStore>();

      if (!opt.store || opt.secret.empty())
      {
        Error e;
        e.status = 500;
        e.code = "session_misconfigured";
        e.message = "Session store or secret missing";
        ctx.send_error(normalize(std::move(e)));
        return;
      }

      bool has_session = false;
      vix::middleware::auth::Session tmp;

      if (auto raw = vix::middleware::http::get(ctx.req(), opt.cookie_name))
      {
        std::string sid;
        if (verify_sid(*raw, opt.secret, sid))
        {
          if (auto loaded = opt.store->load(sid))
          {
            tmp = std::move(*loaded);
            tmp.id = sid;
            has_session = true;
          }
        }
      }

      if (!has_session && opt.auto_create)
      {
        tmp.id = random_sid();
        tmp.is_new = true;
        has_session = true;
      }

      vix::middleware::auth::Session *ps = nullptr;
      if (has_session)
        ps = &ctx.emplace_state<vix::middleware::auth::Session>(std::move(tmp));

      next();

      if (!ps)
        return;

      if (ps->destroyed)
      {
        opt.store->destroy(ps->id);

        vix::middleware::http::Cookie c;
        c.name = opt.cookie_name;
        c.value = "";
        c.path = opt.cookie_path;
        c.http_only = opt.http_only;
        c.secure = opt.secure;
        c.same_site = opt.same_site;
        c.max_age = 0;
        vix::middleware::http::set(ctx.res(), c);
        return;
      }

      if (ps->is_new || ps->dirty)
        opt.store->save(*ps, opt.ttl);

      if (ps->is_new)
      {
        vix::middleware::http::Cookie c;
        c.name = opt.cookie_name;
        c.value = sign_sid(ps->id, opt.secret);
        c.path = opt.cookie_path;
        c.http_only = opt.http_only;
        c.secure = opt.secure;
        c.same_site = opt.same_site;
        c.max_age = static_cast<int>(opt.ttl.count());
        vix::middleware::http::set(ctx.res(), c);
      }
    };
  }

} // namespace vix::middleware::auth
