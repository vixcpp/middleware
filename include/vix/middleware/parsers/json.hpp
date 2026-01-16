/**
 *
 *  @file json.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_MIDDLEWARE_JSON_HPP
#define VIX_MIDDLEWARE_JSON_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include <vix/middleware/middleware.hpp>
#include <vix/utils/String.hpp>

namespace vix::middleware::parsers
{
  struct JsonBody
  {
    nlohmann::json value{};
  };

  struct JsonParserOptions
  {
    bool require_content_type{true};
    bool allow_empty{true};
    std::size_t max_bytes{0};  // 0 => no limit (body_limit middleware can handle globally)
    bool store_in_state{true}; // store JsonBody in ctx.state
  };

  inline MiddlewareFn json(JsonParserOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      auto &req = ctx.req();

      const std::string body = req.body();
      if (body.empty())
      {
        if (!opt.allow_empty)
        {
          Error e;
          e.status = 400;
          e.code = "empty_body";
          e.message = "JSON body is required";
          ctx.send_error(normalize(std::move(e)));
          return;
        }

        // allow empty => store {} optionally
        if (opt.store_in_state)
        {
          JsonBody jb;
          jb.value = nlohmann::json::object();
          ctx.set_state<JsonBody>(std::move(jb));
        }
        next();
        return;
      }

      if (opt.max_bytes > 0 && body.size() > opt.max_bytes)
      {
        Error e;
        e.status = 413;
        e.code = "payload_too_large";
        e.message = "Request body exceeds JSON parser limit";
        e.details["max_bytes"] = std::to_string(opt.max_bytes);
        e.details["got_bytes"] = std::to_string(body.size());
        ctx.send_error(normalize(std::move(e)));
        return;
      }

      if (opt.require_content_type)
      {
        const std::string ct = req.header("content-type");
        // accept "application/json" and "application/json; charset=utf-8"
        if (ct.empty() || !vix::utils::starts_with_icase(ct, "application/json"))
        {
          Error e;
          e.status = 415;
          e.code = "unsupported_media_type";
          e.message = "Content-Type must be application/json";
          if (!ct.empty())
            e.details["content_type"] = ct;
          ctx.send_error(normalize(std::move(e)));
          return;
        }
      }

      try
      {
        const nlohmann::json parsed =
            nlohmann::json::parse(body, nullptr, /*allow_exceptions*/ true, /*ignore_comments*/ true);

        if (opt.store_in_state)
        {
          JsonBody jb;
          jb.value = parsed;
          ctx.set_state<JsonBody>(std::move(jb));
        }

        next();
      }
      catch (const std::exception &e)
      {
        Error err;
        err.status = 400;
        err.code = "invalid_json";
        err.message = "Failed to parse JSON body";
        err.details["what"] = e.what();
        ctx.send_error(normalize(std::move(err)));
      }
    };
  }

} // namespace vix::middleware::parsers

#endif
