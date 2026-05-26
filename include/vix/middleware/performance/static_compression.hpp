/**
 *
 *  @file static_compression.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_STATIC_COMPRESSION_HPP
#define VIX_STATIC_COMPRESSION_HPP

#include <filesystem>
#include <string>
#include <utility>

#include <vix/app/App.hpp>
#include <vix/http/ResponseWrapper.hpp>
#include <vix/middleware/performance/compression.hpp>

namespace vix::middleware::performance
{
  /**
   * @brief Return the request Accept-Encoding header using case-insensitive lookup.
   *
   * Vix request headers may preserve the original header casing depending on
   * the transport/parser layer. HTTP header names are case-insensitive, so this
   * helper checks the common spellings first and then falls back to scanning all
   * request headers.
   *
   * @param req Incoming HTTP request.
   * @return Accept-Encoding header value, or an empty string when missing.
   */
  inline std::string static_accept_encoding(const vix::http::Request &req)
  {
    std::string value = req.header("Accept-Encoding");

    if (!value.empty())
      return value;

    value = req.header("accept-encoding");

    if (!value.empty())
      return value;

    value = req.header("ACCEPT-ENCODING");

    if (!value.empty())
      return value;

    for (const auto &[name, headerValue] : req.headers())
    {
      if (contains_token_icase(name, "accept-encoding"))
        return headerValue;
    }

    return {};
  }

  /**
   * @brief Check whether a static response is eligible for compression.
   *
   * Static compression is skipped for HEAD requests, non-2xx responses,
   * already-encoded responses, and bodies smaller than the configured minimum
   * size.
   *
   * @param req Incoming HTTP request.
   * @param res Response wrapper after the static file has been written.
   * @param minSize Minimum body size required before compression is attempted.
   * @return true if the response can be compressed, false otherwise.
   */
  inline bool static_response_can_compress(
      const vix::http::Request &req,
      const vix::http::ResponseWrapper &res,
      std::size_t minSize)
  {
    if (req.method() == "HEAD")
      return false;

    if (!is_compressible_status(res.res.status()))
      return false;

    if (!res.res.header("Content-Encoding").empty())
      return false;

    return res.res.body().size() >= minSize;
  }

  /**
   * @brief Apply gzip compression to a static response when possible.
   *
   * This function uses the same gzip implementation and Accept-Encoding parsing
   * helpers as the generic compression middleware. Brotli can be added later
   * without changing the core static file API.
   *
   * @param req Incoming HTTP request.
   * @param res Response wrapper containing the static response body.
   * @param opt Compression options.
   */
  inline void compress_static_response(
      const vix::http::Request &req,
      vix::http::ResponseWrapper &res,
      const CompressionOptions &opt)
  {
    if (!opt.enabled)
      return;

    if (!static_response_can_compress(req, res, opt.min_size))
      return;

    if (opt.add_vary)
      res.append("Vary", "Accept-Encoding");

    const std::string acceptEncoding = static_accept_encoding(req);

    if (!token_allowed(acceptEncoding, "gzip"))
    {
#ifndef NDEBUG
      res.header("X-Vix-Static-Compression", "planned");
      res.header("X-Vix-Static-Compression-Choice", "none");
#endif
      return;
    }

#if defined(VIX_HAS_ZLIB) && VIX_HAS_ZLIB
    std::string compressed;

    if (!gzip_compress(res.res.body(), compressed, opt.gzip_level))
      return;

    res.header("Content-Encoding", "gzip");
    res.header("X-Vix-Static-Compression-Choice", "gzip");
    res.res.set_body(std::move(compressed));

#ifndef NDEBUG
    res.header("X-Vix-Static-Compression", "applied");
#endif

#else
#ifndef NDEBUG
    res.header("X-Vix-Static-Compression", "planned");
    res.header("X-Vix-Static-Compression-Choice", "none");
#endif
#endif
  }

  /**
   * @brief Create a static file handler with gzip compression support.
   *
   * The returned handler is designed to be injected into vix::App through
   * vix::App::set_static_handler(). It registers an App middleware that serves
   * files from the configured static directory and compresses successful static
   * responses when the client accepts gzip and zlib support is available.
   *
   * This keeps vix::core independent from vix::middleware:
   *
   * - core owns App::static_dir()
   * - middleware provides the optional compressed static handler
   * - applications opt in through App::set_static_handler()
   *
   * @param opt Compression options.
   * @return vix::App::StaticHandler compatible registration handler.
   */
  inline vix::App::StaticResponseHook compressed_static_response_hook(
      CompressionOptions opt = {})
  {
    return [opt = std::move(opt)](
               const vix::http::Request &req,
               vix::http::ResponseWrapper &res) mutable
    {
      compress_static_response(req, res, opt);
    };
  }

} // namespace vix::middleware::performance

#endif // VIX_STATIC_COMPRESSION_HPP
