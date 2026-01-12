#ifndef HEADERS_HPP
#define HEADERS_HPP

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::security
{
    struct SecurityHeadersOptions
    {
        bool x_content_type_options{true}; // nosniff
        bool x_frame_options{true};        // DENY
        bool x_xss_protection{false};      // legacy (often disabled)
        bool referrer_policy{true};        // no-referrer
        bool permissions_policy{true};     // minimal deny
        bool hsts{false};                  // OFF by default (enable only on HTTPS)
        int hsts_max_age{31536000};        // 1 year
        bool hsts_include_subdomains{true};
        bool hsts_preload{false};
        // CSP (empty => no CSP)
        std::string content_security_policy{};
    };

    inline MiddlewareFn headers(SecurityHeadersOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            next(); // post: apply on response regardless of handler

            auto &res = ctx.res();

            if (opt.x_content_type_options)
                res.header("X-Content-Type-Options", "nosniff");

            if (opt.x_frame_options)
                res.header("X-Frame-Options", "DENY");

            if (opt.x_xss_protection)
                res.header("X-XSS-Protection", "0");

            if (opt.referrer_policy)
                res.header("Referrer-Policy", "no-referrer");

            if (opt.permissions_policy)
                res.header("Permissions-Policy", "geolocation=(), microphone=(), camera=()");

            if (!opt.content_security_policy.empty())
                res.header("Content-Security-Policy", opt.content_security_policy);

            if (opt.hsts)
            {
                std::string v = "max-age=" + std::to_string(opt.hsts_max_age);
                if (opt.hsts_include_subdomains)
                    v += "; includeSubDomains";
                if (opt.hsts_preload)
                    v += "; preload";
                res.header("Strict-Transport-Security", v);
            }
        };
    }

} // namespace vix::middleware::security

#endif