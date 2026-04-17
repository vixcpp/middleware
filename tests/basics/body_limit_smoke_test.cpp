/**
 *
 *  @file body_limit_smoke_test.cpp
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
#include <string_view>
#include <unordered_map>
#include <utility>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/basics/body_limit.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(
    std::string method,
    std::string target,
    std::string body = "",
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
      std::move(body));
}

static bool has_header(const vix::http::Response &res, std::string_view name)
{
  return res.has_header(name);
}

static void test_rejects_large_body()
{
  auto req = make_req("POST", "/upload", std::string(20, 'x'));
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::basics::body_limit({.max_bytes = 10}));

  int final_calls = 0;
  p.run(req, w, [&](Request &, Response &resp)
        {
          final_calls++;
          resp.ok().text("OK"); });

  assert(final_calls == 0);
  assert(res.status() == 413);
  assert(has_header(res, "Content-Type"));

  std::cout << "[OK] body_limit rejects large body\n";
}

static void test_allows_small_body()
{
  auto req = make_req("POST", "/upload", "hello");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::basics::body_limit({.max_bytes = 10}));

  int final_calls = 0;
  p.run(req, w, [&](Request &, Response &resp)
        {
          final_calls++;
          resp.ok().text("OK"); });

  assert(final_calls == 1);
  assert(res.status() == 200);
  assert(res.body() == "OK");

  std::cout << "[OK] body_limit allows small body\n";
}

int main()
{
  test_rejects_large_body();
  test_allows_small_body();
  std::cout << "OK: body_limit smoke tests passed\n";
  return 0;
}
