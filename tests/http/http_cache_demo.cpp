/**
 *
 *  @file http_cache_demo.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#include <iostream>
#include <memory>
#include <string>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/http_cache.hpp>
#include <vix/cache/Cache.hpp>
#include <vix/cache/CachePolicy.hpp>
#include <vix/cache/MemoryStore.hpp>

using namespace vix::middleware;

int main()
{
  using namespace vix::cache;

  auto store = std::make_shared<MemoryStore>();
  CachePolicy policy;
  policy.ttl_ms = 60'000;
  auto cache = std::make_shared<Cache>(policy, store);

  HttpCacheOptions opt{};
  auto cache_mw = http_cache(cache, opt);

  vix::vhttp::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  vix::vhttp::Request req("GET", "/api/ping", std::move(headers), "");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  cache_mw(req, w, [&]()
           {
             std::cout << "-> network\n";
             w.ok().text("Hello from network"); });

  cache_mw(req, w, [&]()
           { std::cout << "-> should NOT run\n"; });

  std::cout << "Response body: " << res.body() << "\n";
  return 0;
}
