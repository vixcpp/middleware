/**
 *
 *  @file app_middleware.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_APP_MIDDLEWARE_HPP
#define VIX_APP_MIDDLEWARE_HPP

#include <memory>
#include <utility>
#include <vector>

#include <vix/middleware/app/http_cache.hpp>
#include <vix/http/RequestHandler.hpp>

namespace vix::middleware::app
{
  /** @brief Alias for the HTTP cache app configuration. */
  using HttpCacheConfig = HttpCacheAppConfig;

  /**
   * @brief Build an HTTP cache middleware for App (prefix is ignored).
   *
   * @param cfg Cache configuration (prefix is cleared).
   * @return App middleware.
   */
  inline vix::App::Middleware http_cache(HttpCacheConfig cfg = {})
  {
    cfg.prefix.clear();
    return http_cache_mw(std::move(cfg));
  }

  /**
   * @brief Install the HTTP cache middleware on an app prefix.
   *
   * @param app Target application.
   * @param cfg Cache configuration (uses cfg.prefix).
   */
  inline void use_http_cache(vix::App &app, HttpCacheConfig cfg = {})
  {
    vix::middleware::app::install_http_cache(app, std::move(cfg));
  }

  /**
   * @brief Chain multiple App middlewares in order.
   *
   * Each middleware must call the provided Next to continue.
   */
  inline vix::App::Middleware chain(std::vector<vix::App::Middleware> mws)
  {
    return [mws = std::move(mws)](
               vix::vhttp::Request &req,
               vix::vhttp::ResponseWrapper &res,
               vix::App::Next next) mutable
    {
      auto i = std::make_shared<std::size_t>(0);
      auto step = std::make_shared<vix::App::Next>();

      *step = [i, step, &mws, &req, &res, next]() mutable
      {
        if (*i >= mws.size())
        {
          next();
          return;
        }

        auto &mw = mws[(*i)++];
        mw(req, res, *step);
      };

      (*step)();
    };
  }

  /**
   * @brief Chain two App middlewares.
   */
  inline vix::App::Middleware chain(vix::App::Middleware a, vix::App::Middleware b)
  {
    return chain(std::vector<vix::App::Middleware>{std::move(a), std::move(b)});
  }

  /**
   * @brief Chain three App middlewares.
   */
  inline vix::App::Middleware chain(vix::App::Middleware a, vix::App::Middleware b, vix::App::Middleware c)
  {
    return chain(std::vector<vix::App::Middleware>{std::move(a), std::move(b), std::move(c)});
  }

} // namespace vix::middleware::app

#endif // VIX_APP_MIDDLEWARE_HPP
