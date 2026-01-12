#include <iostream>
#include <boost/beast/http.hpp>

#include <vix/middleware/http_cache.hpp>
#include <vix/cache/Cache.hpp>
#include <vix/cache/CachePolicy.hpp>
#include <vix/cache/MemoryStore.hpp>

using namespace vix::middleware;
namespace http = boost::beast::http;

int main()
{
    using namespace vix::cache;

    // 1) Create cache
    auto store = std::make_shared<MemoryStore>();
    CachePolicy policy;
    policy.ttl_ms = 60'000; // 1 min
    auto cache = std::make_shared<Cache>(policy, store);

    // 2) Create middleware
    HttpCacheOptions opt{};
    auto cache_mw = http_cache(cache, opt);

    // 3) Fake request / response
    vix::vhttp::RawRequest raw{http::verb::get, "/api/ping", 11};
    raw.set(http::field::host, "localhost");

    vix::vhttp::Request req(raw, {});
    http::response<http::string_body> res;
    vix::vhttp::ResponseWrapper w(res);

    // 4) Call middleware
    cache_mw(req, w, [&]()
             {
        std::cout << "→ network\n";
        w.ok().text("Hello from network"); });

    // 5) Call again → cache HIT
    cache_mw(req, w, [&]()
             { std::cout << "→ should NOT run\n"; });

    std::cout << "Response body: " << res.body() << "\n";
}
