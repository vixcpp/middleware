/**
 *
 *  @file dev_observability_smoke_test.cpp
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
#include <cstdlib>
#include <iostream>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>

using namespace vix::middleware;

static vix::http::Request make_req()
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  return vix::http::Request("GET", "/dev", std::move(headers), "");
}

int main()
{
#if defined(_WIN32)
  _putenv_s("VIX_ENV", "dev");
#else
  setenv("VIX_ENV", "dev", 1);
#endif

  auto req = make_req();
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.enable_dev_observability();

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(res.status() == 200);
  assert(res.body() == "OK");

  assert(res.has_header("x-trace-id"));
  assert(res.has_header("x-span-id"));

  std::cout << "[OK] enable_dev_observability\n";
  return 0;
}
