#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/http_cache.hpp>
#include <vix/cache/Cache.hpp>
#include <vix/cache/CachePolicy.hpp>
#include <vix/cache/MemoryStore.hpp>
#include <vix/cache/CacheKey.hpp>
#include <vix/cache/HeaderUtil.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(
    boost::beast::http::verb method,
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {},
    std::string body = {})
{
    namespace http = boost::beast::http;

    vix::vhttp::RawRequest req{method, target, 11};
    req.set(http::field::host, "localhost");

    for (auto &kv : headers)
        req.set(kv.first, kv.second);

    req.body() = std::move(body);
    req.prepare_payload();
    return req;
}

static std::shared_ptr<vix::cache::Cache> make_cache()
{
    auto store = std::make_shared<vix::cache::MemoryStore>();
    vix::cache::CachePolicy policy;
    policy.ttl_ms = 60'000;
    return std::make_shared<vix::cache::Cache>(policy, store);
}

static std::string compute_key_for(const vix::vhttp::Request &req,
                                   const HttpCacheOptions &opt)
{
    const std::string query_raw = extract_query_raw_from_target(req.target());

    std::unordered_map<std::string, std::string> headers;
    for (auto const &field : req.raw())
    {
        std::string k(field.name_string().data(), field.name_string().size());
        std::string v(field.value().data(), field.value().size());
        headers.emplace(std::move(k), std::move(v));
    }

    vix::cache::HeaderUtil::normalizeInPlace(headers);

    return vix::cache::CacheKey::fromRequest(
        req.method(),
        req.path(),
        query_raw,
        headers,
        opt.vary_headers);
}

static void test_cache_hit_serves_response()
{
    namespace http = boost::beast::http;

    std::shared_ptr<vix::cache::Cache> cache = make_cache();

    // request
    auto raw = make_req(http::verb::get, "/api/users?page=1");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpCacheOptions opt{};
    auto key = compute_key_for(req, opt);

    // prefill cache
    vix::cache::CacheEntry e;
    e.status = 200;
    e.body = R"({"cached":true})";
    e.created_at_ms = now_ms();
    e.headers["content-type"] = "application/json";
    cache->put(key, e);

    int next_calls = 0;
    auto mw = http_cache(cache, opt);

    mw(req, w, [&]()
       {
        next_calls++;
        w.ok().text("should not run"); });

    assert(next_calls == 0);
    assert(res.result_int() == 200);
    assert(res.body() == R"({"cached":true})");

    std::cout << "[OK] http_cache: hit serves cached response\n";
}

static void test_cache_miss_then_put_on_200()
{
    namespace http = boost::beast::http;
    std::shared_ptr<vix::cache::Cache> cache = make_cache();

    auto raw = make_req(http::verb::get, "/api/products?limit=10");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpCacheOptions opt{};
    auto key = compute_key_for(req, opt);

    int next_calls = 0;
    auto mw = http_cache(cache, opt);

    mw(req, w, [&]()
       {
        next_calls++;
        w.ok().text("from network"); });

    assert(next_calls == 1);
    assert(res.result_int() == 200);
    assert(res.body() == "from network");

    // should now be cached
    auto got = cache->get(key, now_ms(), vix::cache::CacheContext::Online());
    assert(got.has_value());
    assert(got->body == "from network");

    std::cout << "[OK] http_cache: miss -> next -> put(200)\n";
}

static void test_bypass_header_skips_cache()
{
    namespace http = boost::beast::http;

    std::shared_ptr<vix::cache::Cache> cache = make_cache();

    auto raw = make_req(
        http::verb::get,
        "/api/x",
        {{"x-vix-cache", "bypass"}});
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpCacheOptions opt{};
    opt.allow_bypass = true;
    opt.bypass_header = "x-vix-cache";
    opt.bypass_value = "bypass";

    int next_calls = 0;
    auto mw = http_cache(cache, opt);

    mw(req, w, [&]()
       {
        next_calls++;
        w.ok().text("bypassed"); });

    assert(next_calls == 1);
    assert(res.body() == "bypassed");

    std::cout << "[OK] http_cache: bypass header works\n";
}

int main()
{
    test_cache_hit_serves_response();
    test_cache_miss_then_put_on_200();
    test_bypass_header_skips_cache();

    std::cout << "OK: middleware http_cache smoke tests passed\n";
    return 0;
}
