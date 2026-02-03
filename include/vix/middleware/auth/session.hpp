/**
 *
 *  @file session.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_MIDDLEWARE_AUTH_SESSION_HPP
#define VIX_MIDDLEWARE_AUTH_SESSION_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::auth
{
  /**
   * @brief Session data stored per client.
   */
  struct Session
  {
    std::string id;
    std::unordered_map<std::string, std::string> data;

    bool is_new{false};
    bool dirty{false};
    bool destroyed{false};

    /** @brief Set a key/value and mark session dirty. */
    void set(std::string k, std::string v);

    /** @brief Get a value by key. */
    std::optional<std::string> get(const std::string &k) const;

    /** @brief Remove a key and mark session dirty. */
    void erase(const std::string &k);

    /** @brief Mark the session for destruction. */
    void destroy();
  };

  /**
   * @brief Session storage interface.
   */
  class ISessionStore
  {
  public:
    virtual ~ISessionStore() = default;

    /** @brief Load a session by id. */
    virtual std::optional<Session> load(const std::string &sid) = 0;

    /** @brief Persist a session for a given TTL. */
    virtual void save(const Session &s, std::chrono::seconds ttl) = 0;

    /** @brief Destroy a session by id. */
    virtual void destroy(const std::string &sid) = 0;
  };

  /**
   * @brief In-memory session store (process-local).
   */
  class InMemorySessionStore final : public ISessionStore
  {
  public:
    std::optional<Session> load(const std::string &sid) override;
    void save(const Session &s, std::chrono::seconds ttl) override;
    void destroy(const std::string &sid) override;

  private:
    std::unordered_map<std::string, Session> map_;
  };

  /**
   * @brief Session middleware options.
   */
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

  /**
   * @brief Session middleware.
   *
   * Loads or creates a session, exposes it in request state, and persists changes.
   */
  MiddlewareFn session(SessionOptions opt);

} // namespace vix::middleware::auth

#endif // VIX_MIDDLEWARE_AUTH_SESSION_HPP
