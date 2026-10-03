/**
 *
 *  @file form_smoke_test.cpp
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
#include <vix/middleware/parsers/form.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(std::string body, std::string ct)
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");
  headers.emplace("Content-Type", std::move(ct));

  return vix::http::Request(
      "POST",
      "/form",
      std::move(headers),
      std::move(body));
}

int main()
{
  auto req = make_req("a=1&b=hello+world", "application/x-www-form-urlencoded");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::parsers::form());

  p.run(req, w, [&](Request &request, Response &resp)
        {
          auto &fb = request.state<vix::middleware::parsers::FormBody>();
          resp.ok().text(fb.fields["b"]); });

  assert(res.status() == 200);
  assert(res.body() == "hello world");

  for (const std::string content_type : {
           std::string{"APPLICATION/X-WWW-FORM-URLENCODED"},
           std::string{"Application/X-Www-Form-Urlencoded; charset=utf-8"},
       })
  {
    auto typed_req = make_req("a=1", content_type);
    vix::http::Response typed_res;
    vix::http::ResponseWrapper typed_wrapper(typed_res);
    HttpPipeline typed_pipeline;
    bool next_called = false;
    typed_pipeline.use(vix::middleware::parsers::form());
    typed_pipeline.run(typed_req, typed_wrapper,
                       [&](Request &, Response &response)
                       {
                         next_called = true;
                         response.ok();
                       });
    assert(next_called);
    assert(typed_res.status() == 200);
  }

  auto rejected_req = make_req("a=1", "application/x-www-form");
  vix::http::Response rejected_res;
  vix::http::ResponseWrapper rejected_wrapper(rejected_res);
  HttpPipeline rejected_pipeline;
  bool next_called = false;
  rejected_pipeline.use(vix::middleware::parsers::form());
  rejected_pipeline.run(rejected_req, rejected_wrapper,
                        [&](Request &, Response &)
                        {
                          next_called = true;
                        });
  assert(!next_called);
  assert(rejected_res.status() == 415);

  std::cout << "[OK] form parser\n";
  return 0;
}
