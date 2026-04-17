/**
 *
 *  @file json_smoke_test.cpp
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
#include <vix/middleware/parsers/json.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(std::string body, std::string ct)
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");
  headers.emplace("Content-Type", std::move(ct));

  return vix::http::Request(
      "POST",
      "/json",
      std::move(headers),
      std::move(body));
}

int main()
{
  auto req = make_req(R"({"x":1})", "application/json; charset=utf-8");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::parsers::json());

  p.run(req, w, [&](Request &request, Response &resp)
        {
          auto &jb = request.state<vix::middleware::parsers::JsonBody>();
          resp.ok().text(jb.value["x"].dump()); });

  assert(res.status() == 200);
  assert(res.body() == "1");

  std::cout << "[OK] json parser\n";
  return 0;
}
