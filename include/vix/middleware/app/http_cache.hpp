#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <vix/middleware/app/adapter.hpp>
#include <vix/middleware/http_cache.hpp>

#include <vix/cache/Cache.hpp>
#include <vix/cache/CachePolicy.hpp>
#include <vix/cache/MemoryStore.hpp>

namespace vix::middleware::app
{
    struct HttpCacheAppConfig
    {
        std::string prefix{"/api/"};
        bool only_get{true};
        int ttl_ms{30'000};

        bool allow_bypass{true};
        std::string bypass_header{"x-vix-cache"};
        std::string bypass_value{"bypass"};

        std::vector<std::string> vary_headers{};
        std::shared_ptr<vix::cache::Cache> cache{};

        bool add_debug_header{false};
        std::string debug_header{"x-vix-cache-status"};
    };

    inline std::shared_ptr<vix::cache::Cache>
    make_default_cache(const HttpCacheAppConfig &cfg)
    {
        auto store = std::make_shared<vix::cache::MemoryStore>();
        vix::cache::CachePolicy policy;
        policy.ttl_ms = cfg.ttl_ms;

        return std::make_shared<vix::cache::Cache>(policy, store);
    }

    inline vix::App::Middleware http_cache_mw(HttpCacheAppConfig cfg = {})
    {
        auto cache = cfg.cache ? std::move(cfg.cache) : make_default_cache(cfg);

        vix::middleware::HttpCacheOptions opt{};
        opt.allow_bypass = cfg.allow_bypass;
        opt.bypass_header = cfg.bypass_header;
        opt.bypass_value = cfg.bypass_value;
        opt.vary_headers = std::move(cfg.vary_headers);

        auto inner = vix::middleware::http_cache(std::move(cache), opt);
        auto mw = vix::middleware::app::adapt(std::move(inner));

        if (cfg.only_get)
        {
            mw = vix::middleware::app::when(
                [](const vix::vhttp::Request &req)
                { return req.method() == "GET"; },
                std::move(mw));
        }

        return mw;
    }

    inline void install_http_cache(vix::App &app, HttpCacheAppConfig cfg = {})
    {
        std::string prefix = cfg.prefix;
        cfg.prefix.clear();

        auto mw = http_cache_mw(std::move(cfg));
        vix::middleware::app::install(app, std::move(prefix), std::move(mw));
    }

} // namespace vix::middleware::app
