/**
 *
 *  @file timing_smoke_test.cpp
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
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/basics/timing.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(std::string method, std::string target)
{
  vix::http::Request::HeaderMap map;
  map.emplace("Host", "localhost");

  return vix::http::Request(
      std::move(method),
      std::move(target),
      std::move(map),
      "");
}

static std::string header_value(const vix::http::Response &res, std::string_view name)
{
  return res.header(name);
}

static void test_timing_sets_headers_and_state()
{
  auto req = make_req("GET", "/slow");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::basics::timing({}));

  p.run(req, w, [&](Request &, Response &resp)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          resp.ok().text("OK"); });

  const std::string xrtv = header_value(res, "x-response-time");
  assert(!xrtv.empty());
  assert(xrtv.size() >= 3);
  assert(xrtv.find("ms") != std::string::npos);

  const std::string stv = header_value(res, "server-timing");
  assert(!stv.empty());
  assert(stv.find("total;dur=") != std::string::npos);

  auto *t = req.try_state<vix::middleware::basics::Timing>();
  assert(t != nullptr);
  assert(t->total_ms >= 0);

  std::cout << "[OK] timing sets headers + stores state\n";
}

static void test_timing_can_be_disabled()
{
  auto req = make_req("GET", "/x");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  vix::middleware::basics::TimingOptions opt;
  opt.set_x_response_time = false;
  opt.set_server_timing = false;

  HttpPipeline p;
  p.use(vix::middleware::basics::timing(opt));

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(header_value(res, "x-response-time").empty());
  assert(header_value(res, "server-timing").empty());

  auto *t = req.try_state<vix::middleware::basics::Timing>();
  assert(t != nullptr);

  std::cout << "[OK] timing can disable headers\n";
}

int main()
{
  test_timing_sets_headers_and_state();
  test_timing_can_be_disabled();

  std::cout << "OK: timing smoke tests passed\n";
  return 0;
}
