#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/helpers/strings/strings.hpp>

namespace vix::middleware::parsers
{
    struct MultipartInfo
    {
        std::string content_type{};
        std::string boundary{};
        std::size_t body_bytes{0};
    };

    struct MultipartOptions
    {
        bool require_boundary{true};
        std::size_t max_bytes{0}; // 0 => no limit
        bool store_in_state{true};
    };

    inline MiddlewareFn multipart(MultipartOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            auto &req = ctx.req();

            const std::string body = req.body();
            if (opt.max_bytes > 0 && body.size() > opt.max_bytes)
            {
                Error e;
                e.status = 413;
                e.code = "payload_too_large";
                e.message = "Request body exceeds multipart limit";
                e.details["max_bytes"] = std::to_string(opt.max_bytes);
                e.details["got_bytes"] = std::to_string(body.size());
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            const std::string ct = req.header("content-type");
            if (ct.empty() || !helpers::strings::starts_with_icase(ct, "multipart/form-data"))
            {
                Error e;
                e.status = 415;
                e.code = "unsupported_media_type";
                e.message = "Content-Type must be multipart/form-data";
                if (!ct.empty())
                    e.details["content_type"] = ct;
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            MultipartInfo info;
            info.content_type = ct;
            info.body_bytes = body.size();
            info.boundary = helpers::strings::extract_boundary(ct);

            if (opt.require_boundary && info.boundary.empty())
            {
                Error e;
                e.status = 400;
                e.code = "missing_boundary";
                e.message = "multipart/form-data boundary is missing";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            if (opt.store_in_state)
                ctx.set_state<MultipartInfo>(std::move(info));

            next();
        };
    }

} // namespace vix::middleware::parsers
