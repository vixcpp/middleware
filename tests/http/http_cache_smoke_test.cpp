/**
 *
 *  @file http_cache_smoke_test.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/http_cache.hpp>
#include <vix/cache/Cache.hpp>
#include <vix/cache/CacheContext.hpp>
#include <vix/cache/CacheEntry.hpp>
#include <vix/cache/CacheKey.hpp>
#include <vix/cache/CachePolicy.hpp>
#include <vix/cache/HeaderUtil.hpp>
#include <vix/cache/MemoryStore.hpp>

using namespace vix::middleware;

static vix::vhttp::Request make_req(
    std::string method,
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {},
    std::string body = {})
{
  vix::vhttp::Request::HeaderMap map;
  map.emplace("Host", "localhost");

  for (const auto &kv : headers)
    map.emplace(kv.first, kv.second);

  return vix::vhttp::Request(
      std::move(method),
      std::move(target),
      std::move(map),
      std::move(body));
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
  std::unordered_map<std::string, std::string> headers = req.headers();
  vix::cache::HeaderUtil::normalizeInPlace(headers);

  return vix::cache::CacheKey::fromRequest(
      req.method(),
      req.path(),
      req.query_string(),
      headers,
      opt.vary_headers);
}

static void test_cache_hit_serves_response()
{
  std::shared_ptr<vix::cache::Cache> cache = make_cache();

  auto req = make_req("GET", "/api/users?page=1");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpCacheOptions opt{};
  auto key = compute_key_for(req, opt);

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
  assert(res.status() == 200);
  assert(res.body() == R"({"cached":true})");

  std::cout << "[OK] http_cache: hit serves cached response\n";
}

static void test_cache_miss_then_put_on_200()
{
  std::shared_ptr<vix::cache::Cache> cache = make_cache();

  auto req = make_req("GET", "/api/products?limit=10");
  vix::vhttp::Response res;
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
  assert(res.status() == 200);
  assert(res.body() == "from network");

  auto got = cache->get(key, now_ms(), vix::cache::CacheContext::Online());
  assert(got.has_value());
  assert(got->body == "from network");

  std::cout << "[OK] http_cache: miss -> next -> put(200)\n";
}

static void test_bypass_header_skips_cache()
{
  std::shared_ptr<vix::cache::Cache> cache = make_cache();

  auto req = make_req(
      "GET",
      "/api/x",
      {{"x-vix-cache", "bypass"}});
  vix::vhttp::Response res;
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
  assert(res.status() == 200);
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
