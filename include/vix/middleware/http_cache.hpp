#ifndef HTTP_CACHE_HPP
#define HTTP_CACHE_HPP

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
#include <vix/cache/Cache.hpp>
#include <vix/cache/CacheContext.hpp>
#include <vix/cache/CacheKey.hpp>
#include <vix/cache/HeaderUtil.hpp>

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
        std::function<vix::cache::CacheContext(Request &)> context_provider{};
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

    inline std::unordered_map<std::string, std::string>
    response_headers_map(http::response<http::string_body> &res)
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

    inline bool ieq_ascii(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            unsigned char ca = static_cast<unsigned char>(a[i]);
            unsigned char cb = static_cast<unsigned char>(b[i]);
            if (std::tolower(ca) != std::tolower(cb))
                return false;
        }
        return true;
    }

    inline HttpMiddleware http_cache(
        std::shared_ptr<vix::cache::Cache> cache,
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

            // keep it strict (GET only) — app layer also has only_get, but safe here
            if (req.method() != "GET")
            {
                next();
                return;
            }

            // bypass: skip cache entirely
            if (has_bypass(req, opt))
            {
                next();
                // debug header (optional but helpful)
                res.header("x-vix-cache-status", "bypass");
                return;
            }

            const std::string query_raw = extract_query_raw_from_target(req.target());
            auto req_headers = request_headers_map(req);
            vix::cache::HeaderUtil::normalizeInPlace(req_headers);

            const std::string key = vix::cache::CacheKey::fromRequest(
                req.method(), req.path(), query_raw, req_headers, opt.vary_headers);

            vix::cache::CacheContext ctx =
                opt.context_provider ? opt.context_provider(req)
                                     : vix::cache::CacheContext::Online();

            const std::int64_t t0 = now_ms();

            // HIT: replay cached response
            if (auto hit = cache->get(key, t0, ctx))
            {
                auto &raw_res = res.res;

                // status
                raw_res.result(static_cast<http::status>(hit->status));

                // restore headers (skip content-length, we will recompute)
                for (const auto &kv : hit->headers)
                {
                    if (ieq_ascii(kv.first, "content-length"))
                        continue;

                    // write directly to beast response (won't be overridden by res.send)
                    raw_res.set(kv.first, kv.second);
                }

                raw_res.set("x-vix-cache-status", "hit");

                // body
                raw_res.body() = hit->body;
                raw_res.prepare_payload(); // recalculates content-length
                return;
            }

            // MISS: let handler generate response, then cache it
            next(); // OK (NextOnce)

            // At this point, res.res is finalized by handlers like res.json()
            auto &raw_res = res.res;
            const auto sc_u = raw_res.result_int();

            // always helpful to debug
            res.header("x-vix-cache-status", "miss");

            if (opt.cache_200_only && sc_u != 200u)
                return;

            if (opt.require_body && raw_res.body().empty())
                return;

            vix::cache::CacheEntry e;
            e.status = static_cast<int>(sc_u);
            e.body = raw_res.body();
            e.created_at_ms = t0;

            // capture + normalize headers so we can reliably replay content-type, etc.
            e.headers = response_headers_map(raw_res);
            vix::cache::HeaderUtil::normalizeInPlace(e.headers);
            cache->put(key, e);
        };
    }

} // namespace vix::middleware

#endif
