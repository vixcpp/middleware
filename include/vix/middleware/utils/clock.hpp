/**
 *
 *  @file clock.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_CLOCK_HPP
#define VIX_CLOCK_HPP

#include <chrono>
#include <cstdint>

namespace vix::middleware::utils
{
  struct Clock final
  {
    using Steady = std::chrono::steady_clock;
    using System = std::chrono::system_clock;

    static std::int64_t now_ms_steady()
    {
      using namespace std::chrono;
      return duration_cast<milliseconds>(Steady::now().time_since_epoch()).count();
    }

    static std::int64_t now_ms_epoch()
    {
      using namespace std::chrono;
      return duration_cast<milliseconds>(System::now().time_since_epoch()).count();
    }

    static std::int64_t to_ms(Steady::duration d)
    {
      using namespace std::chrono;
      return duration_cast<milliseconds>(d).count();
    }

    static std::int64_t to_us(Steady::duration d)
    {
      using namespace std::chrono;
      return duration_cast<microseconds>(d).count();
    }
  };

} // namespace vix::middleware::utils

#endif
