/**
 *
 *  @file rbac_smoke_test.cpp
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

#include <nlohmann/json.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/auth/rbac.hpp>

using namespace vix::middleware;

static vix::http::Request make_req()
{
  vix::http::Request req;
  req.set_method("GET");
  req.set_target("/admin");
  req.set_header("Host", "localhost");
  return req;
}

int main()
{
  HttpPipeline p;

  p.use(auth::rbac_context());
  p.use(auth::require_role("admin"));
  p.use(auth::require_perm("products:write"));

  {
    auto req = make_req();
    vix::http::Response res;
    vix::http::ResponseWrapper w(res);

    auth::JwtClaims claims;
    claims.subject = "user123";
    claims.payload = nlohmann::json{
        {"sub", "user123"},
        {"roles", {"admin"}},
        {"perms", {"products:write", "orders:read"}}};

    req.emplace_state<auth::JwtClaims>(std::move(claims));

    p.run(req, w, [&](Request &, Response &resp)
          { resp.ok().text("OK"); });

    assert(res.status() == 200);
    assert(res.body() == "OK");
  }

  {
    auto req = make_req();
    vix::http::Response res;
    vix::http::ResponseWrapper w(res);

    auth::JwtClaims claims;
    claims.subject = "user123";
    claims.payload = nlohmann::json{
        {"sub", "user123"},
        {"roles", {"admin"}},
        {"perms", {"orders:read"}}};

    req.emplace_state<auth::JwtClaims>(std::move(claims));

    p.run(req, w, [&](Request &, Response &resp)
          { resp.ok().text("SHOULD NOT"); });

    assert(res.status() == 403);
  }

  std::cout << "[OK] rbac (roles + perms)\n";
  return 0;
}
