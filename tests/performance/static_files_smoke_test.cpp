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
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/performance/etag.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
  vix::http::Request::HeaderMap map;
  map.emplace("Host", "localhost");

  for (const auto &kv : headers)
    map.emplace(kv.first, kv.second);

  return vix::http::Request("GET", std::move(target), std::move(map), "");
}

int main()
{
  HttpPipeline p;
  p.use(performance::etag());

  std::string etag_value;

  {
    auto req = make_req("/x");
    vix::http::Response res;
    vix::http::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &resp)
          { resp.ok().text("Hello"); });

    assert(res.status() == 200);
    etag_value = res.header("ETag");
    assert(!etag_value.empty());
  }

  {
    auto req = make_req("/x", {{"If-None-Match", etag_value}});
    vix::http::Response res;
    vix::http::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &resp)
          { resp.ok().text("Hello"); });

    assert(res.status() == 304);
    assert(res.body().empty());
  }

  std::cout << "[OK] etag smoke\n";
  return 0;
}
