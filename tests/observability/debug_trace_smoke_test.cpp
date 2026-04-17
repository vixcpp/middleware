/**
 *
 *  @file debug_trace_smoke_test.cpp
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
#include <vix/middleware/observability/debug_trace.hpp>

using namespace vix::middleware;
using namespace vix::middleware::observability;

static vix::http::Request make_req(std::string method, std::string target)
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  return vix::http::Request(
      std::move(method),
      std::move(target),
      std::move(headers),
      "");
}

static void test_debug_trace_hooks_logs_two_lines()
{
  auto req = make_req("GET", "/dbg");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;

  auto sink = std::make_shared<InMemoryDebugTrace>();
  p.set_hooks(debug_trace_hooks(sink));

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(res.status() == 200);
  assert(res.body() == "OK");

  assert(sink->lines.size() >= 2);
  assert(sink->lines.front().find("begin") != std::string::npos);
  assert(sink->lines.back().find("end") != std::string::npos);

  std::cout << "[OK] debug_trace hooks\n";
}

static void test_debug_trace_mw_logs_two_lines()
{
  auto req = make_req("GET", "/dbg2");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;

  auto sink = std::make_shared<InMemoryDebugTrace>();
  p.use(debug_trace_mw(sink));

  p.run(req, w, [&](Request &, Response &resp)
        { resp.status(201).text("CREATED"); });

  assert(res.status() == 201);
  assert(res.body() == "CREATED");

  assert(sink->lines.size() >= 2);

  std::cout << "[OK] debug_trace middleware\n";
}

int main()
{
  test_debug_trace_hooks_logs_two_lines();
  test_debug_trace_mw_logs_two_lines();
  std::cout << "OK: observability debug_trace smoke tests passed\n";
  return 0;
}
