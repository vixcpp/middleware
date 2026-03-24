/**
 *
 *  @file compression_smoke_test.cpp
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
#include <vix/middleware/performance/compression.hpp>

using namespace vix::middleware;

static vix::vhttp::Request make_req(
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
  vix::vhttp::Request::HeaderMap map;
  map.emplace("Host", "localhost");

  for (const auto &kv : headers)
    map.emplace(kv.first, kv.second);

  return vix::vhttp::Request("GET", std::move(target), std::move(map), "");
}

int main()
{
  HttpPipeline p;
  p.use(performance::compression({.min_size = 8}));

  auto req = make_req("/x", {{"Accept-Encoding", "gzip, br"}});
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text(std::string(20, 'a')); });

  assert(res.status() == 200);
  assert(!res.header("Vary").empty());

#ifndef NDEBUG
  assert(res.header("X-Vix-Compression") == "planned");
#endif

  std::cout << "[OK] compression smoke\n";
  return 0;
}
