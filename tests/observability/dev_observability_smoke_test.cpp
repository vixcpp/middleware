/**
 *
 *  @file dev_observability_smoke_test.cpp
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
#include <cstdlib>
#include <iostream>
#include <string>

#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/pipeline.hpp>

using namespace vix::middleware;

static vix::http::Request make_req()
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  return vix::http::Request("GET", "/dev", std::move(headers), "");
}

namespace
{
  class ScopedVixEnv
  {
  public:
    explicit ScopedVixEnv(const char *value)
    {
      if (const char *current = std::getenv("VIX_ENV"))
      {
        was_set_ = true;
        previous_ = current;
      }

      set(value);
    }

    ~ScopedVixEnv()
    {
      if (was_set_)
      {
        set(previous_.c_str());
      }
      else
      {
#if defined(_WIN32)
        _putenv_s("VIX_ENV", "");
#else
        unsetenv("VIX_ENV");
#endif
      }
    }

    ScopedVixEnv(const ScopedVixEnv &) = delete;
    ScopedVixEnv &operator=(const ScopedVixEnv &) = delete;

    void set(const char *value)
    {
#if defined(_WIN32)
      _putenv_s("VIX_ENV", value);
#else
      setenv("VIX_ENV", value, 1);
#endif
    }

    void unset()
    {
#if defined(_WIN32)
      _putenv_s("VIX_ENV", "");
#else
      unsetenv("VIX_ENV");
#endif
    }

  private:
    bool was_set_{false};
    std::string previous_{};
  };

  void test_env_is_dev_contract()
  {
    ScopedVixEnv environment("");
    environment.unset();
    assert(!HttpPipeline::env_is_dev());

    environment.set("");
    assert(!HttpPipeline::env_is_dev());

    environment.set("dev");
    assert(HttpPipeline::env_is_dev());

    environment.set("DEV");
    assert(HttpPipeline::env_is_dev());

    environment.set("production");
    assert(!HttpPipeline::env_is_dev());

    environment.set("");
    assert(!HttpPipeline::env_is_dev());
  }
} // namespace

int main()
{
  test_env_is_dev_contract();

  ScopedVixEnv environment("dev");

  auto req = make_req();
  vix::http::Response res;
  vix::http::ResponseWrapper w(res);

  HttpPipeline p;
  p.enable_dev_observability();

  p.run(req, w, [&](Request &, Response &resp)
        { resp.ok().text("OK"); });

  assert(res.status() == 200);
  assert(res.body() == "OK");

  assert(res.has_header("x-trace-id"));
  assert(res.has_header("x-span-id"));

  std::cout << "[OK] enable_dev_observability\n";
  return 0;
}
