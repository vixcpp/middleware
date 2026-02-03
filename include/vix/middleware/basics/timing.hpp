/**
 *
 *  @file timing.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_TIMING_HPP
#define VIX_TIMING_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/basics/detail/ascii.hpp>

namespace vix::middleware::basics
{
  /**
   * @brief Typed request state storing request duration.
   *
   * The value represents the total processing time of the request
   * in milliseconds.
   */
  struct Timing final
  {
    std::int64_t total_ms{0};
  };

  /**
   * @brief Configuration options for timing() middleware.
   */
  struct TimingOptions final
  {
    /**
     * @brief If true, set the X-Response-Time header.
     */
    bool set_x_response_time{true};

    /**
     * @brief If true, set the Server-Timing header.
     */
    bool set_server_timing{true};

    /**
     * @brief Header name used for X-Response-Time.
     *
     * The name is normalized to lowercase ASCII internally.
     */
    std::string x_response_time_header{"x-response-time"};

    /**
     * @brief Header name used for Server-Timing.
     *
     * The name is normalized to lowercase ASCII internally.
     */
    std::string server_timing_header{"server-timing"};

    /**
     * @brief Server-Timing metric name.
     *
     * Example output: "total;dur=12"
     */
    std::string server_timing_metric{"total"};

    /**
     * @brief If true, store Timing{ms} in typed request state.
     */
    bool store_in_state{true};
  };

  /**
   * @brief Request timing middleware.
   *
   * Measures the wall-clock time spent executing downstream middleware
   * and handlers.
   *
   * Behavior:
   * - Records start time before calling next().
   * - Records end time after next() returns.
   * - Optionally stores the duration in typed request state (Timing).
   * - Optionally sets response headers:
   *   - X-Response-Time: "<ms>ms"
   *   - Server-Timing: "<metric>;dur=<ms>"
   *
   * @param opt Middleware options.
   * @return A middleware function (MiddlewareFn).
   */
  inline MiddlewareFn timing(TimingOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next)
    {
      using Clock = std::chrono::steady_clock;
      const auto t0 = Clock::now();

      next();

      const auto t1 = Clock::now();
      const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

      // Store in typed request state
      if (opt.store_in_state)
      {
        ctx.set_state(Timing{static_cast<std::int64_t>(ms)});
      }

      // X-Response-Time header
      if (opt.set_x_response_time && !opt.x_response_time_header.empty())
      {
        const std::string h =
            vix::middleware::basics::detail::to_lower_ascii(opt.x_response_time_header);
        ctx.res().header(h, std::to_string(ms) + "ms");
      }

      // Server-Timing header
      if (opt.set_server_timing && !opt.server_timing_header.empty())
      {
        const std::string h =
            vix::middleware::basics::detail::to_lower_ascii(opt.server_timing_header);

        std::string value;
        value.reserve(64);
        value += opt.server_timing_metric.empty() ? "total" : opt.server_timing_metric;
        value += ";dur=";
        value += std::to_string(ms);
        ctx.res().header(h, value);
      }
    };
  }

} // namespace vix::middleware::basics

#endif // VIX_TIMING_HPP
