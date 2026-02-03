/**
 *
 *  @file api_key.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_API_KEY_HPP
#define VIX_API_KEY_HPP

#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::auth
{
  /**
   * @brief API key state stored in request context.
   */
  struct ApiKey
  {
    std::string value;
  };

  /**
   * @brief API key authentication options.
   */
  struct ApiKeyOptions
  {
    std::string header{"x-api-key"};
    std::string query_param{};
    bool required{true};

    std::unordered_set<std::string> allowed_keys{};

    std::function<std::string(const vix::middleware::Request &)> extract{};
    std::function<bool(const std::string &)> validate{};
  };

  /**
   * @brief Extract API key from header or query parameter.
   */
  inline std::string extract_from_header_or_query(
      const vix::middleware::Request &req,
      const ApiKeyOptions &opt)
  {
    if (!opt.header.empty())
    {
      auto v = req.header(opt.header);
      if (!v.empty())
        return v;
    }

    if (!opt.query_param.empty())
    {
      auto it = req.query().find(opt.query_param);
      if (it != req.query().end())
        return it->second;
    }

    return {};
  }

  /**
   * @brief API key authentication middleware.
   */
  inline MiddlewareFn api_key(ApiKeyOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      std::string key = opt.extract
                            ? opt.extract(ctx.req())
                            : extract_from_header_or_query(ctx.req(), opt);

      if (key.empty())
      {
        if (!opt.required)
        {
          next();
          return;
        }

        Error e;
        e.status = 401;
        e.code = "missing_api_key";
        e.message = "API key is required";
        ctx.send_error(normalize(std::move(e)));
        return;
      }

      bool ok = true;

      if (!opt.allowed_keys.empty())
        ok = opt.allowed_keys.count(key) != 0;

      if (ok && opt.validate)
        ok = opt.validate(key);

      if (!ok)
      {
        Error e;
        e.status = 403;
        e.code = "invalid_api_key";
        e.message = "Invalid API key";
        ctx.send_error(normalize(std::move(e)));
        return;
      }

      ctx.emplace_state<ApiKey>(ApiKey{key});
      next();
    };
  }

} // namespace vix::middleware::auth

#endif // VIX_API_KEY_HPP
