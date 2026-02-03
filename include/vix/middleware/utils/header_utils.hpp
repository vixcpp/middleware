/**
 *
 *  @file header_utils.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_HEADER_UTILS_HPP
#define VIX_HEADER_UTILS_HPP

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vix::middleware::utils
{
  /**
   * @brief Convert a string to lowercase (ASCII only).
   *
   * This helper is intended for HTTP header normalization and does not
   * perform locale-aware or UTF-8 transformations.
   *
   * @param s Input string.
   * @return Lowercased copy of the input.
   */
  inline std::string to_lower(std::string s)
  {
    for (char &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }

  /**
   * @brief Case-insensitive ASCII string comparison.
   *
   * @param a First string.
   * @param b Second string.
   * @return true if strings are equal ignoring ASCII case.
   */
  inline bool iequals(std::string_view a, std::string_view b)
  {
    if (a.size() != b.size())
      return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
      unsigned char ca = static_cast<unsigned char>(a[i]);
      unsigned char cb = static_cast<unsigned char>(b[i]);
      if (ca >= 'A' && ca <= 'Z')
        ca = static_cast<unsigned char>(ca - 'A' + 'a');
      if (cb >= 'A' && cb <= 'Z')
        cb = static_cast<unsigned char>(cb - 'A' + 'a');
      if (ca != cb)
        return false;
    }
    return true;
  }

  /**
   * @brief Trim ASCII whitespace from both ends of a string view.
   *
   * Whitespace includes space, tab, CR and LF.
   *
   * @param s Input string view.
   * @return Trimmed string copy.
   */
  inline std::string trim_copy(std::string_view s)
  {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
      s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
      s.remove_suffix(1);
    return std::string(s);
  }

  /**
   * @brief Split a comma-separated header value into individual tokens.
   *
   * Each token is trimmed of surrounding whitespace. Empty tokens are ignored.
   *
   * @param s CSV string view.
   * @return Vector of trimmed tokens.
   */
  inline std::vector<std::string> split_csv(std::string_view s)
  {
    std::vector<std::string> out;
    while (!s.empty())
    {
      auto comma = s.find(',');
      auto part = (comma == std::string_view::npos) ? s : s.substr(0, comma);
      auto t = trim_copy(part);
      if (!t.empty())
        out.push_back(std::move(t));
      if (comma == std::string_view::npos)
        break;
      s.remove_prefix(comma + 1);
    }
    return out;
  }

  /**
   * @brief Join a list of strings into a comma-separated value.
   *
   * @param a Vector of strings.
   * @return CSV string.
   */
  inline std::string join_csv(const std::vector<std::string> &a)
  {
    std::string out;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
      if (i)
        out += ", ";
      out += a[i];
    }
    return out;
  }

  /**
   * @brief Normalize all keys in a header map to lowercase.
   *
   * Values are moved into a new map with lowercased keys. The input map
   * is replaced in-place.
   *
   * @param h Header map to normalize.
   */
  inline void normalize_keys_in_place(std::unordered_map<std::string, std::string> &h)
  {
    std::unordered_map<std::string, std::string> out;
    out.reserve(h.size());
    for (auto &kv : h)
    {
      out.emplace(to_lower(kv.first), std::move(kv.second));
    }
    h.swap(out);
  }

  /**
   * @brief Extract the first token from a comma-separated value.
   *
   * Commonly used for headers like X-Forwarded-For where the first entry
   * represents the original client.
   *
   * @param v Header value.
   * @return First trimmed token.
   */
  inline std::string first_token(std::string_view v)
  {
    auto comma = v.find(',');
    if (comma != std::string_view::npos)
      v = v.substr(0, comma);
    return trim_copy(v);
  }

} // namespace vix::middleware::utils

#endif // VIX_HEADER_UTILS_HPP
