/**
 *
 *  @file cors.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_CORS_HPP
#define VIX_CORS_HPP

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>

// ----------------------------------------------------
// Fix: some environments define `cors` as a macro.
// That breaks parsing of `cors(...) { ... }` and causes
// misleading "missing ';'" errors.
// ----------------------------------------------------
#ifdef cors
#if defined(__GNUC__) || defined(__clang__)
#pragma push_macro("cors")
#endif
#undef cors
#define VIX_RESTORE_CORS_MACRO 1
#endif

namespace vix::middleware::security
{
  /**
   * @brief Configuration options for cors() middleware.
   */
  struct CorsOptions
  {
    /**
     * @brief Allowed origins list (exact matches or "*" when allow_any_origin is true).
     *
     * If allow_any_origin is true and allowed_origins is empty, any origin is accepted.
     */
    std::vector<std::string> allowed_origins{};

    /**
     * @brief If true, allow wildcard origin behavior depending on configuration.
     */
    bool allow_any_origin{true};

    /**
     * @brief If true, set Access-Control-Allow-Credentials: true.
     *
     * Note: when credentials are allowed, returning "*" for Allow-Origin is not valid.
     * This middleware will return the request's Origin instead (when allowed).
     */
    bool allow_credentials{false};

    /**
     * @brief Allowed methods for preflight responses.
     */
    std::vector<std::string> allow_methods{"GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"};

    /**
     * @brief Allowed headers for preflight responses.
     *
     * If empty, and Access-Control-Request-Headers is present, the request value is echoed.
     */
    std::vector<std::string> allow_headers{"Content-Type", "Authorization"};

    /**
     * @brief Response headers exposed to the browser.
     */
    std::vector<std::string> expose_headers{};

    /**
     * @brief Preflight cache duration.
     */
    int max_age_seconds{600};

    /**
     * @brief If true, append "Vary: Origin".
     *
     * This is important when Allow-Origin is not "*".
     */
    bool vary_origin{true};
  };

  /**
   * @brief ASCII case-insensitive string equality.
   *
   * @param a First string.
   * @param b Second string.
   * @return true if equal ignoring ASCII case.
   */
  inline bool iequals(std::string_view a, std::string_view b)
  {
    if (a.size() != b.size())
      return false;

    for (std::size_t i = 0; i < a.size(); ++i)
    {
      unsigned char ca = static_cast<unsigned char>(a[i]);
      unsigned char cb = static_cast<unsigned char>(b[i]);

      if (ca >= 'A' && ca <= 'Z')
        ca = static_cast<unsigned char>(ca - 'A' + 'a');
      if (cb >= 'A' && cb <= 'Z')
        cb = static_cast<unsigned char>(cb - 'A' + 'a');

      if (ca != cb)
        return false;
    }
    return true;
  }

  /**
   * @brief Join a list of strings as a comma-separated value (CSV).
   *
   * @param a Input list.
   * @return CSV string.
   */
  inline std::string join_csv(const std::vector<std::string> &a)
  {
    std::string out;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
      if (i)
        out += ", ";
      out += a[i];
    }
    return out;
  }

  /**
   * @brief Check whether a given Origin header value is allowed.
   *
   * Rules:
   * - If origin is empty, it is not allowed.
   * - If allow_any_origin is true and allowed_origins is empty: allow any non-empty origin.
   * - Otherwise, allow exact matches in allowed_origins.
   * - "*" in allowed_origins is accepted only when allow_any_origin is true.
   *
   * @param origin Origin value.
   * @param opt CORS options.
   * @return true if allowed.
   */
  inline bool origin_allowed(std::string_view origin, const CorsOptions &opt)
  {
    if (origin.empty())
      return false;

    if (opt.allow_any_origin && opt.allowed_origins.empty())
      return true;

    for (const auto &o : opt.allowed_origins)
    {
      if (o == "*" && opt.allow_any_origin)
        return true;
      if (o == origin)
        return true;
    }
    return false;
  }

  /**
   * @brief Read a request header in a tolerant way (exact, lowercase, Title-Case).
   *
   * This helps when a request wrapper stores headers with specific casing.
   *
   * @param req Request.
   * @param name Header name to read.
   * @return Header value or empty string.
   */
  inline std::string header_any_case(const vix::middleware::Request &req, std::string_view name)
  {
    std::string v = req.header(std::string(name));
    if (!v.empty())
      return v;

    std::string lower(name);
    for (char &c : lower)
    {
      if (c >= 'A' && c <= 'Z')
        c = static_cast<char>(c - 'A' + 'a');
    }
    v = req.header(lower);
    if (!v.empty())
      return v;

    std::string title(name);
    bool cap = true;
    for (char &c : title)
    {
      if (cap && c >= 'a' && c <= 'z')
        c = static_cast<char>(c - 'a' + 'A');
      else if (!cap && c >= 'A' && c <= 'Z')
        c = static_cast<char>(c - 'A' + 'a');

      cap = (c == '-');
    }
    return req.header(title);
  }

  /**
   * @brief CORS middleware.
   *
   * Handles:
   * - Preflight requests: OPTIONS + Access-Control-Request-Method
   * - Simple requests: apply response headers after next() if origin is present and allowed
   *
   * Preflight behavior:
   * - If origin not allowed: returns a normalized 403 error.
   * - Otherwise: sets allow-origin, allow-methods, allow-headers, max-age, etc. and returns 204.
   *
   * Simple request behavior:
   * - Calls next() first.
   * - If origin present and allowed: applies allow-origin, credentials, expose-headers, and vary.
   *
   * @param opt CORS options.
   * @return A middleware function (MiddlewareFn).
   */
  inline vix::middleware::MiddlewareFn cors(CorsOptions opt = CorsOptions())
  {
    return [opt = std::move(opt)](vix::middleware::Context &ctx,
                                  vix::middleware::Next next) mutable
    {
      auto &req = ctx.req();
      auto &res = ctx.res();

      const std::string origin = header_any_case(req, "Origin");
      const bool has_origin = !origin.empty();
      const bool allowed = has_origin ? origin_allowed(origin, opt) : false;

      const bool is_options = iequals(req.method(), "OPTIONS");
      const std::string acrm = header_any_case(req, "Access-Control-Request-Method");
      const bool is_preflight = is_options && !acrm.empty();

      const bool wildcard_ok =
          (opt.allow_any_origin && opt.allowed_origins.empty() && !opt.allow_credentials);

      auto apply_common = [&]()
      {
        res.header("Access-Control-Allow-Origin", wildcard_ok ? "*" : origin);

        if (opt.allow_credentials)
          res.header("Access-Control-Allow-Credentials", "true");

        if (!opt.expose_headers.empty())
          res.header("Access-Control-Expose-Headers", join_csv(opt.expose_headers));

        if (opt.vary_origin)
          res.append("Vary", "Origin");
      };

      if (is_preflight)
      {
        if (!allowed)
        {
          vix::middleware::Error e;
          e.status = 403;
          e.code = "cors_forbidden";
          e.message = "CORS origin not allowed";
          e.details["origin"] = origin;

          ctx.send_error(vix::middleware::normalize(std::move(e)));
          return;
        }

        apply_common();
        res.status(204);
        res.header("Access-Control-Allow-Methods", join_csv(opt.allow_methods));

        const std::string acrh = header_any_case(req, "Access-Control-Request-Headers");
        if (!opt.allow_headers.empty())
          res.header("Access-Control-Allow-Headers", join_csv(opt.allow_headers));
        else if (!acrh.empty())
          res.header("Access-Control-Allow-Headers", acrh);

        res.header("Access-Control-Max-Age", std::to_string(opt.max_age_seconds));
        res.send();
        return;
      }

      next();

      if (!has_origin || !allowed)
        return;

      apply_common();
    };
  }

} // namespace vix::middleware::security

// Restore macro if it existed
#if defined(VIX_RESTORE_CORS_MACRO)
#undef VIX_RESTORE_CORS_MACRO
#if defined(__GNUC__) || defined(__clang__)
#pragma pop_macro("cors")
#endif
#endif

#endif // VIX_CORS_HPP
