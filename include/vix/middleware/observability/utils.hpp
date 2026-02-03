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
  /**
   * @brief Return a safe HTTP method string for metrics and tracing.
   *
   * If the request method is empty, this function returns "GET" as a
   * reasonable default. This avoids emitting empty labels in metrics
   * and logs.
   *
   * @param req HTTP request.
   * @return HTTP method or "GET" if missing.
   */
  inline std::string safe_method(const vix::middleware::Request &req)
  {
    const auto &m = req.method();
    return m.empty() ? std::string("GET") : m;
  }

  /**
   * @brief Return a safe HTTP path string for metrics and tracing.
   *
   * If the request path is empty, this function returns "/" as a
   * reasonable default. This avoids emitting empty labels in metrics
   * and logs.
   *
   * @param req HTTP request.
   * @return Request path or "/" if missing.
   */
  inline std::string safe_path(const vix::middleware::Request &req)
  {
    const auto &p = req.path();
    return p.empty() ? std::string("/") : p;
  }

} // namespace vix::middleware::observability

#endif // VIX_MIDDLEWARE_UTILS_HPP
