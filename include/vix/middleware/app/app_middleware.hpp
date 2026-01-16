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

#include <vix/middleware/app/http_cache.hpp>

namespace vix::middleware::app
{
  using HttpCacheConfig = HttpCacheAppConfig;

  inline vix::App::Middleware http_cache(HttpCacheConfig cfg = {})
  {
    cfg.prefix.clear();
    return http_cache_mw(std::move(cfg));
  }

  inline void use_http_cache(vix::App &app, HttpCacheConfig cfg = {})
  {
    vix::middleware::app::install_http_cache(app, std::move(cfg));
  }

  inline vix::App::Middleware chain(std::vector<vix::App::Middleware> mws)
  {
    return [mws = std::move(mws)](vix::Request &req, vix::Response &res, vix::App::Next next) mutable
    {
      std::size_t i = 0;

      vix::App::Next step = [&]()
      {
        if (i >= mws.size())
        {
          next();
          return;
        }

        auto &mw = mws[i++];
        mw(req, res, step);
      };

      step();
    };
  }

  inline vix::App::Middleware chain(vix::App::Middleware a, vix::App::Middleware b)
  {
    return chain(std::vector<vix::App::Middleware>{std::move(a), std::move(b)});
  }

  inline vix::App::Middleware chain(vix::App::Middleware a, vix::App::Middleware b, vix::App::Middleware c)
  {
    return chain(std::vector<vix::App::Middleware>{std::move(a), std::move(b), std::move(c)});
  }

}

#endif
