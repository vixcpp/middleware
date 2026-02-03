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
  /**
   * @brief Deterministic cache key builder utility.
   *
   * KeyBuilder is a small helper used to construct stable string keys from
   * multiple parts (path, query, method, headers, etc.).
   *
   * The format is simple:
   * - parts are separated by '|'
   * - key-value parts can be encoded as "k=v"
   *
   * It also provides a helper to add a subset of request headers, in a stable
   * order, based on a Vary-like list (case-insensitive, normalized to lowercase).
   */
  class KeyBuilder final
  {
  public:
    /**
     * @brief Construct a KeyBuilder with a small pre-reserved buffer.
     */
    KeyBuilder() { s_.reserve(256); }

    /**
     * @brief Add a raw key segment.
     *
     * Segments are separated by '|'.
     *
     * @param part Segment to append.
     * @return Reference to this builder.
     */
    KeyBuilder &add(std::string_view part)
    {
      if (!s_.empty())
        s_ += '|';
      s_.append(part.data(), part.size());
      return *this;
    }

    /**
     * @brief Add a key-value segment encoded as "k=v".
     *
     * Internally this appends "k" as a segment, then adds "=v" in the same segment.
     * The result uses the same '|' delimiter between previous segments and this one.
     *
     * @param k Key.
     * @param v Value.
     * @return Reference to this builder.
     */
    KeyBuilder &add_kv(std::string_view k, std::string_view v)
    {
      add(k);
      s_ += '=';
      s_.append(v.data(), v.size());
      return *this;
    }

    /**
     * @brief Add selected headers in deterministic order.
     *
     * Steps:
     * - normalizes header keys to lowercase
     * - filters to only those present in @p vary (if vary is empty, keeps none)
     * - sorts header names and appends them in order
     *
     * Appended representation:
     * - each header is prefixed by a marker segment "h"
     * - then "name=value" is appended as a kv segment
     *
     * @param headers Header map (copied by value so it can be normalized and filtered).
     * @param vary List of headers to include (Vary semantics).
     * @return Reference to this builder.
     */
    KeyBuilder &add_headers_sorted(
        std::unordered_map<std::string, std::string> headers,
        const std::vector<std::string> &vary)
    {
      normalize_keys_in_place(headers);

      // Only keep vary keys (if vary empty => keep none)
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

    /**
     * @brief Get the built key string.
     *
     * @return Key string.
     */
    std::string str() const { return s_; }

  private:
    std::string s_;
  };

} // namespace vix::middleware::utils

#endif // VIX_KEY_BUILDER_HPP
