/**
 *
 *  @file headers_smoke_test.cpp
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
#include <vix/middleware/security/headers.hpp>

using namespace vix::middleware;

static vix::vhttp::Request make_req()
{
  vix::vhttp::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  return vix::vhttp::Request("GET", "/x", std::move(headers), "");
}

int main()
{
  auto req = make_req();
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::security::headers());

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(res.status() == 200);
  assert(res.has_header("X-Content-Type-Options"));
  assert(res.has_header("X-Frame-Options"));

  std::cout << "[OK] security headers\n";
  return 0;
}
