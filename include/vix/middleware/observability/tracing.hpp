#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/core/hooks.hpp>

namespace vix::middleware::observability
{
    struct TraceContext
    {
        std::string trace_id{};
        std::string span_id{};
        std::string parent_span_id{};
    };

    struct TracingOptions
    {
        std::string trace_header{"x-trace-id"};
        std::string span_header{"x-span-id"};
        std::string parent_span_header{"x-parent-span-id"};
        bool accept_incoming_trace{true};
        bool accept_incoming_span{true};
        bool emit_response_headers{true};
        bool include_parent_in_response{false};
        std::function<void(vix::middleware::Context &, TraceContext &)> enrich{};
    };

    inline std::string hex_u64(std::uint64_t v)
    {
        static constexpr char kHex[] = "0123456789abcdef";
        std::string out;
        out.resize(16);
        for (int i = 15; i >= 0; --i)
        {
            out[i] = kHex[v & 0xF];
            v >>= 4;
        }
        return out;
    }

    inline std::uint64_t rng_u64()
    {
        thread_local std::mt19937_64 rng{std::random_device{}()};
        return rng();
    }

    inline std::string new_trace_id()
    {
        // 128-bit hex (32 chars)
        const std::uint64_t a = rng_u64();
        const std::uint64_t b = rng_u64();
        return hex_u64(a) + hex_u64(b);
    }

    inline std::string new_span_id()
    {
        // 64-bit hex (16 chars)
        return hex_u64(rng_u64());
    }

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

    inline void emit_headers(vix::middleware::Context &ctx, const TraceContext &tc, const TracingOptions &opt)
    {
        if (!opt.emit_response_headers)
            return;

        ctx.res().header(opt.trace_header, tc.trace_id);
        ctx.res().header(opt.span_header, tc.span_id);

        if (opt.include_parent_in_response && !tc.parent_span_id.empty())
            ctx.res().header(opt.parent_span_header, tc.parent_span_id);
    }

    inline vix::middleware::Hooks tracing_hooks(TracingOptions opt = {})
    {
        vix::middleware::Hooks h;

        h.on_begin = [opt = std::move(opt)](vix::middleware::Context &ctx) mutable
        {
            TraceContext tc = build_trace_ctx(ctx, opt);

            if (opt.enrich)
                opt.enrich(ctx, tc);

            ctx.set_state(std::move(tc));
        };

        h.on_end = [opt = std::move(opt)](vix::middleware::Context &ctx) mutable
        {
            auto *tc = ctx.try_state<TraceContext>();
            if (!tc)
                return;

            emit_headers(ctx, *tc, opt);
        };

        h.on_error = [opt = std::move(opt)](vix::middleware::Context &ctx, const vix::middleware::Error &) mutable
        {
            auto *tc = ctx.try_state<TraceContext>();
            if (!tc)
                return;

            emit_headers(ctx, *tc, opt);
        };

        return h;
    }

    inline vix::middleware::MiddlewareFn tracing_mw(TracingOptions opt = {})
    {
        return [opt = std::move(opt)](vix::middleware::Context &ctx, vix::middleware::Next next) mutable
        {
            TraceContext tc = build_trace_ctx(ctx, opt);

            if (opt.enrich)
                opt.enrich(ctx, tc);

            ctx.set_state(tc);

            next();

            emit_headers(ctx, tc, opt);
        };
    }

} // namespace vix::middleware::observability
