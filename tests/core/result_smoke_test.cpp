/**
 *
 *  @file result_smoke_test.cpp
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

#include <vix/middleware/middleware.hpp>

using namespace vix::middleware;

static void test_ok_value()
{
  auto r = ok<std::string>("hello");
  assert(r.is_ok());
  assert(!r.is_err());
  assert(r.value() == "hello");

  std::cout << "[OK] Result<T>::ok\n";
}

static void test_fail_error()
{
  auto r = fail<std::string>(401, "unauthorized", "Missing token",
                             {{"hint", "Authorization: Bearer <token>"}});
  assert(r.is_err());
  assert(!r.is_ok());
  assert(r.error().status == 401);
  assert(r.error().code == "unauthorized");
  assert(r.error().details["hint"].find("Bearer") != std::string::npos);

  auto j = to_json(r.error());
  assert(j["status"] == 401);
  assert(j["code"] == "unauthorized");

  std::cout << "[OK] Result<T>::fail + to_json\n";
}

static void test_void_ok_and_fail()
{
  auto a = ok();
  assert(a.is_ok());

  auto b = fail<void>(forbidden("nope"));
  assert(b.is_err());
  assert(b.error().status == 403);

  std::cout << "[OK] Result<void>\n";
}

int main()
{
  test_ok_value();
  test_fail_error();
  test_void_ok_and_fail();

  std::cout << "OK: core result smoke tests passed\n";
  return 0;
}
