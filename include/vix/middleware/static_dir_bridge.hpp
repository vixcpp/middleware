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
#include <vix/middleware/performance/static_compression.hpp>

namespace vix::middleware
{
  /**
   * @brief Register the static response hook for vix::App.
   *
   * This keeps vix::core independent from vix::middleware while allowing
   * the middleware module to add gzip compression to static file responses.
   */
  inline void register_static_dir()
  {
    vix::App::set_static_response_hook(
        vix::middleware::performance::compressed_static_response_hook());
  }

} // namespace vix::middleware

#endif // VIX_MIDDLEWARE_STATIC_DIR_BRIDGE_HPP
