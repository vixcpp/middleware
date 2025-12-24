#pragma once

#include <vix/middleware/app/http_cache.hpp>

namespace vix::middleware::app
{
    using HttpCacheConfig = HttpCacheAppConfig;

    inline auto http_cache(HttpCacheConfig cfg = {})
    {
        return http_cache_mw(std::move(cfg));
    }
}
