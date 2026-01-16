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

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req()
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{http::verb::get, "/dev", 11};
  req.set(http::field::host, "localhost");
  req.prepare_payload();
  return req;
}

int main()
{
  // Force dev env
#if defined(_WIN32)
  _putenv_s("VIX_ENV", "dev");
#else
  setenv("VIX_ENV", "dev", 1);
#endif

  namespace http = boost::beast::http;

  auto raw = make_req();
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;
  p.enable_dev_observability(); // should activate because VIX_ENV=dev

  p.run(req, w, [&](Request &, Response &)
        { w.ok().text("OK"); });

  assert(res.result_int() == 200);
  assert(res.body() == "OK");

  // tracing should emit ids
  assert(res.find("x-trace-id") != res.end());
  assert(res.find("x-span-id") != res.end());

  std::cout << "[OK] enable_dev_observability\n";
  return 0;
}
