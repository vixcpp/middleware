#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <vix/middleware/app/adapter.hpp>
#include <vix/middleware/http_cache.hpp>
#include <vix/http/cache/Cache.hpp>
#include <vix/http/cache/CachePolicy.hpp>
#include <vix/http/cache/MemoryStore.hpp>

namespace vix::middleware::app
{
    struct HttpCacheAppConfig
    {
        std::string prefix{"/api/"}; // used only by install_http_cache()
        bool only_get{true};

        int ttl_ms{30'000};

        bool allow_bypass{true};
        std::string bypass_header{"x-vix-cache"};
        std::string bypass_value{"bypass"};

        std::vector<std::string> vary_headers{};

        std::shared_ptr<vix::vhttp::cache::Cache> cache{};
    };

    inline std::shared_ptr<vix::vhttp::cache::Cache>
    make_default_cache(const HttpCacheAppConfig &cfg)
    {
        using namespace vix::vhttp::cache;

        auto store = std::make_shared<MemoryStore>();
        CachePolicy policy;
        policy.ttl_ms = cfg.ttl_ms;

        return std::make_shared<Cache>(policy, store);
    }

    inline vix::App::Middleware http_cache_mw(HttpCacheAppConfig cfg = {})
    {
        auto cache = cfg.cache ? std::move(cfg.cache) : make_default_cache(cfg);

        vix::middleware::HttpCacheOptions opt{};
        opt.allow_bypass = cfg.allow_bypass;
        opt.bypass_header = cfg.bypass_header;
        opt.bypass_value = cfg.bypass_value;
        opt.vary_headers = std::move(cfg.vary_headers);

        // low-level: middleware::http_cache(Request&, Response&, mw::Next)
        auto inner = vix::middleware::http_cache(std::move(cache), opt);

        // App::Middleware (Next = std::function<void()>)
        auto mw = vix::middleware::app::adapt(std::move(inner));

        if (cfg.only_get)
        {
            mw = vix::middleware::app::when(
                [](const vix::Request &req)
                { return req.method() == "GET"; },
                std::move(mw));
        }

        return mw;
    }

    // scoped install helper (prefix applied here)
    inline void install_http_cache(vix::App &app, HttpCacheAppConfig cfg = {})
    {
        auto prefix = std::move(cfg.prefix);
        cfg.prefix.clear();

        auto mw = http_cache_mw(std::move(cfg));
        vix::middleware::app::install(app, std::move(prefix), std::move(mw));
    }

} // namespace vix::middleware::app
