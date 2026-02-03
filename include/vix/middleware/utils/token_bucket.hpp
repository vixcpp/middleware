/**
 *
 *  @file token_bucket.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_TOKEN_BUCKET_HPP
#define VIX_TOKEN_BUCKET_HPP

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>

#include <vix/middleware/utils/clock.hpp>

namespace vix::middleware::utils
{
  /**
   * @brief Thread-safe token bucket rate limiter primitive.
   *
   * TokenBucket implements the classic token bucket algorithm:
   * - a bucket has a maximum capacity (tokens)
   * - tokens refill over time at a configured rate (tokens per second)
   * - a request consumes N tokens if available
   *
   * This is typically used by rate-limiting middleware.
   */
  class TokenBucket final
  {
  public:
    /**
     * @brief Construct an empty bucket (capacity=0, refill=0).
     *
     * This bucket will never allow consumption unless n <= 0.
     */
    TokenBucket() = default;

    /**
     * @brief Construct a token bucket.
     *
     * @param capacity Maximum token capacity (clamped to >= 0).
     * @param refill_per_sec Refill rate in tokens per second (clamped to >= 0).
     */
    TokenBucket(double capacity, double refill_per_sec)
        : capacity_(std::max(0.0, capacity)),
          refill_per_sec_(std::max(0.0, refill_per_sec)),
          tokens_(capacity_),
          last_ms_(Clock::now_ms_steady())
    {
    }

    /**
     * @brief Try to consume @p n tokens from the bucket.
     *
     * The bucket is refilled based on elapsed time before attempting consumption.
     *
     * @param n Number of tokens to consume. If n <= 0, this returns true.
     * @return true if enough tokens were available and consumed.
     */
    bool try_consume(double n)
    {
      if (n <= 0.0)
        return true;

      std::lock_guard<std::mutex> lock(mu_);
      refill_locked_();

      if (tokens_ >= n)
      {
        tokens_ -= n;
        return true;
      }
      return false;
    }

    /**
     * @brief Get the current number of tokens.
     *
     * Note: this value may be slightly stale with respect to wall time because
     * refill is applied only when methods are called.
     *
     * @return Current token count.
     */
    double tokens() const
    {
      std::lock_guard<std::mutex> lock(mu_);
      return tokens_;
    }

    /**
     * @brief Estimate how long until at least @p need tokens are available.
     *
     * The bucket is refilled before computing the estimate.
     *
     * @param need Required tokens (default: 1.0).
     * @return Milliseconds to wait (0 if already available). Minimum returned is 1
     *         when tokens are missing, to avoid returning 0 in edge cases.
     */
    std::int64_t retry_after_ms(double need = 1.0)
    {
      std::lock_guard<std::mutex> lock(mu_);
      refill_locked_();

      if (refill_per_sec_ <= 0.0)
        return 1000;

      if (tokens_ >= need)
        return 0;

      const double missing = need - tokens_;
      const double seconds = missing / refill_per_sec_;
      const auto ms = static_cast<std::int64_t>(seconds * 1000.0);
      return std::max<std::int64_t>(1, ms);
    }

  private:
    /**
     * @brief Refill tokens based on elapsed steady time.
     *
     * Caller must hold mu_.
     */
    void refill_locked_()
    {
      const auto now = Clock::now_ms_steady();
      const auto dt_ms = now - last_ms_;
      if (dt_ms <= 0)
        return;

      last_ms_ = now;

      const double dt_sec = static_cast<double>(dt_ms) / 1000.0;
      const double add = dt_sec * refill_per_sec_;
      tokens_ = std::min(capacity_, tokens_ + add);
    }

  private:
    double capacity_{0.0};
    double refill_per_sec_{0.0};

    mutable std::mutex mu_;
    double tokens_{0.0};
    std::int64_t last_ms_{0};
  };

} // namespace vix::middleware::utils

#endif // VIX_TOKEN_BUCKET_HPP
