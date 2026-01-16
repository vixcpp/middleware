/**
 *
 *  @file multipart_smoke_test.cpp
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
#include <vix/middleware/parsers/multipart.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string ct)
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{http::verb::post, "/mp", 11};
  req.set(http::field::host, "localhost");
  req.set(http::field::content_type, ct);
  req.body() = "----X\r\ncontent\r\n----X--\r\n";
  req.prepare_payload();
  return req;
}

int main()
{
  namespace http = boost::beast::http;

  auto raw = make_req("multipart/form-data; boundary=----X");
  http::response<http::string_body> res;

  vix::vhttp::Request req(raw, {});
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::parsers::multipart());

  p.run(req, w, [&](Request &, Response &)
        {
              auto &info = req.state<vix::middleware::parsers::MultipartInfo>();
              w.ok().text(info.boundary); });

  assert(res.result_int() == 200);
  assert(res.body() == "----X");

  std::cout << "[OK] multipart info\n";
  return 0;
}
