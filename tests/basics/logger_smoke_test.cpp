/**
 *
 *  @file logger_smoke_test.cpp
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
#include <vix/middleware/basics/request_id.hpp>
#include <vix/middleware/basics/timing.hpp>
#include <vix/middleware/basics/logger.hpp>

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

struct TestLogger final : vix::middleware::basics::ILogger
{
  int infos{0};
  int warns{0};
  int errors{0};
  std::string last;

  void info(std::string_view msg) override
  {
    infos++;
    last = std::string(msg);
  }
  void warn(std::string_view msg) override
  {
    warns++;
    last = std::string(msg);
  }
  void error(std::string_view msg) override
  {
    errors++;
    last = std::string(msg);
  }
};

static void test_logger_info_for_200_text()
{
  namespace http = boost::beast::http;

  auto raw = make_req(http::verb::get, "/ok");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  auto tl = std::make_shared<TestLogger>();
  p.services().provide<vix::middleware::basics::ILogger>(tl);

  p.use(vix::middleware::basics::request_id());
  p.use(vix::middleware::basics::timing({}));

  vix::middleware::basics::LoggerOptions opt;
  opt.format = vix::middleware::basics::LogFormat::Text;

  p.use(vix::middleware::basics::logger(opt));

  p.run(req, w, [&](Request &, Response &)
        { w.ok().text("OK"); });

  assert(tl->infos == 1);
  assert(tl->warns == 0);
  assert(tl->errors == 0);

  // contains method/path/status
  assert(tl->last.find("GET") != std::string::npos);
  assert(tl->last.find("/ok") != std::string::npos);
  assert(tl->last.find("200") != std::string::npos);

  // request id included
  assert(tl->last.find("rid=") != std::string::npos);

  std::cout << "[OK] logger logs info for 200\n";
}

static void test_logger_warn_for_404_json()
{
  namespace http = boost::beast::http;

  auto raw = make_req(http::verb::get, "/missing");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  auto tl = std::make_shared<TestLogger>();
  p.services().provide<vix::middleware::basics::ILogger>(tl);

  p.use(vix::middleware::basics::timing({}));

  vix::middleware::basics::LoggerOptions opt;
  opt.format = vix::middleware::basics::LogFormat::Json;

  p.use(vix::middleware::basics::logger(opt));

  p.run(req, w, [&](Request &, Response &)
        { w.status(404).json({"error", "not found"}); });

  assert(tl->infos == 0);
  assert(tl->warns == 1);
  assert(tl->errors == 0);

  // basic json fields
  assert(tl->last.find("\"method\"") != std::string::npos);
  assert(tl->last.find("\"path\"") != std::string::npos);
  assert(tl->last.find("\"status\":404") != std::string::npos);

  std::cout << "[OK] logger logs warn for 404\n";
}

static void test_logger_error_for_500()
{
  namespace http = boost::beast::http;

  auto raw = make_req(http::verb::get, "/err");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  auto tl = std::make_shared<TestLogger>();
  p.services().provide<vix::middleware::basics::ILogger>(tl);

  p.use(vix::middleware::basics::timing({}));
  p.use(vix::middleware::basics::logger({}));

  p.run(req, w, [&](Request &, Response &)
        { w.status(500).text("boom"); });

  assert(tl->errors == 1);

  std::cout << "[OK] logger logs error for 500\n";
}

int main()
{
  test_logger_info_for_200_text();
  test_logger_warn_for_404_json();
  test_logger_error_for_500();

  std::cout << "OK: logger smoke tests passed\n";
  return 0;
}
