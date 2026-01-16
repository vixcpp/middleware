/**
 *
 *  @file ascii.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_ASCII_HPP
#define VIX_ASCII_HPP

#include <cctype>
#include <string>
#include <utility>

namespace vix::middleware::basics::detail
{
  inline std::string to_lower_ascii(std::string s)
  {
    for (char &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }
}

#endif
