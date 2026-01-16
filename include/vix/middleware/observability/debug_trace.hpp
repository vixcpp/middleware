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

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/observability/utils.hpp>

namespace vix::middleware::observability
{
  struct TraceContext;

  class IDebugTraceSink
  {
  public:
    virtual ~IDebugTraceSink() = default;
    virtual void log(std::string_view line) = 0;
  };

  class InMemoryDebugTrace final : public IDebugTraceSink
  {
  public:
    void log(std::string_view line) override
    {
      lines.emplace_back(line);
    }

    std::vector<std::string> lines;
  };

  struct DebugTraceStart
  {
    std::chrono::steady_clock::time_point t0{};
  };

  struct DebugTraceOptions
  {
    bool include_method{true};
    bool include_path{true};
    bool include_status{true};
    bool include_duration_ms{true};
    bool include_trace_ids{true};
    std::string prefix{"[vix.debug]"};
  };

  inline std::string build_line_begin(vix::middleware::Context &ctx, const DebugTraceOptions &opt)
  {
    (void)ctx;
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
    }

    return s;
  }

  inline std::string build_line_end(
      vix::middleware::Context &ctx,
      const DebugTraceOptions &opt,
      double ms)
  {
    (void)ctx;
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

  inline vix::middleware::Hooks debug_trace_hooks(
      std::shared_ptr<IDebugTraceSink> sink,
      DebugTraceOptions opt = {})
  {
    vix::middleware::Hooks h;

    h.on_begin = [sink, opt = std::move(opt)](vix::middleware::Context &ctx) mutable
    {
      (void)ctx;
      if (!sink)
        return;

      ctx.set_state(DebugTraceStart{std::chrono::steady_clock::now()});
      sink->log(build_line_begin(ctx, opt));
    };

    h.on_end = [sink, opt = std::move(opt)](vix::middleware::Context &ctx) mutable
    {
      (void)ctx;
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
      s += " code=" + err.code;
      s += " status=" + std::to_string(err.status);
      sink->log(s);
    };

    return h;
  }

  inline vix::middleware::MiddlewareFn debug_trace_mw(
      std::shared_ptr<IDebugTraceSink> sink,
      DebugTraceOptions opt = {})
  {
    return [sink = std::move(sink), opt = std::move(opt)](
               vix::middleware::Context &ctx,
               vix::middleware::Next next) mutable
    {
      (void)ctx;
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

#endif
