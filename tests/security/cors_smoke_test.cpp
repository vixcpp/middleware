/**
 *
 *  @file cors_smoke_test.cpp
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

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/security/cors.hpp>

using namespace vix::middleware;

static vix::http::Request make_preflight()
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");
  headers.emplace("Origin", "https://example.com");
  headers.emplace("Access-Control-Request-Method", "POST");

  return vix::http::Request("OPTIONS", "/api", std::move(headers), "");
}

int main()
{
  auto req = make_preflight();
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  vix::middleware::security::CorsOptions opt;
  opt.allowed_origins = {"https://example.com"};
  opt.allow_any_origin = false;

  HttpPipeline p;
  p.use(vix::middleware::security::cors(opt));

  int final_calls = 0;
  p.run(req, w, [&](Request &, Response &resp)
        {
          final_calls++;
          resp.ok().text("OK"); });

  assert(final_calls == 0);
  assert(res.status() == 204);
  assert(res.has_header("Access-Control-Allow-Origin"));

  std::cout << "[OK] cors preflight\n";
  return 0;
}
