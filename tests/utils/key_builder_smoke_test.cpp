/**
 *
 *  @file key_builder_smoke_test.cpp
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
#include <unordered_map>

#include <vix/middleware/utils/key_builder.hpp>

int main()
{
  using namespace vix::middleware::utils;

  std::unordered_map<std::string, std::string> h{
      {"Accept", "application/json"},
      {"X-Test", "1"}};

  KeyBuilder kb;
  kb.add("GET").add("/api").add_kv("q", "x");
  kb.add_headers_sorted(h, {"Accept"});

  auto key = kb.str();
  assert(key.find("GET") != std::string::npos);
  assert(key.find("h|accept=application/json") != std::string::npos);

  std::cout << "[OK] key_builder\n";
  return 0;
}
