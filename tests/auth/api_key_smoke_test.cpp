/**
 *
 *  @file api_key_smoke_test.cpp
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

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/auth/api_key.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(bool with_key)
{
  vix::http::Request req;
  req.set_method("GET");
  req.set_target("/secure");
  req.set_header("Host", "localhost");

  if (with_key)
    req.set_header("x-api-key", "secret");

  return req;
}

int main()
{
  HttpPipeline p;

  auth::ApiKeyOptions opt{};
  opt.allowed_keys.insert("secret");

  p.use(auth::api_key(opt));

  {
    auto req = make_req(true);
    vix::http::Response res;
    vix::http::ResponseWrapper w(res);

    p.run(req, w, [&](Request &r, Response &resp)
          {
            auto &k = r.state<auth::ApiKey>();
            assert(k.value == "secret");
            resp.ok().text("OK"); });

    assert(res.status() == 200);
  }

  {
    auto req = make_req(false);
    vix::http::Response res;
    vix::http::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &) {});

    assert(res.status() == 401);
  }

  std::cout << "[OK] api_key middleware\n";
  return 0;
}
