/**
 *
 *  @file request_id.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_REQUEST_ID_HPP
#define VIX_REQUEST_ID_HPP

#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/basics/detail/ascii.hpp>

namespace vix::middleware::basics
{
  /**
   * @brief Typed request state holding the request id.
   *
   * This is stored in the request context so other middleware/handlers can
   * retrieve it via ctx.req().try_state<RequestId>().
   */
  struct RequestId final
  {
    std::string value;
  };

  /**
   * @brief Configuration options for request_id() middleware.
   */
  struct RequestIdOptions final
  {
    /**
     * @brief Header name used to read/write the request id.
     *
     * The header name is normalized to lowercase ASCII internally.
     */
    std::string header_name{"x-request-id"};

    /**
     * @brief If true, accept a valid incoming request id from the header.
     */
    bool accept_incoming{true};

    /**
     * @brief If true, generate an id when none is present/accepted.
     */
    bool generate_if_missing{true};

    /**
     * @brief If true, always set the response header when an id exists.
     */
    bool always_set_response_header{true};
  };

  /**
   * @brief Validate that a request id is safe and reasonably sized.
   *
   * Allowed characters: [A-Za-z0-9-_.:] with length in [8..128].
   *
   * @param s Candidate request id.
   * @return true if the value is acceptable.
   */
  inline bool is_reasonable_request_id(std::string_view s)
  {
    if (s.size() < 8 || s.size() > 128)
      return false;

    for (char c : s)
    {
      const unsigned char u = static_cast<unsigned char>(c);
      const bool ok =
          (u >= 'a' && u <= 'z') ||
          (u >= 'A' && u <= 'Z') ||
          (u >= '0' && u <= '9') ||
          c == '-' || c == '_' || c == '.' || c == ':';

      if (!ok)
        return false;
    }
    return true;
  }

  /**
   * @brief Convert a 64-bit integer to a 16-char lowercase hex string.
   *
   * @param v Input value.
   * @return 16 hex characters (lowercase).
   */
  inline std::string hex64(std::uint64_t v)
  {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(16);
    for (int i = 15; i >= 0; --i)
    {
      out[static_cast<std::size_t>(i)] = kHex[v & 0xF];
      v >>= 4;
    }
    return out;
  }

  /**
   * @brief Generate a new request id.
   *
   * The id is 32 lowercase hex characters made from:
   * - steady_clock nanoseconds timestamp (mixed)
   * - thread-local random 64-bit value (mixed)
   *
   * @return New request id.
   */
  inline std::string generate_request_id()
  {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

    static thread_local std::mt19937_64 rng{std::random_device{}()};
    const std::uint64_t r = rng();

    const std::uint64_t a = static_cast<std::uint64_t>(ns) ^ (r + 0x9e3779b97f4a7c15ULL);
    const std::uint64_t b = (r << 1) ^ static_cast<std::uint64_t>(ns >> 1);

    std::string id;
    id.reserve(32);
    id += hex64(a);
    id += hex64(b);
    return id;
  }

  /**
   * @brief Request id middleware.
   *
   * Behavior:
   * - Optionally reads an incoming request id from the configured header.
   * - If missing (or invalid), optionally generates a new request id.
   * - Stores the id in typed request state (RequestId).
   * - Optionally writes the id back to the response header.
   *
   * @param opt Middleware options.
   * @return A middleware function (MiddlewareFn).
   */
  inline MiddlewareFn request_id(RequestIdOptions opt = {})
  {
    return [opt = std::move(opt)](Context &ctx, Next next)
    {
      const std::string header = vix::middleware::basics::detail::to_lower_ascii(opt.header_name);
      std::string rid;

      // Try incoming header
      if (opt.accept_incoming && !header.empty())
      {
        const std::string incoming = ctx.req().header(header);
        if (!incoming.empty() && is_reasonable_request_id(incoming))
        {
          rid = incoming;
        }
      }

      // Generate if missing
      if (rid.empty() && opt.generate_if_missing)
      {
        rid = generate_request_id();
      }

      // Store in typed request state
      if (!rid.empty())
      {
        ctx.set_state(RequestId{rid});
      }

      // Always set response header
      if (opt.always_set_response_header && !header.empty() && !rid.empty())
      {
        ctx.res().header(header, rid);
      }

      next();
    };
  }

} // namespace vix::middleware::basics

#endif // VIX_REQUEST_ID_HPP
