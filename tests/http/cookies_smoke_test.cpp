/**
 *
 *  @file cookies_smoke_test.cpp
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
#include <vix/middleware/http/cookies.hpp>

static vix::http::Request make_req_with_cookie(
    std::string cookie_header,
    std::initializer_list<std::pair<std::string, std::string>> extra_headers = {})
{
  vix::http::Request::HeaderMap headers;
  headers.emplace("Host", "localhost");

  if (!cookie_header.empty())
    headers.emplace("cookie", std::move(cookie_header));

  for (const auto &kv : extra_headers)
    headers.emplace(kv.first, kv.second);

  return vix::http::Request("GET", "/x", std::move(headers), "");
}

static std::string get_header_value(const vix::http::Response &res, std::string_view name)
{
  return res.header(name);
}

int main()
{
  // 1) parse + get
  {
    auto req = make_req_with_cookie("a=1; b=hello; theme=dark");

    auto m = vix::middleware::cookies::parse(req);
    assert(m.size() == 3);
    assert(m["a"] == "1");
    assert(m["b"] == "hello");
    assert(m["theme"] == "dark");

    auto a = vix::middleware::cookies::get(req, "a");
    assert(a.has_value());
    assert(*a == "1");

    auto missing = vix::middleware::cookies::get(req, "missing");
    assert(!missing.has_value());
  }

  // 2) whitespace robustness
  {
    auto req = make_req_with_cookie("  a =  1  ;   b= hello  ;c=world ");

    auto a = vix::middleware::cookies::get(req, "a");
    auto b = vix::middleware::cookies::get(req, "b");
    auto c = vix::middleware::cookies::get(req, "c");

    assert(a && *a == "1");
    assert(b && *b == "hello");
    assert(c && *c == "world");
  }

  // 3) set-cookie formatting
  {
    vix::http::Response res;
    vix::http::ResponseWrapper w(res);

    vix::middleware::cookies::Cookie c;
    c.name = "vix.sid";
    c.value = "TOKEN123";
    c.path = "/";
    c.http_only = true;
    c.secure = true;
    c.same_site = "Lax";
    c.max_age = 3600;

    vix::middleware::cookies::set(w, c);

    const std::string set_cookie = get_header_value(res, "Set-Cookie");
    assert(!set_cookie.empty());

    assert(set_cookie.find("vix.sid=TOKEN123") != std::string::npos);
    assert(set_cookie.find("Path=/") != std::string::npos);
    assert(set_cookie.find("Max-Age=3600") != std::string::npos);
    assert(set_cookie.find("HttpOnly") != std::string::npos);
    assert(set_cookie.find("Secure") != std::string::npos);
    assert(set_cookie.find("SameSite=Lax") != std::string::npos);
  }

  // 4) current API replaces previous Set-Cookie value
  {
    vix::http::Response res;
    vix::http::ResponseWrapper w(res);

    vix::middleware::cookies::Cookie a;
    a.name = "a";
    a.value = "1";

    vix::middleware::cookies::Cookie b;
    b.name = "b";
    b.value = "2";

    vix::middleware::cookies::set(w, a);
    vix::middleware::cookies::set(w, b);

    const std::string set_cookie = get_header_value(res, "Set-Cookie");
    assert(!set_cookie.empty());
    assert(set_cookie.find("b=2") != std::string::npos);
    assert(set_cookie.find("a=1") == std::string::npos);
  }

  std::cout << "[OK] cookies\n";
  return 0;
}
