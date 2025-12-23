#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/core/context.hpp>
#include <vix/middleware/core/next.hpp>
#include <vix/middleware/core/result.hpp>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::security
{
    struct CsrfOptions
    {
        std::string cookie_name{"csrf_token"};
        std::string header_name{"x-csrf-token"};
        bool protect_get{false};
    };

    inline bool iequals(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            unsigned char ca = static_cast<unsigned char>(a[i]);
            unsigned char cb = static_cast<unsigned char>(b[i]);
            if (ca >= 'A' && ca <= 'Z')
                ca = static_cast<unsigned char>(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z')
                cb = static_cast<unsigned char>(cb - 'A' + 'a');
            if (ca != cb)
                return false;
        }
        return true;
    }

    inline bool is_unsafe_method(std::string_view m, bool protect_get)
    {
        if (protect_get && iequals(m, "GET"))
            return true;

        return iequals(m, "POST") || iequals(m, "PUT") || iequals(m, "PATCH") || iequals(m, "DELETE");
    }

    inline std::string extract_cookie(std::string_view cookie_header, std::string_view name)
    {
        // naive parse: "a=1; csrf_token=xyz; b=2"
        std::string_view s = cookie_header;
        while (!s.empty())
        {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == ';'))
                s.remove_prefix(1);

            auto eq = s.find('=');
            if (eq == std::string_view::npos)
                break;

            auto key = s.substr(0, eq);
            s.remove_prefix(eq + 1);

            auto end = s.find(';');
            auto val = (end == std::string_view::npos) ? s : s.substr(0, end);

            // trim key
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
                key.remove_suffix(1);

            if (key == name)
                return std::string(val);

            if (end == std::string_view::npos)
                break;
            s.remove_prefix(end + 1);
        }

        return {};
    }

    inline MiddlewareFn csrf(CsrfOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            const auto &req = ctx.req();

            if (!is_unsafe_method(req.method(), opt.protect_get))
            {
                next();
                return;
            }

            const std::string header_token = req.header(opt.header_name);
            const std::string cookie = req.header("cookie");
            const std::string cookie_token = extract_cookie(cookie, opt.cookie_name);

            if (header_token.empty() || cookie_token.empty() || header_token != cookie_token)
            {
                Error e;
                e.status = 403;
                e.code = "csrf_failed";
                e.message = "CSRF token missing or invalid";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            next();
        };
    }

} // namespace vix::middleware::security
