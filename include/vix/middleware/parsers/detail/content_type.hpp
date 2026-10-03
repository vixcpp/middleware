/**
 * @file content_type.hpp
 * @brief Internal HTTP Content-Type parsing helpers for Middleware parsers.
 */
#ifndef VIX_MIDDLEWARE_PARSERS_DETAIL_CONTENT_TYPE_HPP
#define VIX_MIDDLEWARE_PARSERS_DETAIL_CONTENT_TYPE_HPP

#include <cstddef>
#include <string>
#include <string_view>

namespace vix::middleware::parsers::detail
{
  /**
   * Compare an HTTP Content-Type prefix using ASCII-only case folding.
   *
   * This intentionally folds only ASCII A-Z and remains binary-safe through
   * std::string_view. It is parser-local behavior, not a generic text API.
   */
  inline bool content_type_starts_with_icase(
      std::string_view value,
      std::string_view prefix) noexcept
  {
    if (value.size() < prefix.size())
      return false;

    for (std::size_t index = 0; index < prefix.size(); ++index)
    {
      unsigned char left = static_cast<unsigned char>(value[index]);
      unsigned char right = static_cast<unsigned char>(prefix[index]);

      if (left >= 'A' && left <= 'Z')
        left = static_cast<unsigned char>(left - 'A' + 'a');
      if (right >= 'A' && right <= 'Z')
        right = static_cast<unsigned char>(right - 'A' + 'a');

      if (left != right)
        return false;
    }

    return true;
  }

  /**
   * Extract a multipart boundary using the established Middleware behavior.
   *
   * This is deliberately not a complete Content-Type parser: it recognizes
   * only the literal, case-sensitive "boundary=" parameter and performs no
   * quoted-value unescaping.
   */
  inline std::string extract_multipart_boundary(std::string_view content_type)
  {
    std::size_t position = content_type.find("boundary=");
    if (position == std::string_view::npos)
      return {};

    position += std::string_view("boundary=").size();
    if (position >= content_type.size())
      return {};

    std::string_view boundary = content_type.substr(position);

    while (!boundary.empty() && (boundary.front() == ' ' || boundary.front() == '\t'))
      boundary.remove_prefix(1);

    if (!boundary.empty() && boundary.front() == '"')
    {
      boundary.remove_prefix(1);
      const std::size_t closing_quote = boundary.find('"');
      if (closing_quote != std::string_view::npos)
        boundary = boundary.substr(0, closing_quote);
    }
    else
    {
      const std::size_t semicolon = boundary.find(';');
      if (semicolon != std::string_view::npos)
        boundary = boundary.substr(0, semicolon);
    }

    while (!boundary.empty() && (boundary.back() == ' ' || boundary.back() == '\t'))
      boundary.remove_suffix(1);

    return std::string(boundary);
  }
} // namespace vix::middleware::parsers::detail

#endif // VIX_MIDDLEWARE_PARSERS_DETAIL_CONTENT_TYPE_HPP
