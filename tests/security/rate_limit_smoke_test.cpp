/**
 *
 *  @file rate_limit_smoke_test.cpp
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
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/security/rate_limit.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(std::string target = "/")
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");
  headers.emplace("x-forwarded-for", "1.2.3.4");

  return vix::http::Request("GET", std::move(target), std::move(headers), "");
}

static void test_rate_limit_allows_then_blocks()
{
  vix::middleware::security::RateLimitOptions opt{};
  opt.capacity = 2.0;
  opt.refill_per_sec = 0.0;
  opt.add_headers = true;

  HttpPipeline p;

  auto shared = std::make_shared<vix::middleware::security::RateLimiterState>();
  p.services().provide<vix::middleware::security::RateLimiterState>(shared);

  p.use(vix::middleware::security::rate_limit(opt));

  auto run_once = [&](vix::http::Response &res)
  {
    auto req = make_req("/api/x");
    vix::http::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &resp)
          { resp.ok().text("OK"); });
  };

  {
    vix::http::Response res;
    run_once(res);
    assert(res.status() == 200);
    assert(res.body() == "OK");
    assert(!res.header("X-RateLimit-Limit").empty());
    assert(!res.header("X-RateLimit-Remaining").empty());
  }

  {
    vix::http::Response res;
    run_once(res);
    assert(res.status() == 200);
    assert(res.body() == "OK");
  }

  {
    vix::http::Response res;
    run_once(res);

    assert(res.status() == 429);
    assert(res.body().find("rate_limited") != std::string::npos);
    assert(!res.header("Retry-After").empty());
    assert(res.header("X-RateLimit-Remaining") == "0");
  }

  std::cout << "[OK] rate_limit: allows then blocks (capacity=2, refill=0)\n";
}

int main()
{
  test_rate_limit_allows_then_blocks();
  std::cout << "OK: middleware rate_limit smoke tests passed\n";
  return 0;
}
