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
  /**
   * @brief Convert a string to lowercase using ASCII rules.
   *
   * This function performs a character-by-character conversion using
   * std::tolower with an explicit unsigned char cast, making it safe
   * for non-negative char values.
   *
   * It is intended for ASCII-only transformations (e.g. HTTP header
   * normalization) and does not perform locale-aware or UTF-8 case
   * folding.
   *
   * @param s Input string.
   * @return A new string with all characters converted to lowercase.
   */
  inline std::string to_lower_ascii(std::string s)
  {
    for (char &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }
} // namespace vix::middleware::basics::detail

#endif // VIX_ASCII_HPP
