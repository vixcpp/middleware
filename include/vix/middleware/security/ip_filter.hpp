#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/middleware/core/context.hpp>
#include <vix/middleware/core/next.hpp>
#include <vix/middleware/core/result.hpp>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::security
{
    struct IpFilterOptions
    {
        std::vector<std::string> allow{};
        std::vector<std::string> deny{};
        std::string header_name{"x-forwarded-for"}; // if behind proxy
        bool use_remote_addr_fallback{true};        // fallback: req.header("x-real-ip") etc (v1: best-effort)
    };

    inline std::string extract_client_ip(const vix::middleware::Request &req, const IpFilterOptions &opt)
    {
        std::string ip = req.header(opt.header_name);

        // x-forwarded-for may contain "client, proxy1, proxy2"
        auto comma = ip.find(',');
        if (comma != std::string::npos)
            ip = ip.substr(0, comma);

        // trim spaces
        while (!ip.empty() && (ip.front() == ' ' || ip.front() == '\t'))
            ip.erase(ip.begin());
        while (!ip.empty() && (ip.back() == ' ' || ip.back() == '\t'))
            ip.pop_back();

        if (!ip.empty())
            return ip;

        if (!opt.use_remote_addr_fallback)
            return {};

        // best-effort fallbacks commonly set by proxies
        ip = req.header("x-real-ip");
        if (!ip.empty())
            return ip;

        // no direct remote address exposed in current Request facade
        return {};
    }

    inline bool contains(const std::vector<std::string> &a, std::string_view v)
    {
        for (const auto &x : a)
        {
            if (x == v)
                return true;
        }
        return false;
    }

    inline MiddlewareFn ip_filter(IpFilterOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            const std::string ip = extract_client_ip(ctx.req(), opt);

            if (!opt.deny.empty() && contains(opt.deny, ip))
            {
                Error e;
                e.status = 403;
                e.code = "ip_denied";
                e.message = "Client IP is denied";
                e.details["ip"] = ip;
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            if (!opt.allow.empty() && !contains(opt.allow, ip))
            {
                Error e;
                e.status = 403;
                e.code = "ip_not_allowed";
                e.message = "Client IP is not in allow list";
                e.details["ip"] = ip;
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            next();
        };
    }

} // namespace vix::middleware::security
