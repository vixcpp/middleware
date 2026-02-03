/**
 *
 *  @file http_cache.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_HTTP_CACHE_HPP
#define VIX_HTTP_CACHE_HPP

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <vix/middleware/app/adapter.hpp>
#include <vix/middleware/http_cache.hpp>

#include <vix/cache/Cache.hpp>
#include <vix/cache/CachePolicy.hpp>
#include <vix/cache/MemoryStore.hpp>

namespace vix::middleware::app
{
  /**
   * @brief Configuration for installing the HTTP cache middleware in an App.
   */
  struct HttpCacheAppConfig
  {
    std::string prefix{"/api/"};
    bool only_get{true};
    int ttl_ms{30'000};

    bool allow_bypass{true};
    std::string bypass_header{"x-vix-cache"};
    std::string bypass_value{"bypass"};

    std::vector<std::string> vary_headers{};
    std::shared_ptr<vix::cache::Cache> cache{};

    bool add_debug_header{false};
    std::string debug_header{"x-vix-cache-status"};
  };

  /**
   * @brief Create a default in-memory cache instance from app config.
   *
   * @param cfg App-level cache configuration.
   * @return Shared cache instance.
   */
  inline std::shared_ptr<vix::cache::Cache>
  make_default_cache(const HttpCacheAppConfig &cfg)
  {
    auto store = std::make_shared<vix::cache::MemoryStore>();
    vix::cache::CachePolicy policy;
    policy.ttl_ms = cfg.ttl_ms;

    return std::make_shared<vix::cache::Cache>(policy, store);
  }

  /**
   * @brief Build an App middleware that caches HTTP responses.
   *
   * @param cfg App-level cache configuration.
   * @return App middleware.
   */
  inline vix::App::Middleware http_cache_mw(HttpCacheAppConfig cfg = {})
  {
    auto cache = cfg.cache ? std::move(cfg.cache) : make_default_cache(cfg);

    vix::middleware::HttpCacheOptions opt{};
    opt.allow_bypass = cfg.allow_bypass;
    opt.bypass_header = cfg.bypass_header;
    opt.bypass_value = cfg.bypass_value;
    opt.vary_headers = std::move(cfg.vary_headers);

    auto inner = vix::middleware::http_cache(std::move(cache), opt);
    auto mw = vix::middleware::app::adapt(std::move(inner));

    if (cfg.only_get)
    {
      mw = vix::middleware::app::when(
          [](const vix::vhttp::Request &req)
          { return req.method() == "GET"; },
          std::move(mw));
    }

    return mw;
  }

  /**
   * @brief Install the HTTP cache middleware on an app prefix.
   *
   * @param app Target application.
   * @param cfg App-level cache configuration.
   */
  inline void install_http_cache(vix::App &app, HttpCacheAppConfig cfg = {})
  {
    std::string prefix = cfg.prefix;
    cfg.prefix.clear();

    auto mw = http_cache_mw(std::move(cfg));
    vix::middleware::app::install(app, std::move(prefix), std::move(mw));
  }

} // namespace vix::middleware::app

#endif // VIX_HTTP_CACHE_HPP
