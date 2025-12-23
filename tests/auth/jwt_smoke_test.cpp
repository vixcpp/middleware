#include <cassert>
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/auth/jwt.hpp>

using namespace vix::middleware;

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

static std::string hmac_sha256_b64url(std::string_view msg, std::string_view secret)
{
    unsigned int out_len = 0;
    unsigned char out[EVP_MAX_MD_SIZE];

    HMAC(EVP_sha256(),
         secret.data(),
         static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char *>(msg.data()),
         msg.size(),
         out,
         &out_len);

    return b64url_encode(out, static_cast<size_t>(out_len));
}

static std::string make_jwt_hs256(const nlohmann::json &payload, const std::string &secret)
{
    nlohmann::json header = {{"alg", "HS256"}, {"typ", "JWT"}};

    const std::string h = header.dump();
    const std::string p = payload.dump();

    const std::string h64 = b64url_encode(reinterpret_cast<const unsigned char *>(h.data()), h.size());
    const std::string p64 = b64url_encode(reinterpret_cast<const unsigned char *>(p.data()), p.size());

    const std::string signing = h64 + "." + p64;
    const std::string sig = hmac_sha256_b64url(signing, secret);

    return signing + "." + sig;
}

static vix::vhttp::RawRequest make_req_with_bearer(std::string token)
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::get, "/secure", 11};
    req.set(http::field::host, "localhost");
    req.set("authorization", "Bearer " + token);
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    const std::string secret = "dev_secret";

    HttpPipeline p;
    auth::JwtOptions opt{};
    opt.secret = secret;
    opt.verify_exp = false;
    p.use(auth::jwt(opt));

    {
        nlohmann::json payload = {{"sub", "user123"}, {"roles", {"admin"}}};
        const std::string token = make_jwt_hs256(payload, secret);

        auto raw = make_req_with_bearer(token);
        http::response<http::string_body> res;

        vix::vhttp::Request req(raw, {});
        vix::vhttp::ResponseWrapper w(res);

        p.run(req, w, [&](Request &r, Response &)
              {
                  auto &claims = r.state<auth::JwtClaims>();
                  assert(claims.subject == "user123");
                  assert(!claims.roles.empty());
                  assert(claims.roles[0] == "admin");
                  w.ok().text("OK"); });

        assert(res.result_int() == 200);
        assert(res.body() == "OK");
    }

    {
        nlohmann::json payload = {{"sub", "user123"}};
        const std::string token = make_jwt_hs256(payload, "wrong_secret");

        auto raw = make_req_with_bearer(token);
        http::response<http::string_body> res;

        vix::vhttp::Request req(raw, {});
        vix::vhttp::ResponseWrapper w(res);

        p.run(req, w, [&](Request &, Response &)
              { w.ok().text("SHOULD NOT"); });

        assert(res.result_int() == 401);
        assert(res.body().find("signature") != std::string::npos ||
               res.body().find("invalid_token") != std::string::npos);
    }

    std::cout << "[OK] jwt middleware\n";
    return 0;
}
