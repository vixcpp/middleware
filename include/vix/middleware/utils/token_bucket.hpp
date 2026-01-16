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
  class TokenBucket final
  {
  public:
    TokenBucket() = default;

    TokenBucket(double capacity, double refill_per_sec)
        : capacity_(std::max(0.0, capacity)),
          refill_per_sec_(std::max(0.0, refill_per_sec)),
          tokens_(capacity_),
          last_ms_(Clock::now_ms_steady())
    {
    }

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

    double tokens() const
    {
      std::lock_guard<std::mutex> lock(mu_);
      return tokens_;
    }

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

#endif
