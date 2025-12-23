#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/core/context.hpp>
#include <vix/middleware/core/next.hpp>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::performance
{
    struct CompressionOptions
    {
        std::size_t min_size{1024}; // compress only if >= 1KB
        bool add_vary{true};
        bool enabled{true};
    };

    inline bool accepts_encoding(std::string_view accept, std::string_view token)
    {
        if (accept.empty())
            return false;
        return accept.find(token) != std::string_view::npos;
    }

    inline MiddlewareFn compression(CompressionOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            if (!opt.enabled)
            {
                next();
                return;
            }

            const std::string ae = ctx.req().header("accept-encoding");
            const bool wants_gzip = accepts_encoding(ae, "gzip");
            const bool wants_br = accepts_encoding(ae, "br");
            next();
            auto &raw = ctx.res().res;
            if (opt.add_vary)
                ctx.res().append("Vary", "Accept-Encoding");

            if (!wants_gzip && !wants_br)
                return;

            if (raw.body().size() < opt.min_size)
                return;

#ifndef NDEBUG
            ctx.res().header("X-Vix-Compression", "planned");
#endif
        };
    }

} // namespace vix::middleware::performance
