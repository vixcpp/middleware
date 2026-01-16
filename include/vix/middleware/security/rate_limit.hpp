/**
 *
 *  @file rate_limit.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_RATE_LIMIT_HPP
#define VIX_RATE_LIMIT_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/utils/token_bucket.hpp>

namespace vix::middleware::security
{
  struct RateLimitOptions
  {
    double capacity{60.0};
    double refill_per_sec{1.0}; // 60/min => 1/sec
    bool add_headers{true};
    std::string key_header{"x-forwarded-for"}; // by default: IP
    std::function<std::string(const vix::middleware::Request &)> key_fn{};
  };

  struct RateLimiterState
  {
    std::mutex mu;
    std::unordered_map<std::string, std::unique_ptr<vix::middleware::utils::TokenBucket>> buckets;
  };

  inline std::string trim_copy(std::string s)
  {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
      s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
      s.pop_back();
    return s;
  }

  inline std::string first_csv_token(std::string s)
  {
    auto comma = s.find(',');
    if (comma != std::string::npos)
      s = s.substr(0, comma);
    return trim_copy(std::move(s));
  }

  inline std::string default_key_from_req(const vix::middleware::Request &req, const RateLimitOptions &opt)
  {
    // try configured header (x-forwarded-for by default)
    std::string k = req.header(opt.key_header);
    k = first_csv_token(std::move(k));
    if (!k.empty())
      return k;

    // fallback: x-real-ip
    k = trim_copy(req.header("x-real-ip"));
    if (!k.empty())
      return k;

    return "anonymous";
  }

  inline MiddlewareFn rate_limit(RateLimitOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next) mutable
    {
      static RateLimiterState fallback_global{};
      RateLimiterState *st = nullptr;

      // Services::get<T>() returns shared_ptr<T>
      if (auto svc = ctx.services().get<RateLimiterState>())
        st = svc.get();
      else
        st = &fallback_global;

      const std::string key = opt.key_fn
                                  ? opt.key_fn(ctx.req())
                                  : default_key_from_req(ctx.req(), opt);

      // Get or create bucket (thread-safe)
      vix::middleware::utils::TokenBucket *bucket_ptr = nullptr;

      {
        std::lock_guard<std::mutex> lock(st->mu);

        auto it = st->buckets.find(key);
        if (it == st->buckets.end())
        {
          auto ptr = std::make_unique<vix::middleware::utils::TokenBucket>(opt.capacity, opt.refill_per_sec);
          it = st->buckets.emplace(key, std::move(ptr)).first;
        }

        bucket_ptr = it->second.get();
      }

      // Consume 1 token
      const bool ok = bucket_ptr->try_consume(1.0);

      if (!ok)
      {
        // Retry-After (seconds, ceil)
        const auto ms = bucket_ptr->retry_after_ms(1.0);
        const auto sec = (ms + 999) / 1000;

        if (opt.add_headers)
        {
          ctx.res().header("X-RateLimit-Limit", std::to_string(static_cast<long long>(opt.capacity)));
          ctx.res().header("X-RateLimit-Remaining", "0");
          ctx.res().header("Retry-After", std::to_string(sec));
          ctx.res().header("X-RateLimit-Reset", std::to_string(sec)); // best-effort
        }
        else
        {
          ctx.res().header("Retry-After", std::to_string(sec));
        }

        Error e;
        e.status = 429;
        e.code = "rate_limited";
        e.message = "Too many requests";
        e.details["key"] = key;

        ctx.send_error(normalize(std::move(e)));
        return;
      }

      if (opt.add_headers)
      {
        ctx.res().header("X-RateLimit-Limit", std::to_string(static_cast<long long>(opt.capacity)));
        ctx.res().header("X-RateLimit-Remaining", std::to_string(static_cast<long long>(bucket_ptr->tokens())));
      }

      next();
    };
  }

} // namespace vix::middleware::security

#endif
