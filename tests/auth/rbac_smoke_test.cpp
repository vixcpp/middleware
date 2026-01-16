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

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/auth/rbac.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req()
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{http::verb::get, "/admin", 11};
  req.set(http::field::host, "localhost");
  return req;
}

int main()
{
  namespace http = boost::beast::http;

  HttpPipeline p;

  p.use(auth::rbac_context());
  p.use(auth::require_role("admin"));
  p.use(auth::require_perm("products:write"));

  {
    auto raw = make_req();
    http::response<http::string_body> res;
    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    auth::JwtClaims claims;
    claims.subject = "user123";
    claims.payload = nlohmann::json{
        {"sub", "user123"},
        {"roles", {"admin"}},
        {"perms", {"products:write", "orders:read"}}};

    req.emplace_state<auth::JwtClaims>(std::move(claims));

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("OK"); });

    assert(res.result_int() == 200);
    assert(res.body() == "OK");
  }

  {
    auto raw = make_req();
    http::response<http::string_body> res;
    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    auth::JwtClaims claims;
    claims.subject = "user123";
    claims.payload = nlohmann::json{
        {"sub", "user123"},
        {"roles", {"admin"}},
        {"perms", {"orders:read"}}}; // missing products:write

    req.emplace_state<auth::JwtClaims>(std::move(claims));

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("SHOULD NOT"); });

    assert(res.result_int() == 403);
  }

  std::cout << "[OK] rbac (roles + perms)\n";
  return 0;
}
