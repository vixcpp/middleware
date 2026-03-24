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
#include <string>
#include <utility>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/parsers/multipart.hpp>

using namespace vix::middleware;

static vix::vhttp::Request make_req(std::string ct)
{
  vix::vhttp::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");
  headers.emplace("Content-Type", std::move(ct));

  return vix::vhttp::Request(
      "POST",
      "/mp",
      std::move(headers),
      "----X\r\ncontent\r\n----X--\r\n");
}

int main()
{
  auto req = make_req("multipart/form-data; boundary=----X");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::parsers::multipart());

  p.run(req, w, [&](Request &request, Response &resp)
        {
          auto &info = request.state<vix::middleware::parsers::MultipartInfo>();
          resp.ok().text(info.boundary); });

  assert(res.status() == 200);
  assert(res.body() == "----X");

  std::cout << "[OK] multipart info\n";
  return 0;
}
