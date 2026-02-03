/**
 *
 *  @file csrf.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_CSRF_HPP
#define VIX_CSRF_HPP

#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::security
{
  /**
   * @brief Configuration options for csrf() middleware.
   */
  struct CsrfOptions
  {
    /**
     * @brief Cookie name that contains the CSRF token.
     */
    std::string cookie_name{"csrf_token"};

    /**
     * @brief Header name expected to carry the CSRF token.
     */
    std::string header_name{"x-csrf-token"};

    /**
     * @brief If true, treat GET as unsafe and require CSRF tokens for GET too.
     */
    bool protect_get{false};
  };

  namespace detail
  {
    /**
     * @brief ASCII case-insensitive equality.
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
     * @brief Check whether a method should be protected by CSRF.
     *
     * By default: POST/PUT/PATCH/DELETE are unsafe.
     * Optionally: GET can be treated as unsafe via protect_get.
     *
     * @param m HTTP method.
     * @param protect_get Whether GET is considered unsafe.
     * @return true if unsafe.
     */
    inline bool is_unsafe_method(std::string_view m, bool protect_get)
    {
      if (protect_get && iequals(m, "GET"))
        return true;
      return iequals(m, "POST") || iequals(m, "PUT") || iequals(m, "PATCH") || iequals(m, "DELETE");
    }

    /**
     * @brief Extract a cookie value by name from a Cookie header string.
     *
     * This is a lightweight parser for "Cookie: a=b; c=d" style headers.
     * It does not perform URL-decoding.
     *
     * @param cookie_header Raw Cookie header value.
     * @param name Cookie name to extract.
     * @return Cookie value or empty string if not found.
     */
    inline std::string extract_cookie(std::string_view cookie_header, std::string_view name)
    {
      std::string_view s = cookie_header;
      while (!s.empty())
      {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == ';'))
          s.remove_prefix(1);

        auto eq = s.find('=');
        if (eq == std::string_view::npos)
          break;

        auto key = s.substr(0, eq);
        s.remove_prefix(eq + 1);

        auto end = s.find(';');
        auto val = (end == std::string_view::npos) ? s : s.substr(0, end);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
          key.remove_suffix(1);

        if (key == name)
          return std::string(val);

        if (end == std::string_view::npos)
          break;
        s.remove_prefix(end + 1);
      }
      return {};
    }
  } // namespace detail

  /**
   * @brief CSRF protection middleware (double-submit cookie).
   *
   * This middleware enforces that, for unsafe methods, the client sends:
   * - a token in a header (header_name)
   * - the same token in a cookie (cookie_name)
   *
   * If either token is missing or they do not match, the request is rejected
   * with a normalized 403 error.
   *
   * @note This is the "double submit cookie" pattern. The middleware assumes
   * the application already sets the CSRF cookie on the client.
   *
   * @param opt CSRF options.
   * @return A middleware function (MiddlewareFn).
   */
  inline MiddlewareFn csrf(CsrfOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      const auto &req = ctx.req();

      if (!detail::is_unsafe_method(req.method(), opt.protect_get))
      {
        next();
        return;
      }

      const std::string header_token = req.header(opt.header_name);
      const std::string cookie = req.header("cookie");
      const std::string cookie_token = detail::extract_cookie(cookie, opt.cookie_name);

      if (header_token.empty() || cookie_token.empty() || header_token != cookie_token)
      {
        Error e;
        e.status = 403;
        e.code = "csrf_failed";
        e.message = "CSRF token missing or invalid";
        ctx.send_error(normalize(std::move(e)));
        return;
      }

      next();
    };
  }

} // namespace vix::middleware::security

#endif // VIX_CSRF_HPP
