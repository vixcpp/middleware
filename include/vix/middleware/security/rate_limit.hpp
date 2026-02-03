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
  /**
   * @brief Configuration options for rate_limit() middleware.
   */
  struct RateLimitOptions
  {
    /**
     * @brief Token bucket capacity (max burst).
     *
     * Default: 60 tokens.
     */
    double capacity{60.0};

    /**
     * @brief Refill rate (tokens per second).
     *
     * Default: 1 token/sec (60 per minute).
     */
    double refill_per_sec{1.0}; // 60/min => 1/sec

    /**
     * @brief If true, add informational rate limit headers.
     */
    bool add_headers{true};

    /**
     * @brief Header used to derive a default client key.
     *
     * Default: "x-forwarded-for" (common behind proxies/load balancers).
     */
    std::string key_header{"x-forwarded-for"}; // by default: IP

    /**
     * @brief Optional custom key function.
     *
     * If provided, this overrides key_header extraction logic.
     */
    std::function<std::string(const vix::middleware::Request &)> key_fn{};
  };

  /**
   * @brief Shared rate limiter state storing per-key token buckets.
   *
   * You can install this into Services to share rate limit buckets across
   * the whole server, or rely on the fallback static state used by the middleware.
   */
  struct RateLimiterState
  {
    /**
     * @brief Mutex protecting the buckets map.
     */
    std::mutex mu;

    /**
     * @brief Map: client key -> TokenBucket.
     */
    std::unordered_map<std::string, std::unique_ptr<vix::middleware::utils::TokenBucket>> buckets;
  };

  /**
   * @brief Trim ASCII whitespace from both ends of a string.
   *
   * @param s Input string.
   * @return Trimmed copy.
   */
  inline std::string trim_copy(std::string s)
  {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
      s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
      s.pop_back();
    return s;
  }

  /**
   * @brief Return the first token from a comma-separated header value.
   *
   * This is commonly used for X-Forwarded-For where the first entry is the
   * original client IP.
   *
   * @param s Input string.
   * @return First CSV token trimmed.
   */
  inline std::string first_csv_token(std::string s)
  {
    auto comma = s.find(',');
    if (comma != std::string::npos)
      s = s.substr(0, comma);
    return trim_copy(std::move(s));
  }

  /**
   * @brief Derive a default rate limit key from the request.
   *
   * Order:
   * 1) configured header (key_header, default: x-forwarded-for), first CSV token
   * 2) x-real-ip
   * 3) "anonymous"
   *
   * @param req Request.
   * @param opt Options.
   * @return Derived key.
   */
  inline std::string default_key_from_req(const vix::middleware::Request &req, const RateLimitOptions &opt)
  {
    std::string k = req.header(opt.key_header);
    k = first_csv_token(std::move(k));
    if (!k.empty())
      return k;

    k = trim_copy(req.header("x-real-ip"));
    if (!k.empty())
      return k;

    return "anonymous";
  }

  /**
   * @brief Rate limiting middleware based on a per-key token bucket.
   *
   * Behavior:
   * - Resolves a RateLimiterState from Services if available, otherwise uses a
   *   fallback global static state.
   * - Derives a key using key_fn (if provided) or default_key_from_req().
   * - Gets/creates a TokenBucket for the key.
   * - Tries to consume 1 token:
   *   - If denied: returns a normalized 429 error and sets Retry-After headers.
   *   - If allowed: optionally sets X-RateLimit-* headers and calls next().
   *
   * Headers (when add_headers is true):
   * - X-RateLimit-Limit
   * - X-RateLimit-Remaining
   * - Retry-After (on rejection)
   * - X-RateLimit-Reset (best-effort, seconds until 1 token becomes available)
   *
   * @param opt Rate limit options.
   * @return A middleware function (MiddlewareFn).
   */
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

      const bool ok = bucket_ptr->try_consume(1.0);

      if (!ok)
      {
        const auto ms = bucket_ptr->retry_after_ms(1.0);
        const auto sec = (ms + 999) / 1000;

        if (opt.add_headers)
        {
          ctx.res().header("X-RateLimit-Limit", std::to_string(static_cast<long long>(opt.capacity)));
          ctx.res().header("X-RateLimit-Remaining", "0");
          ctx.res().header("Retry-After", std::to_string(sec));
          ctx.res().header("X-RateLimit-Reset", std::to_string(sec));
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

#endif // VIX_RATE_LIMIT_HPP
