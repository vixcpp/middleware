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

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/observability/debug_trace.hpp>

using namespace vix::middleware;
using namespace vix::middleware::observability;

static vix::vhttp::RawRequest make_req(boost::beast::http::verb method, std::string target)
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{method, target, 11};
  req.set(http::field::host, "localhost");
  req.prepare_payload();
  return req;
}

static void test_debug_trace_hooks_logs_two_lines()
{
  namespace http = boost::beast::http;

  auto raw = make_req(http::verb::get, "/dbg");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  auto sink = std::make_shared<InMemoryDebugTrace>();
  p.set_hooks(debug_trace_hooks(sink));

  p.run(req, w, [&](Request &, Response &)
        { w.ok().text("OK"); });

  assert(res.result_int() == 200);
  assert(res.body() == "OK");

  assert(sink->lines.size() >= 2);
  // basic sanity
  assert(sink->lines.front().find("begin") != std::string::npos);
  assert(sink->lines.back().find("end") != std::string::npos);

  std::cout << "[OK] debug_trace hooks\n";
}

static void test_debug_trace_mw_logs_two_lines()
{
  namespace http = boost::beast::http;

  auto raw = make_req(http::verb::get, "/dbg2");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  auto sink = std::make_shared<InMemoryDebugTrace>();
  p.use(debug_trace_mw(sink));

  p.run(req, w, [&](Request &, Response &)
        { w.status(201).text("CREATED"); });

  assert(res.result_int() == 201);
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
