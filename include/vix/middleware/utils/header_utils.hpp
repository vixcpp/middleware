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
  inline std::string to_lower(std::string s)
  {
    for (char &c : s)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  }

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

  inline std::string trim_copy(std::string_view s)
  {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
      s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
      s.remove_suffix(1);
    return std::string(s);
  }

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

  inline std::string first_token(std::string_view v)
  {
    // For "x-forwarded-for: client, proxy1, proxy2"
    auto comma = v.find(',');
    if (comma != std::string_view::npos)
      v = v.substr(0, comma);
    return trim_copy(v);
  }

} // namespace vix::middleware::utils

#endif
