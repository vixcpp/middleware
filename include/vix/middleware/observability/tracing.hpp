/**
 *
 *  @file tracing.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_TRACING_HPP
#define VIX_TRACING_HPP

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::observability
{
  /**
   * @brief Typed request state storing trace/span identifiers.
   *
   * The context can be set by tracing_hooks() or tracing_mw() and then accessed
   * by downstream middleware/handlers via ctx.try_state<TraceContext>().
   */
  struct TraceContext
  {
    /**
     * @brief Trace identifier.
     *
     * Generated as either 16 or 32 hex characters when accepted from incoming
     * headers, and generated as 32 hex characters (128-bit) when created locally.
     */
    std::string trace_id{};

    /**
     * @brief Current span identifier (16 hex characters).
     */
    std::string span_id{};

    /**
     * @brief Parent span identifier (16 hex characters) when an incoming span is accepted.
     */
    std::string parent_span_id{};
  };

  /**
   * @brief Configuration options for tracing.
   */
  struct TracingOptions
  {
    /**
     * @brief Request/response header used for trace id.
     */
    std::string trace_header{"x-trace-id"};

    /**
     * @brief Request/response header used for span id.
     */
    std::string span_header{"x-span-id"};

    /**
     * @brief Request/response header used for parent span id.
     */
    std::string parent_span_header{"x-parent-span-id"};

    /**
     * @brief If true, accept incoming trace id from trace_header.
     */
    bool accept_incoming_trace{true};

    /**
     * @brief If true, accept incoming span id from span_header as parent_span_id.
     */
    bool accept_incoming_span{true};

    /**
     * @brief If true, emit trace/span response headers.
     */
    bool emit_response_headers{true};

    /**
     * @brief If true, include parent_span_id in the response when present.
     */
    bool include_parent_in_response{false};

    /**
     * @brief Optional callback to enrich TraceContext (e.g. attach tenant/user ids elsewhere).
     */
    std::function<void(vix::middleware::Context &, TraceContext &)> enrich{};
  };

  /**
   * @brief Convert an unsigned 64-bit integer to a 16-char lowercase hex string.
   *
   * @param v Input value.
   * @return 16 hex characters (lowercase).
   */
  inline std::string hex_u64(std::uint64_t v)
  {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(16);
    for (int i = 15; i >= 0; --i)
    {
      out[static_cast<std::size_t>(i)] = kHex[v & 0xF];
      v >>= 4;
    }
    return out;
  }

  /**
   * @brief Thread-local random 64-bit generator.
   *
   * @return Random 64-bit value.
   */
  inline std::uint64_t rng_u64()
  {
    thread_local std::mt19937_64 rng{std::random_device{}()};
    return rng();
  }

  /**
   * @brief Generate a new trace id (128-bit hex string, 32 chars).
   */
  inline std::string new_trace_id()
  {
    const std::uint64_t a = rng_u64();
    const std::uint64_t b = rng_u64();
    return hex_u64(a) + hex_u64(b);
  }

  /**
   * @brief Generate a new span id (64-bit hex string, 16 chars).
   */
  inline std::string new_span_id()
  {
    return hex_u64(rng_u64());
  }

  /**
   * @brief Check whether a string contains only hex characters.
   *
   * @param s Candidate string.
   * @return true if all characters are [0-9a-fA-F] and the string is non-empty.
   */
  inline bool looks_like_hex(std::string_view s)
  {
    if (s.empty())
      return false;

    for (char c : s)
    {
      const bool ok =
          (c >= '0' && c <= '9') ||
          (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F');

      if (!ok)
        return false;
    }
    return true;
  }

  /**
   * @brief Build a TraceContext from incoming headers and options.
   *
   * Rules:
   * - If accept_incoming_trace is true, accept a hex trace id from trace_header
   *   with length 16 or 32.
   * - If accept_incoming_span is true, accept a hex span id from span_header
   *   with length 16 and store it as parent_span_id.
   * - If no trace_id was accepted, generate a new trace_id (32 hex chars).
   * - Always generate a new span_id (16 hex chars).
   *
   * @param ctx Request context.
   * @param opt Tracing options.
   * @return Built TraceContext.
   */
  inline TraceContext build_trace_ctx(vix::middleware::Context &ctx, const TracingOptions &opt)
  {
    TraceContext tc{};

    if (opt.accept_incoming_trace)
    {
      const std::string incoming = ctx.req().header(opt.trace_header);
      if (!incoming.empty() && looks_like_hex(incoming) && (incoming.size() == 16 || incoming.size() == 32))
        tc.trace_id = incoming;
    }

    if (opt.accept_incoming_span)
    {
      const std::string incoming_span = ctx.req().header(opt.span_header);
      if (!incoming_span.empty() && looks_like_hex(incoming_span) && incoming_span.size() == 16)
        tc.parent_span_id = incoming_span;
    }

    if (tc.trace_id.empty())
      tc.trace_id = new_trace_id();

    tc.span_id = new_span_id();
    return tc;
  }

  /**
   * @brief Emit trace headers to the response.
   *
   * If emit_response_headers is false, this does nothing.
   *
   * @param ctx Request context.
   * @param tc Trace context.
   * @param opt Tracing options.
   */
  inline void emit_headers(vix::middleware::Context &ctx, const TraceContext &tc, const TracingOptions &opt)
  {
    if (!opt.emit_response_headers)
      return;

    ctx.res().header(opt.trace_header, tc.trace_id);
    ctx.res().header(opt.span_header, tc.span_id);

    if (opt.include_parent_in_response && !tc.parent_span_id.empty())
      ctx.res().header(opt.parent_span_header, tc.parent_span_id);
  }

  /**
   * @brief Create hooks that install tracing state and emit headers.
   *
   * Behavior:
   * - on_begin: builds TraceContext, calls enrich (optional), stores it in state, emits headers
   * - on_end: re-emits headers (useful if response headers were reset downstream)
   * - on_error: re-emits headers (ensures trace ids are present on error responses)
   *
   * @param opt Tracing options.
   * @return Hooks configured for tracing.
   */
  inline vix::middleware::Hooks tracing_hooks(TracingOptions opt = {})
  {
    vix::middleware::Hooks h;

    h.on_begin = [opt = std::move(opt)](vix::middleware::Context &ctx) mutable
    {
      TraceContext tc = build_trace_ctx(ctx, opt);

      if (opt.enrich)
        opt.enrich(ctx, tc);

      ctx.set_state(tc);
      emit_headers(ctx, tc, opt);
    };

    h.on_end = [opt = std::move(opt)](vix::middleware::Context &ctx) mutable
    {
      auto *tc = ctx.try_state<TraceContext>();
      if (!tc)
        return;
      emit_headers(ctx, *tc, opt);
    };

    h.on_error = [opt = std::move(opt)](
                     vix::middleware::Context &ctx,
                     const vix::middleware::Error &) mutable
    {
      auto *tc = ctx.try_state<TraceContext>();
      if (!tc)
        return;
      emit_headers(ctx, *tc, opt);
    };

    return h;
  }

  /**
   * @brief Middleware variant that installs tracing state and emits headers.
   *
   * Behavior:
   * - Builds TraceContext, calls enrich (optional), stores it in state, emits headers
   * - Calls next()
   * - Re-emits headers after next() returns
   *
   * @param opt Tracing options.
   * @return A middleware function (MiddlewareFn).
   */
  inline vix::middleware::MiddlewareFn tracing_mw(TracingOptions opt = {})
  {
    return [opt = std::move(opt)](vix::middleware::Context &ctx, vix::middleware::Next next) mutable
    {
      TraceContext tc = build_trace_ctx(ctx, opt);

      if (opt.enrich)
        opt.enrich(ctx, tc);

      ctx.set_state(tc);
      emit_headers(ctx, tc, opt);

      next();

      emit_headers(ctx, tc, opt);
    };
  }

} // namespace vix::middleware::observability

#endif // VIX_TRACING_HPP
