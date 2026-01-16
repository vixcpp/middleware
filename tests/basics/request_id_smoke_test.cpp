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
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/basics/request_id.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(
    boost::beast::http::verb method,
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
  namespace http = boost::beast::http;

  vix::vhttp::RawRequest req{method, target, 11};
  req.set(http::field::host, "localhost");
  for (auto &kv : headers)
    req.set(kv.first, kv.second);
  req.prepare_payload();
  return req;
}

static void test_generates_and_sets_header()
{
  namespace http = boost::beast::http;

  auto raw = make_req(http::verb::get, "/x");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::basics::request_id());

  p.run(req, w, [&](Request &, Response &)
        {
        // final handler just returns OK
        w.ok().text("OK"); });

  // header must exist
  const auto it = res.find("x-request-id");
  assert(it != res.end());
  assert(!std::string(it->value()).empty());

  // typed state must exist
  auto *rid = req.try_state<vix::middleware::basics::RequestId>();
  assert(rid != nullptr);
  assert(!rid->value.empty());

  std::cout << "[OK] request_id generates and sets header\n";
}

static void test_accepts_incoming_header()
{
  namespace http = boost::beast::http;

  auto raw = make_req(http::verb::get, "/x", {{"x-request-id", "abcDEF-1234"}}); // valid
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  vix::middleware::basics::RequestIdOptions opt;
  opt.accept_incoming = true;
  opt.generate_if_missing = true;
  opt.always_set_response_header = true;

  p.use(vix::middleware::basics::request_id(opt));

  p.run(req, w, [&](Request &, Response &)
        { w.ok().text("OK"); });

  const auto it = res.find("x-request-id");
  assert(it != res.end());
  assert(std::string(it->value()) == "abcDEF-1234");

  auto *rid = req.try_state<vix::middleware::basics::RequestId>();
  assert(rid != nullptr);
  assert(rid->value == "abcDEF-1234");

  std::cout << "[OK] request_id accepts incoming header\n";
}

static void test_rejects_bad_incoming_and_generates()
{
  namespace http = boost::beast::http;

  // Too short + invalid char
  auto raw = make_req(http::verb::get, "/x", {{"x-request-id", "!!"}}); // rejected
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  vix::middleware::basics::RequestIdOptions opt;
  opt.accept_incoming = true;
  opt.generate_if_missing = true;

  p.use(vix::middleware::basics::request_id(opt));

  p.run(req, w, [&](Request &, Response &)
        { w.ok().text("OK"); });

  const auto it = res.find("x-request-id");
  assert(it != res.end());
  const std::string out = std::string(it->value());
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
