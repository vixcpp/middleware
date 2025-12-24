#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>

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

    inline bool starts_with_icase(std::string_view s, std::string_view prefix)
    {
        if (s.size() < prefix.size())
            return false;

        for (std::size_t i = 0; i < prefix.size(); ++i)
        {
            unsigned char a = static_cast<unsigned char>(s[i]);
            unsigned char b = static_cast<unsigned char>(prefix[i]);
            if (a >= 'A' && a <= 'Z')
                a = static_cast<unsigned char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z')
                b = static_cast<unsigned char>(b - 'A' + 'a');
            if (a != b)
                return false;
        }
        return true;
    }

    inline std::string extract_boundary(std::string_view ct)
    {
        // "multipart/form-data; boundary=----WebKitFormBoundaryabc"
        auto pos = ct.find("boundary=");
        if (pos == std::string_view::npos)
            return {};

        pos += std::string_view("boundary=").size();
        if (pos >= ct.size())
            return {};

        std::string_view b = ct.substr(pos);

        // trim spaces
        while (!b.empty() && (b.front() == ' ' || b.front() == '\t'))
            b.remove_prefix(1);

        // strip quotes
        if (!b.empty() && b.front() == '"')
        {
            b.remove_prefix(1);
            auto endq = b.find('"');
            if (endq != std::string_view::npos)
                b = b.substr(0, endq);
        }
        else
        {
            // stop at ; if any
            auto semi = b.find(';');
            if (semi != std::string_view::npos)
                b = b.substr(0, semi);
        }

        // trim end spaces
        while (!b.empty() && (b.back() == ' ' || b.back() == '\t'))
            b.remove_suffix(1);

        return std::string(b);
    }

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
            if (ct.empty() || !starts_with_icase(ct, "multipart/form-data"))
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
            info.boundary = extract_boundary(ct);

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
