/**
 *
 *  @file etag.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_ETAG_HPP
#define VIX_ETAG_HPP

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::performance
{
  /**
   * @brief Configuration options for etag() middleware.
   */
  struct EtagOptions
  {
    bool weak{true};
    bool add_cache_control_if_missing{false};
    std::string cache_control{"public, max-age=0"};
    std::size_t min_body_size{1};
  };

  inline std::uint64_t fnv1a_64(std::string_view s)
  {
    std::uint64_t h = 1469598103934665603ull;

    for (unsigned char c : s)
    {
      h ^= static_cast<std::uint64_t>(c);
      h *= 1099511628211ull;
    }

    return h;
  }

  inline std::string to_hex_u64(std::uint64_t v)
  {
    static const char *hex = "0123456789abcdef";
    std::string out(16, '0');

    for (int i = 15; i >= 0; --i)
    {
      out[static_cast<std::size_t>(i)] = hex[v & 0xFULL];
      v >>= 4;
    }

    return out;
  }

  inline bool header_name_equals_icase(std::string_view a, std::string_view b)
  {
    if (a.size() != b.size())
      return false;

    for (std::size_t i = 0; i < a.size(); ++i)
    {
      const auto ca = static_cast<unsigned char>(a[i]);
      const auto cb = static_cast<unsigned char>(b[i]);

      if (std::tolower(ca) != std::tolower(cb))
        return false;
    }

    return true;
  }

  inline std::string request_header_icase(
      const vix::middleware::Request &req,
      std::string_view name)
  {
    std::string value = req.header(name);

    if (!value.empty())
      return value;

    for (const auto &[header_name, header_value] : req.headers())
    {
      if (header_name_equals_icase(header_name, name))
        return header_value;
    }

    return {};
  }

  inline bool method_allows_etag(const vix::middleware::Request &req)
  {
    const auto &m = req.method();
    return (m == "GET" || m == "HEAD");
  }

  inline MiddlewareFn etag(EtagOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      if (!method_allows_etag(ctx.req()))
      {
        next();
        return;
      }

      next();

      auto &res = ctx.res();
      const int sc = res.res.status();

      if (sc < 200 || sc >= 300)
        return;

      const std::string body = res.res.body();

      if (body.size() < opt.min_body_size)
        return;

      const std::uint64_t h = fnv1a_64(body);
      std::string tag = "\"" + to_hex_u64(h) + "\"";

      if (opt.weak)
        tag = "W/" + tag;

      res.header("ETag", tag);

      if (opt.add_cache_control_if_missing)
      {
        const std::string cc = res.res.header("Cache-Control");

        if (cc.empty())
          res.header("Cache-Control", opt.cache_control);
      }

      const std::string inm = request_header_icase(ctx.req(), "If-None-Match");

      if (!inm.empty() && inm == tag)
      {
        res.status(304);
        res.text("");
        res.header("ETag", tag);
      }
    };
  }

} // namespace vix::middleware::performance

#endif // VIX_ETAG_HPP
