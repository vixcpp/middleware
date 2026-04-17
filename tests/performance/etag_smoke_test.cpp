/**
 *
 *  @file static_files_smoke_test.cpp
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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/performance/static_files.hpp>

using namespace vix::middleware;

static vix::http::Request make_req(std::string target)
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  return vix::http::Request("GET", std::move(target), std::move(headers), "");
}

int main()
{
  const auto root = std::filesystem::temp_directory_path() / "vix_static_smoke";
  std::filesystem::create_directories(root);
  {
    std::ofstream f(root / "index.html");
    f << "<h1>OK</h1>";
  }

  HttpPipeline p;
  p.use(performance::static_files(root, {.mount = "/", .index_file = "index.html"}));

  auto req = make_req("/");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  p.run(req, w, [&](Request &, Response &resp)
        { resp.status(404).text("nope"); });

  assert(res.status() == 200);
  assert(res.body().find("OK") != std::string::npos);

  std::cout << "[OK] static_files smoke\n";
  return 0;
}
