/**
 *
 *  @file cookies.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_MIDDLEWARE_HTTP_COOKIES_HPP
#define VIX_MIDDLEWARE_HTTP_COOKIES_HPP

#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <cctype>

#include <boost/beast/core/string.hpp>
#include <boost/beast/http/field.hpp>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::cookies
{
  /**
   * @brief HTTP cookie representation.
   */
  struct Cookie
  {
    std::string name;
    std::string value;

    std::string path{"/"};
    std::string domain{};
    int max_age{-1}; // -1 => omit
    bool http_only{true};
    bool secure{false};
    std::string same_site{"Lax"}; // Lax | Strict | None
  };

  /** @brief Trim ASCII whitespace from both ends. */
  inline std::string trim(std::string_view s)
  {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
      b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
      e--;
    return std::string(s.substr(b, e - b));
  }

  /**
   * @brief Parse a Cookie header into key/value pairs.
   */
  inline std::unordered_map<std::string, std::string> parse_cookie_header(std::string_view header)
  {
    std::unordered_map<std::string, std::string> out;

    size_t i = 0;
    while (i < header.size())
    {
      size_t semi = header.find(';', i);
      if (semi == std::string_view::npos)
        semi = header.size();

      auto part = header.substr(i, semi - i);
      i = (semi < header.size()) ? semi + 1 : semi;

      auto eq = part.find('=');
      if (eq == std::string_view::npos)
        continue;

      std::string k = trim(part.substr(0, eq));
      std::string v = trim(part.substr(eq + 1));

      if (!k.empty())
        out.emplace(std::move(k), std::move(v));
    }

    return out;
  }

  /**
   * @brief Parse cookies from a request.
   */
  inline std::unordered_map<std::string, std::string> parse(const vix::middleware::Request &req)
  {
    const std::string h = req.header("cookie");
    if (h.empty())
      return {};
    return parse_cookie_header(h);
  }

  /**
   * @brief Get a single cookie value by name.
   */
  inline std::optional<std::string> get(const vix::middleware::Request &req, std::string_view name)
  {
    const std::string h = req.header("cookie");
    if (h.empty())
      return std::nullopt;

    auto m = parse_cookie_header(h);
    auto it = m.find(std::string(name));
    if (it == m.end())
      return std::nullopt;
    return it->second;
  }

  /**
   * @brief Build a Set-Cookie header value.
   */
  inline std::string build_set_cookie_value(const Cookie &c)
  {
    std::string s;
    s.reserve(128);

    s += c.name;
    s += '=';
    s += c.value;

    if (!c.path.empty())
    {
      s += "; Path=";
      s += c.path;
    }
    if (!c.domain.empty())
    {
      s += "; Domain=";
      s += c.domain;
    }
    if (c.max_age >= 0)
    {
      s += "; Max-Age=";
      s += std::to_string(c.max_age);
    }

    if (c.http_only)
      s += "; HttpOnly";
    if (c.secure)
      s += "; Secure";

    if (!c.same_site.empty())
    {
      s += "; SameSite=";
      s += c.same_site;
    }

    return s;
  }

  /**
   * @brief Add a Set-Cookie header to the response.
   */
  inline void set(vix::middleware::Response &res, const Cookie &c)
  {
    using boost::beast::string_view;

    const std::string v = build_set_cookie_value(c);

    res.res.insert(
        string_view{"Set-Cookie", 10},
        string_view{v.data(), v.size()});
  }

} // namespace vix::middleware::cookies

#endif // VIX_MIDDLEWARE_HTTP_COOKIES_HPP
