/**
 *
 *  @file header.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_HEADERS_HPP
#define VIX_HEADERS_HPP

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::security
{
  /**
   * @brief Configuration options for security response headers.
   *
   * These headers harden browser behavior against common attacks such as
   * MIME sniffing, clickjacking, XSS, data exfiltration, and insecure transport.
   *
   * All options are applied *after* the downstream middleware/handler has run.
   */
  struct SecurityHeadersOptions
  {
    /**
     * @brief Add "X-Content-Type-Options: nosniff".
     *
     * Prevents browsers from MIME-sniffing responses away from the declared
     * Content-Type.
     */
    bool x_content_type_options{true};

    /**
     * @brief Add "X-Frame-Options: DENY".
     *
     * Prevents the page from being embedded in frames or iframes (clickjacking).
     */
    bool x_frame_options{true};

    /**
     * @brief Add "X-XSS-Protection".
     *
     * Legacy header for older browsers. Disabled by default as modern browsers
     * ignore or deprecate it.
     */
    bool x_xss_protection{false};

    /**
     * @brief Add "Referrer-Policy: no-referrer".
     *
     * Prevents the browser from sending the Referer header.
     */
    bool referrer_policy{true};

    /**
     * @brief Add a minimal "Permissions-Policy".
     *
     * Disables sensitive browser features such as geolocation, microphone,
     * and camera by default.
     */
    bool permissions_policy{true};

    /**
     * @brief Enable HTTP Strict Transport Security (HSTS).
     *
     * Disabled by default. Should only be enabled when the application is
     * served exclusively over HTTPS.
     */
    bool hsts{false};

    /**
     * @brief HSTS max-age in seconds.
     *
     * Default: 1 year.
     */
    int hsts_max_age{31536000};

    /**
     * @brief Include subdomains in HSTS policy.
     */
    bool hsts_include_subdomains{true};

    /**
     * @brief Enable HSTS preload flag.
     *
     * Use with caution: requires domain submission to browser preload lists.
     */
    bool hsts_preload{false};

    /**
     * @brief Content-Security-Policy header value.
     *
     * Empty string means no CSP header is set.
     */
    std::string content_security_policy{};
  };

  /**
   * @brief Security headers middleware.
   *
   * Applies a set of HTTP response headers that improve security posture.
   * The middleware always calls next() first, then appends headers to the
   * outgoing response.
   *
   * @param opt Security headers configuration.
   * @return A middleware function (MiddlewareFn).
   */
  inline MiddlewareFn headers(SecurityHeadersOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      next();

      auto &res = ctx.res();

      if (opt.x_content_type_options)
        res.header("X-Content-Type-Options", "nosniff");

      if (opt.x_frame_options)
        res.header("X-Frame-Options", "DENY");

      if (opt.x_xss_protection)
        res.header("X-XSS-Protection", "0");

      if (opt.referrer_policy)
        res.header("Referrer-Policy", "no-referrer");

      if (opt.permissions_policy)
        res.header("Permissions-Policy", "geolocation=(), microphone=(), camera=()");

      if (!opt.content_security_policy.empty())
        res.header("Content-Security-Policy", opt.content_security_policy);

      if (opt.hsts)
      {
        std::string v = "max-age=" + std::to_string(opt.hsts_max_age);
        if (opt.hsts_include_subdomains)
          v += "; includeSubDomains";
        if (opt.hsts_preload)
          v += "; preload";
        res.header("Strict-Transport-Security", v);
      }
    };
  }

} // namespace vix::middleware::security

#endif // VIX_HEADERS_HPP
