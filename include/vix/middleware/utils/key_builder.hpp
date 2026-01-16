/**
 *
 *  @file key_builder.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_KEY_BUILDER_HPP
#define VIX_KEY_BUILDER_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vix/middleware/utils/header_utils.hpp>

namespace vix::middleware::utils
{
  class KeyBuilder final
  {
  public:
    KeyBuilder() { s_.reserve(256); }

    KeyBuilder &add(std::string_view part)
    {
      if (!s_.empty())
        s_ += '|';
      s_.append(part.data(), part.size());
      return *this;
    }

    KeyBuilder &add_kv(std::string_view k, std::string_view v)
    {
      add(k);
      s_ += '=';
      s_.append(v.data(), v.size());
      return *this;
    }

    // headers in deterministic order (by header name)
    KeyBuilder &add_headers_sorted(
        std::unordered_map<std::string, std::string> headers,
        const std::vector<std::string> &vary)
    {
      normalize_keys_in_place(headers);

      // only keep vary keys (if vary empty => keep none)
      if (!vary.empty())
      {
        std::unordered_map<std::string, std::string> filtered;
        filtered.reserve(vary.size());
        for (const auto &k : vary)
        {
          auto lk = to_lower(k);
          auto it = headers.find(lk);
          if (it != headers.end())
            filtered.emplace(std::move(lk), it->second);
        }
        headers.swap(filtered);
      }
      else
      {
        headers.clear();
      }

      // sort keys
      std::vector<std::string> keys;
      keys.reserve(headers.size());
      for (auto &kv : headers)
        keys.push_back(kv.first);

      std::sort(keys.begin(), keys.end());

      for (const auto &k : keys)
      {
        add("h");
        add_kv(k, headers[k]);
      }
      return *this;
    }

    std::string str() const { return s_; }

  private:
    std::string s_;
  };

} // namespace vix::middleware::utils

#endif
