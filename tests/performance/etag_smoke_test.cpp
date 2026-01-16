/**
 *
 *  @file etag_smoke_test.cpp
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
#include <vix/middleware/performance/etag.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string target, std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{http::verb::get, target, 11};
  req.set(http::field::host, "localhost");
  for (auto &kv : headers)
    req.set(kv.first, kv.second);
  return req;
}

int main()
{
  namespace http = boost::beast::http;

  HttpPipeline p;
  p.use(performance::etag());

  // 1) First response sets ETag
  std::string etag;
  {
    auto raw = make_req("/x");
    http::response<http::string_body> res;
    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("Hello"); });

    assert(res.result_int() == 200);
    etag = std::string(res["ETag"]);
    assert(!etag.empty());
  }

  // 2) If-None-Match => 304
  {
    auto raw = make_req("/x", {{"If-None-Match", etag}});
    http::response<http::string_body> res;
    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("Hello"); });

    assert(res.result_int() == 304);
    assert(res.body().empty());
  }

  std::cout << "[OK] etag smoke\n";
  return 0;
}
