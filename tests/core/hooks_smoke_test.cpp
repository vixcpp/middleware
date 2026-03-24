/**
 *
 *  @file hooks_smoke_test.cpp
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
#include <stdexcept>
#include <string>
#include <utility>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/middleware.hpp>

using namespace vix::middleware;

static vix::vhttp::Request make_req(
    std::string method,
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
  vix::vhttp::Request::HeaderMap map;
  map.emplace("Host", "localhost");

  for (const auto &kv : headers)
    map.emplace(kv.first, kv.second);

  return vix::vhttp::Request(
      std::move(method),
      std::move(target),
      std::move(map),
      "");
}

static void test_hooks_begin_end_called()
{
  auto req = make_req("GET", "/hooks");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  int begin = 0;
  int end = 0;
  int err = 0;

  Hooks h;
  h.on_begin = [&](Context &)
  { begin++; };
  h.on_end = [&](Context &)
  { end++; };
  h.on_error = [&](Context &, const Error &)
  { err++; };

  p.set_hooks(std::move(h));

  p.use([](Context &, Next next)
        { next(); });

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(begin == 1);
  assert(end == 1);
  assert(err == 0);
  assert(res.status() == 200);
  assert(res.body() == "OK");

  std::cout << "[OK] hooks begin/end called\n";
}

static void test_hooks_error_called_when_exception_escapes()
{
  auto req = make_req("GET", "/boom");
  vix::vhttp::Response res;
  vix::vhttp::ResponseWrapper w(res);

  HttpPipeline p;

  int begin = 0;
  int end = 0;
  int err = 0;
  Error last;

  Hooks h;
  h.on_begin = [&](Context &)
  { begin++; };
  h.on_end = [&](Context &)
  { end++; };
  h.on_error = [&](Context &, const Error &e)
  {
    err++;
    last = e;
  };

  p.set_hooks(std::move(h));

  bool threw = false;
  try
  {
    p.run(req, w, [&](Request &, Response &)
          { throw std::runtime_error("kaboom"); });
  }
  catch (...)
  {
    threw = true;
  }

  assert(threw == true);
  assert(begin == 1);
  assert(end == 0);
  assert(err == 1);
  assert(last.status == 500);
  assert(last.code == "unhandled_exception");

  std::cout << "[OK] hooks on_error called when exception escapes\n";
}

int main()
{
  test_hooks_begin_end_called();
  test_hooks_error_called_when_exception_escapes();

  std::cout << "OK: hooks smoke tests passed\n";
  return 0;
}
