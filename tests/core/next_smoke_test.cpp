/**
 *
 *  @file next_smoke_test.cpp
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

#include <vix/middleware/middleware.hpp>

using namespace vix::middleware;

static void test_next_calls()
{
  int x = 0;

  Next n(NextFn([&]()
                { x++; }));
  n();

  assert(x == 1);
  std::cout << "[OK] Next calls underlying fn\n";
}

static void test_next_once_prevents_double()
{
  int x = 0;

  NextOnce n(NextFn([&]()
                    { x++; }));

  n();
  n();                           // ignored
  const bool ran = n.try_call(); // also false now

  assert(x == 1);
  assert(ran == false);
  assert(n.called() == true);

  std::cout << "[OK] NextOnce prevents double next()\n";
}

int main()
{
  test_next_calls();
  test_next_once_prevents_double();

  std::cout << "OK: core next smoke tests passed\n";
  return 0;
}
