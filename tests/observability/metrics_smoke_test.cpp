/**
 *
 *  @file metrics_smoke_test.cpp
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
#include <vix/middleware/observability/metrics.hpp>

using namespace vix::middleware;
using namespace vix::middleware::observability;

static vix::vhttp::Request make_req(std::string method, std::string target)
{
  vix::vhttp::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  return vix::vhttp::Request(
      std::move(method),
      std::move(target),
      std::move(headers),
      "");
}

static void test_metrics_hooks()
{
  auto req = make_req("GET", "/metrics");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  auto sink = std::make_shared<InMemoryMetrics>();
  MetricsOptions opt;
  opt.prefix = "vix_http";
  opt.include_path = false;

  p.set_hooks(metrics_hooks(sink, opt));

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(res.status() == 200);
  assert(res.body() == "OK");

  assert(sink->counter("vix_http_requests_total") == 1);
  assert(sink->counter("vix_http_responses_total") == 1);

  auto obs = sink->last_observation("vix_http_request_duration_ms");
  assert(obs != nullptr);
  assert(obs->count == 1);

  std::cout << "[OK] metrics hooks\n";
}

static void test_metrics_middleware()
{
  auto req = make_req("GET", "/metrics-mw");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  auto sink = std::make_shared<InMemoryMetrics>();
  p.use(metrics_mw(sink));

  p.run(req, w, [&](Request &, Response &resp)
        { resp.status(201).text("CREATED"); });

  assert(res.status() == 201);
  assert(res.body() == "CREATED");

  assert(sink->counter("vix_http_requests_total") == 1);
  assert(sink->counter("vix_http_responses_total") == 1);

  std::cout << "[OK] metrics middleware\n";
}

int main()
{
  test_metrics_hooks();
  test_metrics_middleware();

  std::cout << "OK: observability metrics smoke tests passed\n";
  return 0;
}
