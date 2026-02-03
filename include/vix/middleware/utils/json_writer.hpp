/**
 *
 *  @file json_writer.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_JSON_WRITER_HPP
#define VIX_JSON_WRITER_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vix::middleware::utils
{
  /**
   * @brief Escape a string for safe inclusion in a JSON string literal.
   *
   * This performs a minimal JSON escaping:
   * - escapes quotes and backslashes
   * - escapes common control characters (b, f, n, r, t)
   * - replaces other ASCII control chars (< 0x20) with a space
   *
   * @param s Input string view.
   * @return Escaped string.
   */
  inline std::string json_escape(std::string_view s)
  {
    std::string out;
    out.reserve(s.size() + 8);

    for (char c : s)
    {
      switch (c)
      {
      case '\"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20)
          out += ' ';
        else
          out += c;
        break;
      }
    }
    return out;
  }

  /**
   * @brief Tiny incremental JSON string builder.
   *
   * JsonWriter builds a JSON document by appending tokens into an internal string.
   * It supports:
   * - objects and arrays
   * - string, integer, boolean, null values
   * - object_of(map) convenience helper
   *
   * This is intentionally lightweight and does not aim to be a full JSON library.
   * It is useful for small structured payloads and middleware-produced responses.
   */
  class JsonWriter final
  {
  public:
    /**
     * @brief Construct a JsonWriter with a small pre-reserved buffer.
     */
    JsonWriter() { s_.reserve(256); }

    /**
     * @brief Get the current JSON string.
     *
     * @return Built JSON document.
     */
    std::string str() const { return s_; }

    /**
     * @brief Begin a JSON object.
     *
     * Example:
     * @code
     * w.begin_obj();
     * w.key("a"); w.number(1);
     * w.end_obj();
     * @endcode
     */
    void begin_obj()
    {
      sep();
      s_ += "{";
      stack_.push_back('}');
      first_.push_back(true);
    }

    /**
     * @brief End the current JSON object.
     */
    void end_obj()
    {
      s_ += "}";
      stack_.pop_back();
      first_.pop_back();
      mark_value_written();
    }

    /**
     * @brief Begin a JSON array.
     */
    void begin_arr()
    {
      sep();
      s_ += "[";
      stack_.push_back(']');
      first_.push_back(true);
    }

    /**
     * @brief End the current JSON array.
     */
    void end_arr()
    {
      s_ += "]";
      stack_.pop_back();
      first_.pop_back();
      mark_value_written();
    }

    /**
     * @brief Write an object key.
     *
     * This must be called when the current container is an object.
     * The next write must be a value.
     *
     * @param k Key name.
     */
    void key(std::string_view k)
    {
      if (!first_.empty() && !first_.back())
        s_ += ",";
      if (!first_.empty())
        first_.back() = false;

      s_ += "\"";
      s_ += json_escape(k);
      s_ += "\":";
      after_key_ = true;
    }

    /**
     * @brief Write a JSON string value.
     *
     * @param v String value.
     */
    void string(std::string_view v)
    {
      sep();
      s_ += "\"";
      s_ += json_escape(v);
      s_ += "\"";
      mark_value_written();
    }

    /**
     * @brief Write a JSON integer number.
     *
     * @param v Integer value.
     */
    void number(std::int64_t v)
    {
      sep();
      s_ += std::to_string(v);
      mark_value_written();
    }

    /**
     * @brief Write a JSON boolean value.
     *
     * @param v Boolean value.
     */
    void boolean(bool v)
    {
      sep();
      s_ += (v ? "true" : "false");
      mark_value_written();
    }

    /**
     * @brief Write a JSON null value.
     */
    void null()
    {
      sep();
      s_ += "null";
      mark_value_written();
    }

    /**
     * @brief Convenience helper: write an object from a map<string,string>.
     *
     * Keys and values are written as JSON strings.
     *
     * @param m Map to serialize.
     */
    void object_of(const std::unordered_map<std::string, std::string> &m)
    {
      begin_obj();
      for (const auto &kv : m)
      {
        key(kv.first);
        string(kv.second);
      }
      end_obj();
    }

  private:
    /**
     * @brief Insert separators between values when needed.
     *
     * Objects handle commas in key(). Arrays handle commas here.
     */
    void sep()
    {
      if (after_key_)
      {
        after_key_ = false;
        return;
      }

      if (!first_.empty())
      {
        if (stack_.back() == ']' && !first_.back())
          s_ += ",";
      }
    }

    /**
     * @brief Mark that a value has been written in the current array.
     */
    void mark_value_written()
    {
      if (!first_.empty() && stack_.back() == ']')
        first_.back() = false;
    }

  private:
    std::string s_;
    std::vector<char> stack_;
    std::vector<bool> first_;
    bool after_key_{false};
  };

} // namespace vix::middleware::utils

#endif // VIX_JSON_WRITER_HPP
