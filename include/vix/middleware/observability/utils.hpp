/**
 *
 *  @file utils.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_MIDDLEWARE_UTILS_HPP
#define VIX_MIDDLEWARE_UTILS_HPP

#include <string>
#include <string_view>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::observability
{
  inline std::string safe_method(const vix::middleware::Request &req)
  {
    const auto &m = req.method();
    return m.empty() ? std::string("GET") : m;
  }

  inline std::string safe_path(const vix::middleware::Request &req)
  {
    const auto &p = req.path();
    return p.empty() ? std::string("/") : p;
  }
}

#endif
