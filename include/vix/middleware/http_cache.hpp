#pragma once

#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cctype>

#include <boost/beast/http.hpp>

#include <vix/middleware/middleware.hpp>
#include <vix/http/cache/Cache.hpp>
#include <vix/http/cache/CacheContext.hpp>
#include <vix/http/cache/CacheKey.hpp>
#include <vix/http/cache/HeaderUtil.hpp>

namespace vix::middleware
{
    namespace http = boost::beast::http;

    struct HttpCacheOptions
    {
        std::vector<std::string> vary_headers{};
        bool cache_200_only{true};
        bool require_body{false};
        bool allow_bypass{true};
        std::string bypass_header{"x-vix-cache"};
        std::string bypass_value{"bypass"};
        std::function<vix::vhttp::cache::CacheContext(Request &)> context_provider{};
    };

    inline std::int64_t now_ms()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    inline std::string extract_query_raw_from_target(std::string_view target)
    {
        const auto qpos = target.find('?');
        if (qpos == std::string_view::npos)
            return {};
        if (qpos + 1 >= target.size())
            return {};
        return std::string(target.substr(qpos + 1));
    }

    inline std::unordered_map<std::string, std::string> request_headers_map(Request &req)
    {
        std::unordered_map<std::string, std::string> h;
        for (auto const &field : req.raw())
        {
            std::string k(field.name_string().data(), field.name_string().size());
            std::string v(field.value().data(), field.value().size());
            h.emplace(std::move(k), std::move(v));
        }
        return h;
    }

    inline std::unordered_map<std::string, std::string> response_headers_map(http::response<http::string_body> &res)
    {
        std::unordered_map<std::string, std::string> h;
        for (auto const &field : res.base())
        {
            std::string k(field.name_string().data(), field.name_string().size());
            std::string v(field.value().data(), field.value().size());
            h.emplace(std::move(k), std::move(v));
        }
        return h;
    }

    inline bool has_bypass(Request &req, const HttpCacheOptions &opt)
    {
        if (!opt.allow_bypass)
            return false;
        if (opt.bypass_header.empty())
            return false;

        const std::string v = req.header(opt.bypass_header);
        if (v.empty())
            return false;

        auto lower = [](std::string s)
        {
            for (char &c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };

        return lower(v) == lower(opt.bypass_value);
    }

    inline HttpMiddleware http_cache(
        std::shared_ptr<vix::vhttp::cache::Cache> cache,
        HttpCacheOptions opt = {})
    {
        return [cache = std::move(cache), opt = std::move(opt)](
                   Request &req, Response &res, Next next) mutable
        {
            if (!cache)
            {
                next();
                return;
            }

            if (req.method() != "GET")
            {
                next();
                return;
            }

            if (has_bypass(req, opt))
            {
                next();
                return;
            }

            const std::string query_raw = extract_query_raw_from_target(req.target());
            auto headers = request_headers_map(req);
            vix::vhttp::cache::HeaderUtil::normalizeInPlace(headers);

            const std::string key = vix::vhttp::cache::CacheKey::fromRequest(
                req.method(), req.path(), query_raw, headers, opt.vary_headers);

            vix::vhttp::cache::CacheContext ctx =
                opt.context_provider ? opt.context_provider(req)
                                     : vix::vhttp::cache::CacheContext::Online();

            const std::int64_t t0 = now_ms();

            if (auto hit = cache->get(key, t0, ctx))
            {
                res.status(hit->status);
                for (const auto &kv : hit->headers)
                    res.header(kv.first, kv.second);

                res.send(hit->body);
                return;
            }

            next(); // OK (NextOnce)

            auto &raw_res = res.res;
            const auto sc_u = raw_res.result_int(); // unsigned

            if (opt.cache_200_only && sc_u != 200u)
                return;

            const int sc = static_cast<int>(sc_u);

            if (opt.require_body && raw_res.body().empty())
                return;

            vix::vhttp::cache::CacheEntry e;
            e.status = sc;
            e.body = raw_res.body();
            e.created_at_ms = t0;
            e.headers = response_headers_map(raw_res);

            cache->put(key, e);
        };
    }

} // namespace vix::middleware
