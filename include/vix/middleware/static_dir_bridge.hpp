/**
 *
 *  @file static_dir_bridge.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_MIDDLEWARE_STATIC_DIR_BRIDGE_HPP
#define VIX_MIDDLEWARE_STATIC_DIR_BRIDGE_HPP

#include <vix/app/App.hpp>
#include <vix/middleware/middleware.hpp>
#include <vix/middleware/performance/static_files.hpp>

namespace vix::middleware
{
  inline void register_static_dir()
  {
    static vix::mw::Services g_services{};

    vix::App::set_static_handler(
        [](vix::App &app,
           const std::filesystem::path &root,
           const std::string &mount,
           const std::string &index_file,
           bool add_cache_control,
           const std::string &cache_control,
           bool fallthrough) -> bool
        {
          vix::middleware::performance::StaticFilesOptions opt;
          opt.mount = mount;
          opt.index_file = index_file;
          opt.add_cache_control = add_cache_control;
          opt.cache_control = cache_control;
          opt.fallthrough = fallthrough;

          auto mw = vix::middleware::performance::static_files(root, std::move(opt));

          auto httpmw = vix::middleware::to_http_middleware(std::move(mw), g_services);

          vix::App::Middleware appmw =
              [httpmw = std::move(httpmw)](
                  vix::vhttp::Request &req,
                  vix::vhttp::ResponseWrapper &res,
                  vix::App::Next next) mutable
          {
            httpmw(req, res, vix::mw::Next([n = std::move(next)]() mutable
                                           { n(); }));
          };

          app.use(std::string(mount), std::move(appmw));
          return true;
        });
  }
}

#endif
