/**
 *
 *  @file recovery_smoke_test.cpp
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
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/basics/request_id.hpp>
#include <vix/middleware/basics/recovery.hpp>

using namespace vix::middleware;

static vix::vhttp::Request make_req(
    std::string method,
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
  vix::vhttp::Request::HeaderMap map;
  map.emplace("Host", "localhost");

  for (const auto &kv : headers)
    map.emplace(kv.first, kv.second);

  return vix::vhttp::Request(
      std::move(method),
      std::move(target),
      std::move(map),
      "");
}

static std::string header_value(const vix::vhttp::Response &res, std::string_view name)
{
  return res.header(name);
}

struct TestLogger final : vix::middleware::basics::IRecoveryLogger
{
  int errors{0};
  std::string last;

  void error(std::string_view msg) override
  {
    ++errors;
    last = std::string(msg);
  }
};

static void test_recovery_catches_exception_and_returns_json()
{
  auto req = make_req("GET", "/boom");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  p.use(vix::middleware::basics::request_id());

  vix::middleware::basics::RecoveryOptions opt;
  opt.include_exception_message = true;
  p.use(vix::middleware::basics::recovery(opt));

  p.run(req, w, [&](Request &, Response &)
        { throw std::runtime_error("kaboom"); });

  assert(res.status() == 500);

  const std::string ct = header_value(res, "Content-Type");
  assert(!ct.empty());
  assert(ct.find("application/json") != std::string::npos);

  const std::string rid = header_value(res, "x-request-id");
  assert(!rid.empty());

  const std::string body = res.body();
  assert(body.find("\"code\"") != std::string::npos);
  assert(body.find("internal_server_error") != std::string::npos);
  assert(body.find("\"exception\"") != std::string::npos);
  assert(body.find("kaboom") != std::string::npos);

  std::cout << "[OK] recovery catches exception and returns JSON\n";
}

static void test_recovery_logs_via_services_if_present()
{
  auto req = make_req("GET", "/boom2");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  auto logger = std::make_shared<TestLogger>();
  p.services().provide<vix::middleware::basics::IRecoveryLogger>(logger);

  p.use(vix::middleware::basics::recovery({}));

  p.run(req, w, [&](Request &, Response &)
        { throw std::runtime_error("oops"); });

  assert(res.status() == 500);
  assert(logger->errors == 1);
  assert(logger->last.find("Unhandled exception") != std::string::npos);

  std::cout << "[OK] recovery logs via services (DI)\n";
}

int main()
{
  test_recovery_catches_exception_and_returns_json();
  test_recovery_logs_via_services_if_present();

  std::cout << "OK: recovery smoke tests passed\n";
  return 0;
}
