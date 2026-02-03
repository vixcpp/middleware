/**
 *
 *  @file ip_filter.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_IP_FILTER_HPP
#define VIX_IP_FILTER_HPP

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::security
{
  /**
   * @brief Configuration options for ip_filter() middleware.
   */
  struct IpFilterOptions
  {
    /**
     * @brief List of allowed client IPs.
     *
     * If non-empty, only IPs present in this list are accepted.
     */
    std::vector<std::string> allow{};

    /**
     * @brief List of denied client IPs.
     *
     * Deny rules are evaluated before allow rules.
     */
    std::vector<std::string> deny{};

    /**
     * @brief Header used to extract the client IP.
     *
     * Default: "x-forwarded-for", commonly set by proxies.
     */
    std::string header_name{"x-forwarded-for"};

    /**
     * @brief Whether to use fallback headers when the main header is missing.
     *
     * Fallbacks include headers such as "x-real-ip".
     */
    bool use_remote_addr_fallback{true};
  };

  /**
   * @brief Extract the client IP address from the request.
   *
   * Resolution order:
   * 1) header_name (first token if CSV, e.g. X-Forwarded-For)
   * 2) x-real-ip (if fallback enabled)
   *
   * This is a best-effort extraction based on common proxy conventions.
   *
   * @param req Request.
   * @param opt IP filter options.
   * @return Extracted client IP or empty string if unavailable.
   */
  inline std::string extract_client_ip(const vix::middleware::Request &req, const IpFilterOptions &opt)
  {
    std::string ip = req.header(opt.header_name);

    // x-forwarded-for may contain "client, proxy1, proxy2"
    auto comma = ip.find(',');
    if (comma != std::string::npos)
      ip = ip.substr(0, comma);

    // trim spaces
    while (!ip.empty() && (ip.front() == ' ' || ip.front() == '\t'))
      ip.erase(ip.begin());
    while (!ip.empty() && (ip.back() == ' ' || ip.back() == '\t'))
      ip.pop_back();

    if (!ip.empty())
      return ip;

    if (!opt.use_remote_addr_fallback)
      return {};

    ip = req.header("x-real-ip");
    if (!ip.empty())
      return ip;

    return {};
  }

  /**
   * @brief Check if a value exists in a vector of strings.
   *
   * @param a Vector to search.
   * @param v Value to find.
   * @return true if found.
   */
  inline bool contains(const std::vector<std::string> &a, std::string_view v)
  {
    for (const auto &x : a)
    {
      if (x == v)
        return true;
    }
    return false;
  }

  /**
   * @brief IP filtering middleware.
   *
   * Behavior:
   * - Extracts the client IP using extract_client_ip().
   * - If the IP is present in the deny list, the request is rejected with 403.
   * - If the allow list is non-empty and the IP is not present, the request is rejected.
   * - Otherwise, the request is allowed and next() is called.
   *
   * @param opt IP filter options.
   * @return A middleware function (MiddlewareFn).
   */
  inline MiddlewareFn ip_filter(IpFilterOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      const std::string ip = extract_client_ip(ctx.req(), opt);

      if (!opt.deny.empty() && contains(opt.deny, ip))
      {
        Error e;
        e.status = 403;
        e.code = "ip_denied";
        e.message = "Client IP is denied";
        e.details["ip"] = ip;
        ctx.send_error(normalize(std::move(e)));
        return;
      }

      if (!opt.allow.empty() && !contains(opt.allow, ip))
      {
        Error e;
        e.status = 403;
        e.code = "ip_not_allowed";
        e.message = "Client IP is not in allow list";
        e.details["ip"] = ip;
        ctx.send_error(normalize(std::move(e)));
        return;
      }

      next();
    };
  }

} // namespace vix::middleware::security

#endif // VIX_IP_FILTER_HPP
