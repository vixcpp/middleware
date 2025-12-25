#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>
#ifndef VIX_HAS_ZLIB
#define VIX_HAS_ZLIB 0
#endif

#ifndef VIX_HAS_BROTLI
#define VIX_HAS_BROTLI 0
#endif

// Enable these in your build if libs are available:
//   -DVIX_HAS_ZLIB=1   (and link -lz)
//   -DVIX_HAS_BROTLI=1 (and link -lbrotlienc -lbrotlicommon)

#if defined(VIX_HAS_ZLIB) && VIX_HAS_ZLIB
#include <zlib.h>
#endif

#if defined(VIX_HAS_BROTLI) && VIX_HAS_BROTLI
#include <brotli/encode.h>
#endif

namespace vix::middleware::performance
{
    struct CompressionOptions
    {
        std::size_t min_size{1024}; // compress only if >= 1KB
        bool add_vary{true};
        bool enabled{true};

        // Prefer br over gzip when both are accepted and available
        bool prefer_br{true};

        // Brotli level: 0..11 (reasonable default = 5)
        int brotli_quality{5};

        // Gzip level: 1..9 (reasonable default = 6)
        int gzip_level{6};
    };

    // ---------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------

    inline bool icase_eq(char a, char b)
    {
        auto la = (a >= 'A' && a <= 'Z') ? char(a - 'A' + 'a') : a;
        auto lb = (b >= 'A' && b <= 'Z') ? char(b - 'A' + 'a') : b;
        return la == lb;
    }

    inline bool contains_token_icase(std::string_view hay, std::string_view needle)
    {
        if (needle.empty() || hay.size() < needle.size())
            return false;

        for (std::size_t i = 0; i + needle.size() <= hay.size(); ++i)
        {
            bool ok = true;
            for (std::size_t j = 0; j < needle.size(); ++j)
            {
                if (!icase_eq(hay[i + j], needle[j]))
                {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return true;
        }
        return false;
    }

    // Very small "q=0" detection (if token exists but has ";q=0" nearby, treat as not accepted)
    inline bool token_allowed(std::string_view accept, std::string_view token)
    {
        if (!contains_token_icase(accept, token))
            return false;

        // If we find "token;q=0" (with optional spaces), treat as disabled.
        // Not a full RFC parser, but good enough for v2.
        // Example: "gzip;q=0, br"
        const std::string pat = std::string(token) + ";q=0";
        if (contains_token_icase(accept, pat))
            return false;

        const std::string pat2 = std::string(token) + "; q=0";
        if (contains_token_icase(accept, pat2))
            return false;

        return true;
    }

#if defined(VIX_HAS_ZLIB) && VIX_HAS_ZLIB
    inline bool gzip_compress(const std::string &input, std::string &out, int level)
    {
        // gzip wrapper: deflateInit2(windowBits=15+16) adds gzip header/trailer
        z_stream zs{};
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;

        const int lvl = (level < 1) ? 1 : (level > 9 ? 9 : level);

        if (deflateInit2(&zs, lvl, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            return false;

        zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.data()));
        zs.avail_in = static_cast<uInt>(input.size());

        out.clear();
        out.reserve(input.size() / 2);

        char buffer[16 * 1024];

        int ret = Z_OK;
        while (ret == Z_OK)
        {
            zs.next_out = reinterpret_cast<Bytef *>(buffer);
            zs.avail_out = sizeof(buffer);

            ret = deflate(&zs, zs.avail_in ? Z_NO_FLUSH : Z_FINISH);

            const std::size_t written = sizeof(buffer) - zs.avail_out;
            if (written)
                out.append(buffer, written);
        }

        deflateEnd(&zs);
        return (ret == Z_STREAM_END);
    }
#endif

#if defined(VIX_HAS_BROTLI) && VIX_HAS_BROTLI
    inline bool brotli_compress(const std::string &input, std::string &out, int quality)
    {
        const int q = (quality < 0) ? 0 : (quality > 11 ? 11 : quality);

        size_t out_cap = BrotliEncoderMaxCompressedSize(input.size());
        out.clear();
        out.resize(out_cap);

        size_t out_size = out_cap;

        const BROTLI_BOOL ok = BrotliEncoderCompress(
            q,
            BROTLI_DEFAULT_WINDOW,
            BROTLI_MODE_GENERIC,
            input.size(),
            reinterpret_cast<const uint8_t *>(input.data()),
            &out_size,
            reinterpret_cast<uint8_t *>(out.data()));

        if (!ok)
            return false;

        out.resize(out_size);
        return true;
    }
#endif

    inline bool response_already_encoded(vix::middleware::Response &res)
    {
        auto &raw = res.res;
        return !std::string(raw["Content-Encoding"]).empty();
    }

    inline bool is_compressible_status(int code)
    {
        return (code >= 200 && code < 300);
    }

    inline void set_body_and_length(vix::middleware::Response &res, std::string &&body)
    {
        auto &raw = res.res;
        raw.body() = std::move(body);
        raw.content_length(raw.body().size());
    }

    // ---------------------------------------------------------------------
    // Middleware v2
    // ---------------------------------------------------------------------
    inline MiddlewareFn compression(CompressionOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            if (!opt.enabled)
            {
                next();
                return;
            }

            const std::string ae = ctx.req().header("accept-encoding");

            const bool wants_br = token_allowed(ae, "br");
            const bool wants_gzip = token_allowed(ae, "gzip");

            next();

            auto &res = ctx.res();
            auto &raw = res.res;

            if (opt.add_vary)
                res.append("Vary", "Accept-Encoding");

            if (!is_compressible_status(raw.result_int()))
                return;

            if (response_already_encoded(res))
                return;

            const std::string body = raw.body();
            if (body.size() < opt.min_size)
                return;

            // Choose algorithm
            enum class Algo
            {
                None,
                Br,
                Gzip
            };
            Algo algo = Algo::None;

#if defined(VIX_HAS_BROTLI) && VIX_HAS_BROTLI
            if (opt.prefer_br && wants_br)
                algo = Algo::Br;
#endif
#if defined(VIX_HAS_ZLIB) && VIX_HAS_ZLIB
            if (algo == Algo::None && wants_gzip)
                algo = Algo::Gzip;
#endif
#if defined(VIX_HAS_BROTLI) && VIX_HAS_BROTLI
            if (algo == Algo::None && wants_br)
                algo = Algo::Br;
#endif
            if (algo == Algo::None)
            {
                // Debug always visible: tells why nothing happened
                res.header("X-Vix-Compression-Choice", "none");
                return;
            }

            std::string compressed;

            if (algo == Algo::Gzip)
            {
#if defined(VIX_HAS_ZLIB) && VIX_HAS_ZLIB
                if (!gzip_compress(body, compressed, opt.gzip_level))
                    return;

                res.header("Content-Encoding", "gzip");
                res.header("X-Vix-Compression-Choice", "gzip");
                set_body_and_length(res, std::move(compressed));

#else
                return;
#endif
            }
            else if (algo == Algo::Br)
            {
#if defined(VIX_HAS_BROTLI) && VIX_HAS_BROTLI
                if (!brotli_compress(body, compressed, opt.brotli_quality))
                    return;

                res.header("Content-Encoding", "br");
                res.header("X-Vix-Compression-Choice", "br");
                set_body_and_length(res, std::move(compressed));

#else
                return;
#endif
            }

#ifndef NDEBUG
            // Helps validating behavior in debug mode
            res.header("X-Vix-Compression", "applied");
#endif
        };
    }

} // namespace vix::middleware::performance
