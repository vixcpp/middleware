/**
 *
 *  @file request_id_smoke_test.cpp
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
#include <vix/middleware/basics/request_id.hpp>

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

static std::string header_value(const vix::http::Response &res, std::string_view name)
{
  return res.header(name);
}

static void test_generates_and_sets_header()
{
  auto req = make_req("GET", "/x");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::basics::request_id());

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  const std::string out = header_value(res, "x-request-id");
  assert(!out.empty());

  auto *rid = req.try_state<vix::middleware::basics::RequestId>();
  assert(rid != nullptr);
  assert(!rid->value.empty());

  std::cout << "[OK] request_id generates and sets header\n";
}

static void test_accepts_incoming_header()
{
  auto req = make_req("GET", "/x", {{"x-request-id", "abcDEF-1234"}});
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;

  vix::middleware::basics::RequestIdOptions opt;
  opt.accept_incoming = true;
  opt.generate_if_missing = true;
  opt.always_set_response_header = true;

  p.use(vix::middleware::basics::request_id(opt));

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  const std::string out = header_value(res, "x-request-id");
  assert(out == "abcDEF-1234");

  auto *rid = req.try_state<vix::middleware::basics::RequestId>();
  assert(rid != nullptr);
  assert(rid->value == "abcDEF-1234");

  std::cout << "[OK] request_id accepts incoming header\n";
}

static void test_rejects_bad_incoming_and_generates()
{
  auto req = make_req("GET", "/x", {{"x-request-id", "!!"}});
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;

  vix::middleware::basics::RequestIdOptions opt;
  opt.accept_incoming = true;
  opt.generate_if_missing = true;

  p.use(vix::middleware::basics::request_id(opt));

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  const std::string out = header_value(res, "x-request-id");
  assert(!out.empty());
  assert(out != "!!");

  auto *rid = req.try_state<vix::middleware::basics::RequestId>();
  assert(rid != nullptr);
  assert(!rid->value.empty());
  assert(rid->value != "!!");

  std::cout << "[OK] request_id rejects bad header and generates\n";
}

int main()
{
  test_generates_and_sets_header();
  test_accepts_incoming_header();
  test_rejects_bad_incoming_and_generates();

  std::cout << "OK: request_id smoke tests passed\n";
  return 0;
}
