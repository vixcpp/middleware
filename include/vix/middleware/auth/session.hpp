#ifndef VIX_MIDDLEWARE_AUTH_SESSION_HPP
#define VIX_MIDDLEWARE_AUTH_SESSION_HPP

#include <string>
#include <unordered_map>
#include <optional>
#include <memory>
#include <chrono>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::auth
{
  struct Session
  {
    std::string id;
    std::unordered_map<std::string, std::string> data;

    bool is_new{false};
    bool dirty{false};
    bool destroyed{false};

    void set(std::string k, std::string v);
    std::optional<std::string> get(const std::string &k) const;
    void erase(const std::string &k);
    void destroy();
  };

  class ISessionStore
  {
  public:
    virtual ~ISessionStore() = default;
    virtual std::optional<Session> load(const std::string &sid) = 0;
    virtual void save(const Session &s, std::chrono::seconds ttl) = 0;
    virtual void destroy(const std::string &sid) = 0;
  };

  class InMemorySessionStore final : public ISessionStore
  {
  public:
    std::optional<Session> load(const std::string &sid) override;
    void save(const Session &s, std::chrono::seconds ttl) override;
    void destroy(const std::string &sid) override;

  private:
    std::unordered_map<std::string, Session> map_;
  };

  struct SessionOptions
  {
    std::shared_ptr<ISessionStore> store{};

    std::string secret; // required
    std::string cookie_name{"sid"};
    std::string cookie_path{"/"};
    bool secure{false};
    bool http_only{true};
    std::string same_site{"Lax"};

    std::chrono::seconds ttl{std::chrono::hours(24 * 7)};
    bool auto_create{true};
  };

  MiddlewareFn session(SessionOptions opt);

} // namespace vix::middleware::auth

#endif
