/**
 *
 *  @file tracing_smoke_test.cpp
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
#include <string>
#include <utility>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/observability/tracing.hpp>

using namespace vix::middleware;
using namespace vix::middleware::observability;

static vix::http::Request make_req(
    std::string method,
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
  vix::http::Request::HeaderMap map;
  map.emplace("Host", "localhost");

  for (const auto &kv : headers)
    map.emplace(kv.first, kv.second);

  return vix::http::Request(
      std::move(method),
      std::move(target),
      std::move(map),
      "");
}

static void test_tracing_hooks_generates_and_sets_headers()
{
  auto req = make_req("GET", "/t");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.set_hooks(tracing_hooks());

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(res.status() == 200);
  assert(res.body() == "OK");

  assert(res.has_header("x-trace-id"));
  assert(res.has_header("x-span-id"));

  std::cout << "[OK] tracing hooks headers\n";
}

static void test_tracing_mw_accepts_incoming_trace()
{
  const std::string incoming_trace = "0123456789abcdef0123456789abcdef";
  auto req = make_req("GET", "/t2", {{"x-trace-id", incoming_trace}});
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(tracing_mw());

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(res.has_header("x-trace-id"));
  assert(res.header("x-trace-id") == incoming_trace);

  std::cout << "[OK] tracing middleware incoming trace\n";
}

int main()
{
  test_tracing_hooks_generates_and_sets_headers();
  test_tracing_mw_accepts_incoming_trace();
  std::cout << "OK: observability tracing smoke tests passed\n";
  return 0;
}
