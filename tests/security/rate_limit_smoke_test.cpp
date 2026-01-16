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

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/security/rate_limit.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string target = "/")
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{http::verb::get, target, 11};
  req.set(http::field::host, "localhost");
  req.set("x-forwarded-for", "1.2.3.4");
  return req;
}

static void test_rate_limit_allows_then_blocks()
{
  namespace http = boost::beast::http;

  // capacity=2 tokens, refill=0 => after 2 requests, third should be blocked
  vix::middleware::security::RateLimitOptions opt{};
  opt.capacity = 2.0;
  opt.refill_per_sec = 0.0;
  opt.add_headers = true;

  HttpPipeline p;

  // Provide shared state in services (better than static fallback for tests)
  auto shared = std::make_shared<vix::middleware::security::RateLimiterState>();
  p.services().provide<vix::middleware::security::RateLimiterState>(shared);

  p.use(vix::middleware::security::rate_limit(opt));

  auto run_once = [&](boost::beast::http::response<http::string_body> &res)
  {
    auto raw = make_req("/api/x");
    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("OK"); });
  };

  // 1) OK
  {
    http::response<http::string_body> res;
    run_once(res);
    assert(res.result_int() == 200);
    assert(res.body() == "OK");
    assert(!res["X-RateLimit-Limit"].empty());
    assert(!res["X-RateLimit-Remaining"].empty());
  }

  // 2) OK
  {
    http::response<http::string_body> res;
    run_once(res);
    assert(res.result_int() == 200);
    assert(res.body() == "OK");
  }

  // 3) BLOCKED
  {
    http::response<http::string_body> res;
    run_once(res);

    assert(res.result_int() == 429);
    // body is JSON (from ctx.send_error)
    assert(res.body().find("rate_limited") != std::string::npos);
    assert(!res["Retry-After"].empty());
    assert(res["X-RateLimit-Remaining"] == "0");
  }

  std::cout << "[OK] rate_limit: allows then blocks (capacity=2, refill=0)\n";
}

int main()
{
  test_rate_limit_allows_then_blocks();
  std::cout << "OK: middleware rate_limit smoke tests passed\n";
  return 0;
}
