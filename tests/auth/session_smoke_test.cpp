/**
 *
 *  @file session_smoke_test.cpp
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
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/auth/session.hpp>
#include <vix/middleware/http/cookies.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(std::string target = "/")
{
  vix::http::Request req;
  req.set_method("GET");
  req.set_target(std::move(target));
  req.set_header("Host", "localhost");
  return req;
}

template <typename ResponseLike>
static std::string header_value(const ResponseLike &res, std::string_view name)
{
  for (const auto &[key, value] : res.headers())
  {
    if (key == name)
      return value;
  }
  return {};
}

int main()
{
  HttpPipeline p;

  auth::SessionOptions opt{};
  opt.secret = "dev_secret";
  opt.cookie_name = "vix.sid";
  opt.auto_create = true;
  opt.ttl = std::chrono::seconds(60);
  p.use(auth::session(opt));

  auto req = make_req("/secure");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  p.run(req, w, [&](Request &r, Response &resp)
        {
          auto *s = r.try_state<auth::Session>();
          assert(s != nullptr);
          assert(!s->id.empty());
          assert(s->is_new == true);

          resp.ok().text("OK"); });

  assert(res.status() == 200);
  assert(res.body() == "OK");

  const std::string set_cookie = header_value(res, "Set-Cookie");
  assert(!set_cookie.empty());
  assert(set_cookie.find("vix.sid=") != std::string::npos);

  std::cout << "[OK] session middleware: auto_create + set-cookie\n";
  return 0;
}
