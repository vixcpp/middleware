/**
 *
 *  @file pipeline_smoke_test.cpp
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
#include <string>
#include <utility>

#include <vix/middleware/pipeline.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/http/RequestHandler.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(std::string target = "/")
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  return vix::http::Request(
      "GET",
      std::move(target),
      std::move(headers),
      "");
}

static void test_pipeline_order_and_final()
{
  auto req = make_req("/x");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;

  std::string trace;

  p.use([&](Request &, Response &, Next next)
        {
          trace += "A";
          next();
          trace += "a"; });

  p.use([&](Request &, Response &, Next next)
        {
          trace += "B";
          next();
          trace += "b"; });

  p.run(req, w, [&](Request &, Response &resp)
        {
          trace += "F";
          resp.ok().text("OK"); });

  assert(trace == "ABFba");
  assert(res.status() == 200);
  assert(res.body() == "OK");

  std::cout << "[OK] pipeline order + final\n";
}

static void test_pipeline_short_circuit()
{
  auto req = make_req("/short");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  int called = 0;

  p.use([&](Request &, Response &resp, Next)
        {
          called++;
          resp.status(403).text("blocked"); });

  p.use([&](Request &, Response &, Next next)
        {
          called++;
          next(); });

  p.run(req, w, [&](Request &, Response &resp)
        {
          called++;
          resp.ok().text("final"); });

  assert(called == 1);
  assert(res.status() == 403);
  assert(res.body() == "blocked");

  std::cout << "[OK] pipeline short-circuit\n";
}

int main()
{
  test_pipeline_order_and_final();
  test_pipeline_short_circuit();
  std::cout << "OK: middleware pipeline smoke tests passed\n";
  return 0;
}
