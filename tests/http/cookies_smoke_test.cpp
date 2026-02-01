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
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/http/Request.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/http/cookies.hpp>

static vix::vhttp::RawRequest make_req_with_cookie(std::string cookie_header)
{
  namespace http = boost::beast::http;
  vix::vhttp::RawRequest req{http::verb::get, "/x", 11};
  req.set(http::field::host, "localhost");
  if (!cookie_header.empty())
    req.set(http::field::cookie, cookie_header);
  return req;
}

static std::string get_header_value(
    const boost::beast::http::response<boost::beast::http::string_body> &res,
    boost::beast::http::field f)
{
  auto it = res.find(f);
  if (it == res.end())
    return {};
  return std::string(it->value());
}

int main()
{
  namespace http = boost::beast::http;

  // 1) parse + get
  {
    auto raw = make_req_with_cookie("a=1; b=hello; theme=dark");
    vix::vhttp::Request req(raw, {});

    auto m = vix::middleware::http::parse(req);
    assert(m.size() == 3);
    assert(m["a"] == "1");
    assert(m["b"] == "hello");
    assert(m["theme"] == "dark");

    auto a = vix::middleware::http::get(req, "a");
    assert(a.has_value());
    assert(*a == "1");

    auto missing = vix::middleware::http::get(req, "missing");
    assert(!missing.has_value());
  }

  // 2) whitespace robustness
  {
    auto raw = make_req_with_cookie("  a =  1  ;   b= hello  ;c=world ");
    vix::vhttp::Request req(raw, {});

    auto a = vix::middleware::http::get(req, "a");
    auto b = vix::middleware::http::get(req, "b");
    auto c = vix::middleware::http::get(req, "c");

    assert(a && *a == "1");
    assert(b && *b == "hello");
    assert(c && *c == "world");
  }

  // 3) set-cookie formatting
  {
    http::response<http::string_body> res;
    vix::vhttp::ResponseWrapper w(res);

    vix::middleware::http::Cookie c;
    c.name = "vix.sid";
    c.value = "TOKEN123";
    c.path = "/";
    c.http_only = true;
    c.secure = true;
    c.same_site = "Lax";
    c.max_age = 3600;

    vix::middleware::http::set(w, c);

    const std::string set_cookie = get_header_value(res, http::field::set_cookie);
    assert(!set_cookie.empty());

    // minimal checks (order matters because build_set_cookie_value writes sequentially)
    assert(set_cookie.find("vix.sid=TOKEN123") != std::string::npos);
    assert(set_cookie.find("Path=/") != std::string::npos);
    assert(set_cookie.find("Max-Age=3600") != std::string::npos);
    assert(set_cookie.find("HttpOnly") != std::string::npos);
    assert(set_cookie.find("Secure") != std::string::npos);
    assert(set_cookie.find("SameSite=Lax") != std::string::npos);
  }

  // 4) multiple Set-Cookie headers are repeatable (insert)
  {
    http::response<http::string_body> res;
    vix::vhttp::ResponseWrapper w(res);

    vix::middleware::http::Cookie a;
    a.name = "a";
    a.value = "1";

    vix::middleware::http::Cookie b;
    b.name = "b";
    b.value = "2";

    vix::middleware::http::set(w, a);
    vix::middleware::http::set(w, b);

    // Count Set-Cookie occurrences in the raw header sequence
    int count = 0;
    for (auto const &f : res.base())
    {
      if (f.name() == http::field::set_cookie)
        ++count;
    }
    assert(count == 2);
  }

  std::cout << "[OK] cookies\n";
  return 0;
}
