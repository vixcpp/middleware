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
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/parsers/multipart.hpp>
#include <vix/middleware/parsers/multipart_save.hpp>

using namespace vix::middleware;

static vix::http::Request make_multipart_probe_req(std::string ct)
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");
  headers.emplace("Content-Type", std::move(ct));

  return vix::http::Request(
      "POST",
      "/mp",
      std::move(headers),
      "----X\r\ncontent\r\n----X--\r\n");
}

static vix::http::Request make_multipart_save_req(
    std::string ct,
    std::string body)
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");
  headers.emplace("Content-Type", std::move(ct));
  headers.emplace("Content-Length", std::to_string(body.size()));

  return vix::http::Request(
      "POST",
      "/upload",
      std::move(headers),
      std::move(body));
}

static void test_multipart_probe()
{
  auto req = make_multipart_probe_req("multipart/form-data; boundary=----X");
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::parsers::multipart());

  p.run(req, w, [&](Request &request, Response &resp)
        {
          auto &info = request.state<vix::middleware::parsers::MultipartInfo>();
          resp.ok().text(info.boundary); });

  assert(res.status() == 200);
  assert(res.body() == "----X");
}

static void test_multipart_content_type_contract()
{
  struct BoundaryCase
  {
    std::string content_type;
    std::string boundary;
  };

  for (const BoundaryCase &test_case : {
           BoundaryCase{"Multipart/Form-Data; boundary= \tplain-boundary \t; charset=utf-8",
                        "plain-boundary"},
           BoundaryCase{"multipart/form-data; boundary= \"quoted boundary\"; charset=utf-8",
                        "quoted boundary"},
       })
  {
    auto req = make_multipart_probe_req(test_case.content_type);
    vix::http::Response res;
    vix::http::ResponseWrapper wrapper(res);
    HttpPipeline pipeline;
    bool next_called = false;
    pipeline.use(vix::middleware::parsers::multipart());
    pipeline.run(req, wrapper,
                 [&](Request &request, Response &response)
                 {
                   next_called = true;
                   assert(request.state<vix::middleware::parsers::MultipartInfo>().boundary ==
                          test_case.boundary);
                   response.ok();
                 });
    assert(next_called);
    assert(res.status() == 200);
  }

  for (const std::string content_type : {
           std::string{"multipart/form-data"},
           std::string{"multipart/form-data; Boundary=case-sensitive"},
       })
  {
    auto req = make_multipart_probe_req(content_type);
    vix::http::Response res;
    vix::http::ResponseWrapper wrapper(res);
    HttpPipeline pipeline;
    bool next_called = false;
    pipeline.use(vix::middleware::parsers::multipart());
    pipeline.run(req, wrapper,
                 [&](Request &, Response &)
                 {
                   next_called = true;
                 });
    assert(!next_called);
    assert(res.status() == 400);
  }

  auto rejected_req = make_multipart_probe_req("text/plain; boundary=ignored");
  vix::http::Response rejected_res;
  vix::http::ResponseWrapper rejected_wrapper(rejected_res);
  HttpPipeline rejected_pipeline;
  bool rejected_next_called = false;
  rejected_pipeline.use(vix::middleware::parsers::multipart());
  rejected_pipeline.run(rejected_req, rejected_wrapper,
                        [&](Request &, Response &)
                        {
                          rejected_next_called = true;
                        });
  assert(!rejected_next_called);
  assert(rejected_res.status() == 415);
}

static void test_multipart_save()
{
  const std::filesystem::path upload_dir =
      std::filesystem::temp_directory_path() / "vix_multipart_save_smoke";

  std::error_code ec;
  std::filesystem::remove_all(upload_dir, ec);
  std::filesystem::create_directories(upload_dir, ec);

  const std::string boundary = "----VIXBOUNDARY";
  const std::string separator = "--" + boundary;

  const std::string body =
      separator + "\r\n"
                  "Content-Disposition: form-data; name=\"title\"\r\n"
                  "\r\n"
                  "hello multipart\r\n" +
      separator + "\r\n"
                  "Content-Disposition: form-data; name=\"file\"; filename=\"hello.txt\"\r\n"
                  "Content-Type: text/plain\r\n"
                  "\r\n"
                  "saved content\r\n" +
      separator + "--\r\n";

  auto req = make_multipart_save_req(
      "Multipart/Form-Data; boundary=" + boundary,
      body);

  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.use(vix::middleware::parsers::multipart_save({
      .max_bytes = 1024 * 1024,
      .max_files = 4,
      .max_file_bytes = 1024 * 1024,
      .upload_dir = upload_dir.string(),
      .create_upload_dir = true,
      .keep_original_filename = true,
      .keep_extension = true,
      .store_in_state = true,
  }));

  p.run(req, w, [&](Request &request, Response &resp)
        {
          auto &form = request.state<vix::middleware::parsers::MultipartForm>();

          auto title = form.fields.find("title");
          assert(title != form.fields.end());
          assert(title->second == "hello multipart");

          assert(form.files.size() == 1);
          assert(form.files[0].field_name == "file");
          assert(form.files[0].filename == "hello.txt");
          assert(form.files[0].content_type == "text/plain");
          assert(form.files[0].bytes == std::string("saved content").size());
          assert(std::filesystem::exists(form.files[0].saved_path));

          std::ifstream in(form.files[0].saved_path, std::ios::binary);
          std::string saved(
              (std::istreambuf_iterator<char>(in)),
              std::istreambuf_iterator<char>());

          assert(saved == "saved content");

          resp.ok().text("saved"); });

  assert(res.status() == 200);
  assert(res.body() == "saved");

  std::filesystem::remove_all(upload_dir, ec);
}

int main()
{
  test_multipart_probe();
  test_multipart_content_type_contract();
  test_multipart_save();

  std::cout << "[OK] multipart parser and save\n";
  return 0;
}
