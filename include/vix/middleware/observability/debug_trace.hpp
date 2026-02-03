/**
 *
 *  @file debug_trace.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_DEBUG_TRACE_HPP
#define VIX_DEBUG_TRACE_HPP

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/observability/utils.hpp>

namespace vix::middleware::observability
{
  struct TraceContext;

  /**
   * @brief Debug trace sink interface.
   *
   * A debug trace sink receives pre-formatted log lines produced by
   * debug_trace_hooks() or debug_trace_mw().
   */
  class IDebugTraceSink
  {
  public:
    virtual ~IDebugTraceSink() = default;

    /**
     * @brief Consume a single debug trace line.
     *
     * @param line Formatted line.
     */
    virtual void log(std::string_view line) = 0;
  };

  /**
   * @brief In-memory debug trace sink.
   *
   * Useful for tests and local inspection.
   */
  class InMemoryDebugTrace final : public IDebugTraceSink
  {
  public:
    /**
     * @brief Append a trace line to the internal buffer.
     *
     * @param line Formatted line.
     */
    void log(std::string_view line) override
    {
      lines.emplace_back(line);
    }

    /**
     * @brief Collected trace lines.
     */
    std::vector<std::string> lines;
  };

  /**
   * @brief Typed request state storing start time for debug tracing.
   */
  struct DebugTraceStart
  {
    std::chrono::steady_clock::time_point t0{};
  };

  /**
   * @brief Configuration options for debug tracing.
   */
  struct DebugTraceOptions
  {
    /**
     * @brief Include HTTP method in logs.
     */
    bool include_method{true};

    /**
     * @brief Include request path in logs.
     */
    bool include_path{true};

    /**
     * @brief Include HTTP status in end logs.
     */
    bool include_status{true};

    /**
     * @brief Include duration (milliseconds) in end logs.
     */
    bool include_duration_ms{true};

    /**
     * @brief Include trace ids (reserved for future integration).
     */
    bool include_trace_ids{true};

    /**
     * @brief Prefix prepended to each trace line.
     */
    std::string prefix{"[vix.debug]"};
  };

  /**
   * @brief Build the "begin" trace line.
   *
   * @param ctx Request context.
   * @param opt Trace options.
   * @return Formatted line.
   */
  inline std::string build_line_begin(vix::middleware::Context &ctx, const DebugTraceOptions &opt)
  {
    std::string s;
    s.reserve(256);
    s += opt.prefix;
    s += " begin";

    if (opt.include_method)
    {
      s += " method=";
      s += safe_method(ctx.req());
    }

    if (opt.include_path)
    {
      s += " path=";
      s += safe_path(ctx.req());
    }

    if (opt.include_trace_ids)
    {
      // Reserved: integrate trace/span ids when TraceContext is wired.
    }

    return s;
  }

  /**
   * @brief Build the "end" trace line.
   *
   * @param ctx Request context.
   * @param opt Trace options.
   * @param ms Elapsed time in milliseconds.
   * @return Formatted line.
   */
  inline std::string build_line_end(
      vix::middleware::Context &ctx,
      const DebugTraceOptions &opt,
      double ms)
  {
    std::string s;
    s.reserve(256);
    s += opt.prefix;
    s += " end";

    if (opt.include_method)
    {
      s += " method=";
      s += safe_method(ctx.req());
    }

    if (opt.include_path)
    {
      s += " path=";
      s += safe_path(ctx.req());
    }

    if (opt.include_status)
    {
      const int sc = static_cast<int>(ctx.res().res.result_int());
      s += " status=";
      s += std::to_string(sc);
    }

    if (opt.include_duration_ms)
    {
      s += " ms=";
      s += std::to_string(ms);
    }

    return s;
  }

  /**
   * @brief Create hooks that emit begin/end/error debug trace lines.
   *
   * Behavior:
   * - on_begin: stores start time in DebugTraceStart and logs "begin"
   * - on_end: computes elapsed time (if start time exists) and logs "end"
   * - on_error: logs an "error" line with error code/status
   *
   * @param sink Debug trace sink (shared ownership).
   * @param opt Trace options.
   * @return Hooks configured for debug tracing.
   */
  inline vix::middleware::Hooks debug_trace_hooks(
      std::shared_ptr<IDebugTraceSink> sink,
      DebugTraceOptions opt = {})
  {
    vix::middleware::Hooks h;

    h.on_begin = [sink, opt = std::move(opt)](vix::middleware::Context &ctx) mutable
    {
      if (!sink)
        return;

      ctx.set_state(DebugTraceStart{std::chrono::steady_clock::now()});
      sink->log(build_line_begin(ctx, opt));
    };

    h.on_end = [sink, opt = std::move(opt)](vix::middleware::Context &ctx) mutable
    {
      if (!sink)
        return;

      auto *st = ctx.try_state<DebugTraceStart>();
      const auto t1 = std::chrono::steady_clock::now();
      double ms = 0.0;

      if (st)
        ms = std::chrono::duration<double, std::milli>(t1 - st->t0).count();

      sink->log(build_line_end(ctx, opt, ms));
    };

    h.on_error = [sink, opt = std::move(opt)](
                     vix::middleware::Context &ctx,
                     const vix::middleware::Error &err) mutable
    {
      (void)ctx;
      if (!sink)
        return;

      std::string s;
      s.reserve(256);
      s += opt.prefix;
      s += " error";
      s += " code=";
      s += err.code;
      s += " status=";
      s += std::to_string(err.status);
      sink->log(s);
    };

    return h;
  }

  /**
   * @brief Middleware variant that emits begin/end debug trace lines.
   *
   * This variant does not hook into a dedicated error path; it simply
   * logs begin/end around next().
   *
   * @param sink Debug trace sink (shared ownership).
   * @param opt Trace options.
   * @return A middleware function (MiddlewareFn).
   */
  inline vix::middleware::MiddlewareFn debug_trace_mw(
      std::shared_ptr<IDebugTraceSink> sink,
      DebugTraceOptions opt = {})
  {
    return [sink = std::move(sink), opt = std::move(opt)](
               vix::middleware::Context &ctx,
               vix::middleware::Next next) mutable
    {
      if (!sink)
      {
        next();
        return;
      }

      const auto t0 = std::chrono::steady_clock::now();
      sink->log(build_line_begin(ctx, opt));

      next();

      const auto t1 = std::chrono::steady_clock::now();
      const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      sink->log(build_line_end(ctx, opt, ms));
    };
  }

} // namespace vix::middleware::observability

#endif // VIX_DEBUG_TRACE_HPP
