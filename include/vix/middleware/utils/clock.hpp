/**
 *
 *  @file clock.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_CLOCK_HPP
#define VIX_CLOCK_HPP

#include <chrono>
#include <cstdint>

namespace vix::middleware::utils
{
  /**
   * @brief Small time utility wrapper used by middleware components.
   *
   * This helper centralizes common clock usage and conversions:
   * - steady clock for durations and measurements
   * - system clock for epoch-based timestamps
   *
   * All values are returned as signed 64-bit integers for easy storage
   * and serialization.
   */
  struct Clock final
  {
    /**
     * @brief Alias for the steady clock.
     *
     * Monotonic clock suitable for measuring elapsed time.
     */
    using Steady = std::chrono::steady_clock;

    /**
     * @brief Alias for the system clock.
     *
     * Represents wall-clock time since Unix epoch.
     */
    using System = std::chrono::system_clock;

    /**
     * @brief Current steady-clock time in milliseconds.
     *
     * The returned value is the duration since the steady clock epoch.
     * It is monotonic and must not be interpreted as wall-clock time.
     *
     * @return Milliseconds since steady clock epoch.
     */
    static std::int64_t now_ms_steady()
    {
      using namespace std::chrono;
      return duration_cast<milliseconds>(Steady::now().time_since_epoch()).count();
    }

    /**
     * @brief Current system-clock time in milliseconds since Unix epoch.
     *
     * This value is suitable for logging, timestamps, and serialization,
     * but may be affected by system clock adjustments.
     *
     * @return Milliseconds since Unix epoch.
     */
    static std::int64_t now_ms_epoch()
    {
      using namespace std::chrono;
      return duration_cast<milliseconds>(System::now().time_since_epoch()).count();
    }

    /**
     * @brief Convert a steady-clock duration to milliseconds.
     *
     * @param d Duration measured with steady_clock.
     * @return Duration in milliseconds.
     */
    static std::int64_t to_ms(Steady::duration d)
    {
      using namespace std::chrono;
      return duration_cast<milliseconds>(d).count();
    }

    /**
     * @brief Convert a steady-clock duration to microseconds.
     *
     * @param d Duration measured with steady_clock.
     * @return Duration in microseconds.
     */
    static std::int64_t to_us(Steady::duration d)
    {
      using namespace std::chrono;
      return duration_cast<microseconds>(d).count();
    }
  };

} // namespace vix::middleware::utils

#endif // VIX_CLOCK_HPP
