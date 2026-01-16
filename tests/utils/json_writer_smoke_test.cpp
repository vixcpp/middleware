/**
 *
 *  @file json_writer_smoke_test.cpp
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

#include <vix/middleware/utils/json_writer.hpp>

int main()
{
  using namespace vix::middleware::utils;

  JsonWriter w;
  w.begin_obj();
  w.key("ok");
  w.boolean(true);
  w.key("msg");
  w.string("hello");
  w.end_obj();

  auto s = w.str();
  assert(s.find("\"ok\":true") != std::string::npos);
  assert(s.find("\"msg\":\"hello\"") != std::string::npos);

  std::cout << "[OK] json_writer\n";
  return 0;
}
