/**
 *
 *  @file hooks_merge_smoke_test.cpp
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

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/middleware.hpp>

using namespace vix::middleware;

static vix::http::Request make_req()
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  return vix::http::Request("GET", "/x", std::move(headers), "");
}

int main()
{
  auto req = make_req();
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;

  std::string trace;

  Hooks a;
  a.on_begin = [&](Context &)
  { trace += "A"; };
  a.on_end = [&](Context &)
  { trace += "a"; };

  Hooks b;
  b.on_begin = [&](Context &)
  { trace += "B"; };
  b.on_end = [&](Context &)
  { trace += "b"; };

  p.set_hooks(merge_hooks(a, b));

  p.run(req, w, [&](Request &, Response &resp)
        {
          trace += "F";
          resp.ok().text("OK"); });

  assert(trace == "ABFba");
  assert(res.status() == 200);
  assert(res.body() == "OK");

  std::cout << "[OK] merge_hooks order\n";
  return 0;
}
