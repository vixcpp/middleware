#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::security
{
    struct CorsOptions
    {
        // Allow list. Empty => allow all (if allow_any_origin==true)
        std::vector<std::string> allowed_origins{};
        bool allow_any_origin{true};
        bool allow_credentials{false};
        // Preflight config
        std::vector<std::string> allow_methods{"GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"};
        std::vector<std::string> allow_headers{"Content-Type", "Authorization"};
        std::vector<std::string> expose_headers{};
        int max_age_seconds{600};

        bool vary_origin{true};
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

    inline std::string join_csv(const std::vector<std::string> &a)
    {
        std::string out;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (i)
                out += ", ";
            out += a[i];
        }
        return out;
    }

    inline bool origin_allowed(std::string_view origin, const CorsOptions &opt)
    {
        if (origin.empty())
            return false;

        if (opt.allow_any_origin && opt.allowed_origins.empty())
            return true;

        for (const auto &o : opt.allowed_origins)
        {
            if (o == "*" && opt.allow_any_origin)
                return true;
            if (o == origin)
                return true;
        }
        return false;
    }

    inline MiddlewareFn cors(CorsOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            auto &req = ctx.req();
            auto &res = ctx.res();

            const std::string origin = req.header("origin");
            const bool has_origin = !origin.empty();
            const bool allowed = has_origin ? origin_allowed(origin, opt) : false;

            const bool is_options = iequals(req.method(), "OPTIONS");
            const std::string acrm = req.header("access-control-request-method");
            const bool is_preflight = is_options && !acrm.empty();

            if (is_preflight)
            {
                if (!allowed)
                {
                    Error e;
                    e.status = 403;
                    e.code = "cors_forbidden";
                    e.message = "CORS origin not allowed";
                    e.details["origin"] = origin;
                    ctx.send_error(normalize(std::move(e)));
                    return;
                }

                res.status(204);
                res.header("Access-Control-Allow-Origin", (opt.allow_any_origin && !opt.allow_credentials) ? "*" : origin);
                res.header("Access-Control-Allow-Methods", join_csv(opt.allow_methods));
                res.header("Access-Control-Allow-Headers", join_csv(opt.allow_headers));
                res.header("Access-Control-Max-Age", std::to_string(opt.max_age_seconds));
                if (opt.allow_credentials)
                    res.header("Access-Control-Allow-Credentials", "true");
                if (opt.vary_origin)
                    res.append("Vary", "Origin");

                res.send(); // finalize empty
                return;
            }

            // Normal requests: run handler then set headers
            next();

            if (!has_origin)
                return;

            if (!allowed)
                return; // silently no CORS headers

            res.header("Access-Control-Allow-Origin", (opt.allow_any_origin && !opt.allow_credentials) ? "*" : origin);
            if (opt.allow_credentials)
                res.header("Access-Control-Allow-Credentials", "true");
            if (!opt.expose_headers.empty())
                res.header("Access-Control-Expose-Headers", join_csv(opt.expose_headers));
            if (opt.vary_origin)
                res.append("Vary", "Origin");
        };
    }

} // namespace vix::middleware::security
