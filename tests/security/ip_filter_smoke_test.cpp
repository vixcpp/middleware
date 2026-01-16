/**
 *
 *  @file ip_filter_smoke_test.cpp
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
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/security/ip_filter.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string ip)
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{http::verb::get, "/x", 11};
  req.set(http::field::host, "localhost");
  req.set("X-Forwarded-For", ip);
  req.prepare_payload();
  return req;
}

int main()
{
  namespace http = boost::beast::http;

  auto raw = make_req("1.2.3.4");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  vix::middleware::security::IpFilterOptions opt;
  opt.deny = {"1.2.3.4"};

  HttpPipeline p;
  p.use(vix::middleware::security::ip_filter(opt));

  int final_calls = 0;
  p.run(req, w, [&](Request &, Response &)
        { final_calls++; w.ok().text("OK"); });

  assert(final_calls == 0);
  assert(res.result_int() == 403);

  std::cout << "[OK] ip_filter deny\n";
  return 0;
}
