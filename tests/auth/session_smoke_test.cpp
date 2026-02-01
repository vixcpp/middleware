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
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/auth/session.hpp> // a creer
#include <vix/middleware/http/cookies.hpp> // get/set cookie helpers

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string target = "/")
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{http::verb::get, target, 11};
  req.set(http::field::host, "localhost");
  return req;
}

static std::string header_value(
    const boost::beast::http::response<boost::beast::http::string_body> &res,
    boost::beast::http::field f)
{
  auto it = res.find(f);
  if (it == res.end())
    return {};
  return std::string(it->value());
}

int main()
{
  namespace http = boost::beast::http;

  HttpPipeline p;

  auth::SessionOptions opt{};
  opt.secret = "dev_secret";
  opt.cookie_name = "vix.sid";
  opt.auto_create = true;
  opt.ttl = std::chrono::seconds(60);
  p.use(auth::session(opt));

  auto raw = make_req("/secure");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  p.run(req, w, [&](Request &r, Response &ww)
        {
    // session must exist
    auto* s = r.try_state<auth::Session>();
    assert(s != nullptr);
    assert(!s->id.empty());
    assert(s->is_new == true);

    ww.ok().text("OK"); });

  assert(res.result_int() == 200);
  assert(res.body() == "OK");

  // must set cookie
  const std::string set_cookie = header_value(res, http::field::set_cookie);
  assert(!set_cookie.empty());
  assert(set_cookie.find("vix.sid=") != std::string::npos);

  std::cout << "[OK] session middleware: auto_create + set-cookie\n";
  return 0;
}
