/**
 *
 *  @file token_bucket_smoke_test.cpp
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

#include <vix/middleware/utils/token_bucket.hpp>

int main()
{
  using vix::middleware::utils::TokenBucket;

  TokenBucket b(2.0, 0.0);

  assert(b.try_consume(1.0) == true);
  assert(b.try_consume(1.0) == true);
  assert(b.try_consume(1.0) == false);

  std::cout << "[OK] token_bucket basic\n";
  return 0;
}
