/**
 *
 *  @file csrf_smoke_test.cpp
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
#include <vix/middleware/security/csrf.hpp>

using namespace vix::middleware;

static vix::vhttp::Request make_req(bool ok)
{
  vix::vhttp::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  if (ok)
  {
    headers.emplace("Cookie", "csrf_token=abc");
    headers.emplace("X-CSRF-Token", "abc");
  }
  else
  {
    headers.emplace("Cookie", "csrf_token=abc");
    headers.emplace("X-CSRF-Token", "nope");
  }

  return vix::vhttp::Request("POST", "/x", std::move(headers), "x=1");
}

int main()
{
  {
    auto req = make_req(false);
    vix::vhttp::Response res;
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.use(vix::middleware::security::csrf());

    int final_calls = 0;
    p.run(req, w, [&](Request &, Response &resp)
          {
            final_calls++;
            resp.ok().text("OK"); });

    assert(final_calls == 0);
    assert(res.status() == 403);
  }

  {
    auto req = make_req(true);
    vix::vhttp::Response res;
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.use(vix::middleware::security::csrf());

    int final_calls = 0;
    p.run(req, w, [&](Request &, Response &resp)
          {
            final_calls++;
            resp.ok().text("OK"); });

    assert(final_calls == 1);
    assert(res.status() == 200);
  }

  std::cout << "[OK] csrf\n";
  return 0;
}
