/**
 *
 *  @file body_limit.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef BODY_LIMIT_HPP
#define BODY_LIMIT_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::basics
{
  /**
   * @brief Options for the request body size limiter middleware.
   */
  struct BodyLimitOptions
  {
    std::size_t max_bytes{1 * 1024 * 1024}; // 1 MiB default
    bool apply_to_get{false};               // usually false: GET has no body
    bool allow_chunked{true};               // if Content-Length missing, allow by default

    // Return true => enforce limit, false => skip
    std::function<bool(const vix::middleware::Context &)> should_apply{};
  };

  inline bool iequals_ascii(std::string_view a, std::string_view b)
  {
    if (a.size() != b.size())
      return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
      auto ca = static_cast<unsigned char>(a[i]);
      auto cb = static_cast<unsigned char>(b[i]);
      if (ca >= 'A' && ca <= 'Z')
        ca = static_cast<unsigned char>(ca - 'A' + 'a');
      if (cb >= 'A' && cb <= 'Z')
        cb = static_cast<unsigned char>(cb - 'A' + 'a');
      if (ca != cb)
        return false;
    }
    return true;
  }

  inline bool method_is_get(const vix::middleware::Request &req)
  {
    return iequals_ascii(req.method(), "GET");
  }

  inline std::size_t parse_content_length(std::string_view v, bool &ok)
  {
    ok = false;
    if (v.empty())
      return 0;

    std::size_t out = 0;
    for (char c : v)
    {
      if (c < '0' || c > '9')
        return 0;

      const std::size_t digit = static_cast<std::size_t>(c - '0');

      // overflow-safe: out = out*10 + digit
      if (out > (static_cast<std::size_t>(-1) / 10))
        return 0;
      out *= 10;
      if (out > (static_cast<std::size_t>(-1) - digit))
        return 0;
      out += digit;
    }

    ok = true;
    return out;
  }

  /**
   * @brief Enforce a maximum request body size.
   *
   * Uses the in-memory body size when already available, otherwise falls back
   * to Content-Length (if present). If Content-Length is missing, behavior
   * depends on allow_chunked.
   */
  inline vix::middleware::MiddlewareFn body_limit(BodyLimitOptions opt = {})
  {
    return [opt = std::move(opt)](vix::middleware::Context &ctx, vix::middleware::Next next) mutable
    {
      auto &req = ctx.req();

      if (opt.should_apply)
      {
        if (!opt.should_apply(ctx))
        {
          next();
          return;
        }
      }
      else
      {
        // Default behavior:
        // - skip GET unless explicitly enabled
        if (method_is_get(req) && !opt.apply_to_get)
        {
          next();
          return;
        }
      }

      const std::size_t body_size = req.body().size();
      if (body_size > 0)
      {
        if (body_size > opt.max_bytes)
        {
          Error err;
          err.status = 413;
          err.code = "payload_too_large";
          err.message = "Request body exceeds the configured limit";
          err.details["max_bytes"] = std::to_string(opt.max_bytes);
          err.details["got_bytes"] = std::to_string(body_size);
          ctx.send_error(normalize(std::move(err)));
          return;
        }

        next();
        return;
      }

      const std::string cl = req.header("content-length");
      if (!cl.empty())
      {
        bool ok = false;
        const std::size_t len = parse_content_length(cl, ok);
        if (ok && len > opt.max_bytes)
        {
          Error err;
          err.status = 413;
          err.code = "payload_too_large";
          err.message = "Content-Length exceeds the configured limit";
          err.details["max_bytes"] = std::to_string(opt.max_bytes);
          err.details["content_length"] = std::to_string(len);
          ctx.send_error(normalize(std::move(err)));
          return;
        }

        next();
        return;
      }

      // No Content-Length:
      // - if allow_chunked => let it pass (server might stream/decode)
      // - else => reject (strict mode)
      if (!opt.allow_chunked)
      {
        Error err;
        err.status = 411; // Length Required
        err.code = "length_required";
        err.message = "Content-Length is required by this server policy";
        ctx.send_error(normalize(std::move(err)));
        return;
      }

      next();
    };
  }

} // namespace vix::middleware::basics

#endif // BODY_LIMIT_HPP
