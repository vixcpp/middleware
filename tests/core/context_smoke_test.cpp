/**
 *
 *  @file context_smoke_test.cpp
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

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/middleware.hpp>

using namespace vix::middleware;

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

struct FooState
{
  int x{0};
};

static void test_context_state_roundtrip()
{
  auto req = make_req("GET", "/ctx");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;

  p.use([](Context &ctx, Next next)
        {
          ctx.set_state(FooState{42});
          next(); });

  p.run(req, w, [&](Request &request, Response &resp)
        {
          auto *st = request.try_state<FooState>();
          assert(st != nullptr);
          assert(st->x == 42);
          resp.ok().text("OK"); });

  assert(res.status() == 200);
  assert(res.body() == "OK");

  std::cout << "[OK] Context state roundtrip\n";
}

static void test_context_send_error()
{
  auto req = make_req("GET", "/ctx-error");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;

  p.use([](Context &ctx, Next)
        {
          Error e;
          e.status = 401;
          e.code = "unauthorized";
          e.message = "Missing token";
          e.details["hint"] = "Provide Authorization: Bearer <token>";
          ctx.send_error(e); });

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("NO"); });

  assert(res.status() == 401);
  assert(res.body().find("\"code\"") != std::string::npos);
  assert(res.body().find("unauthorized") != std::string::npos);
  assert(res.body().find("Missing token") != std::string::npos);

  std::cout << "[OK] Context send_error\n";
}

int main()
{
  test_context_state_roundtrip();
  test_context_send_error();

  std::cout << "OK: core context smoke tests passed\n";
  return 0;
}
