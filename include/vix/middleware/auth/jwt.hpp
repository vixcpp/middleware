#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::auth
{
    // Stored into RequestState on success
    struct JwtClaims
    {
        nlohmann::json payload;         // full payload (claims)
        std::string subject;            // payload["sub"] if present
        std::vector<std::string> roles; // payload["roles"] if present (string|array)
    };

    struct JwtOptions
    {
        // Secret for HS256
        std::string secret;

        // Token extraction
        std::string auth_header{"authorization"};
        std::string bearer_prefix{"bearer "}; // case-insensitive
        std::string query_param{};            // optional
        std::function<std::string(const vix::middleware::Request &)> extract{};

        // Validation
        bool verify_exp{true};
        std::string issuer{};   // optional: match "iss"
        std::string audience{}; // optional: match "aud"
    };

    // Small helpers
    inline std::string lower_copy(std::string s)
    {
        for (char &c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    inline bool starts_with_ci(std::string_view s, std::string_view prefix_lower)
    {
        if (s.size() < prefix_lower.size())
            return false;
        for (size_t i = 0; i < prefix_lower.size(); ++i)
        {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
            if (a != prefix_lower[i])
                return false;
        }
        return true;
    }

    inline std::string trim_copy(std::string s)
    {
        auto is_ws = [](unsigned char ch)
        { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; };

        while (!s.empty() && is_ws(static_cast<unsigned char>(s.front())))
            s.erase(s.begin());
        while (!s.empty() && is_ws(static_cast<unsigned char>(s.back())))
            s.pop_back();
        return s;
    }

    // base64url decode (no padding)
    inline std::optional<std::string> b64url_decode(std::string_view in)
    {
        // Convert base64url -> base64
        std::string b64;
        b64.reserve(in.size() + 4);

        for (char c : in)
        {
            if (c == '-')
                b64.push_back('+');
            else if (c == '_')
                b64.push_back('/');
            else
                b64.push_back(c);
        }

        while (b64.size() % 4 != 0)
            b64.push_back('=');

        // OpenSSL EVP decode
        std::string out;
        out.resize((b64.size() * 3) / 4);

        int len = EVP_DecodeBlock(
            reinterpret_cast<unsigned char *>(&out[0]),
            reinterpret_cast<const unsigned char *>(b64.data()),
            static_cast<int>(b64.size()));

        if (len < 0)
            return std::nullopt;

        // Remove padding decoded null bytes based on '=' count
        size_t pad = 0;
        if (!b64.empty() && b64.back() == '=')
            pad++;
        if (b64.size() >= 2 && b64[b64.size() - 2] == '=')
            pad++;

        out.resize(static_cast<size_t>(len) - pad);
        return out;
    }

    inline std::string b64url_encode(const unsigned char *data, size_t len)
    {
        // Base64 encode
        std::string b64;
        b64.resize(4 * ((len + 2) / 3));

        int out_len = EVP_EncodeBlock(
            reinterpret_cast<unsigned char *>(&b64[0]),
            data,
            static_cast<int>(len));

        b64.resize(static_cast<size_t>(out_len));

        // base64 -> base64url, strip '='
        for (char &c : b64)
        {
            if (c == '+')
                c = '-';
            else if (c == '/')
                c = '_';
        }
        while (!b64.empty() && b64.back() == '=')
            b64.pop_back();

        return b64;
    }

    inline std::optional<std::string> hmac_sha256_b64url(std::string_view msg, std::string_view secret)
    {
        unsigned int out_len = 0;
        unsigned char out[EVP_MAX_MD_SIZE];

        if (!HMAC(EVP_sha256(),
                  secret.data(),
                  static_cast<int>(secret.size()),
                  reinterpret_cast<const unsigned char *>(msg.data()),
                  msg.size(),
                  out,
                  &out_len))
        {
            return std::nullopt;
        }

        return b64url_encode(out, static_cast<size_t>(out_len));
    }

    inline std::optional<std::string> extract_token_default(const vix::middleware::Request &req, const JwtOptions &opt)
    {
        // 1) Authorization header
        std::string h = req.header(opt.auth_header);
        h = trim_copy(std::move(h));

        if (!h.empty())
        {
            const std::string prefix_lower = opt.bearer_prefix.empty() ? std::string("bearer ") : lower_copy(opt.bearer_prefix);
            if (starts_with_ci(h, prefix_lower))
            {
                std::string token = h.substr(prefix_lower.size());
                token = trim_copy(std::move(token));
                if (!token.empty())
                    return token;
            }
        }

        // 2) query param (optional)
        if (!opt.query_param.empty())
        {
            auto it = req.query().find(opt.query_param);
            if (it != req.query().end() && !it->second.empty())
                return it->second;
        }

        return std::nullopt;
    }

    inline std::optional<std::string> json_string_claim(const nlohmann::json &j, const char *key)
    {
        if (!j.contains(key))
            return std::nullopt;
        const auto &v = j.at(key);
        if (v.is_string())
            return v.get<std::string>();
        return std::nullopt;
    }

    inline bool claim_matches(const nlohmann::json &payload, const char *key, const std::string &expected)
    {
        if (expected.empty())
            return true;
        auto v = json_string_claim(payload, key);
        return v && *v == expected;
    }

    inline std::vector<std::string> extract_roles(const nlohmann::json &payload)
    {
        std::vector<std::string> roles;

        if (!payload.contains("roles"))
            return roles;

        const auto &r = payload.at("roles");
        if (r.is_string())
        {
            roles.push_back(r.get<std::string>());
            return roles;
        }

        if (r.is_array())
        {
            for (const auto &x : r)
                if (x.is_string())
                    roles.push_back(x.get<std::string>());
        }

        return roles;
    }

    inline bool is_expired(const nlohmann::json &payload)
    {
        if (!payload.contains("exp"))
            return false;

        const auto &exp = payload.at("exp");
        if (!exp.is_number_integer() && !exp.is_number_unsigned())
            return false;

        const std::int64_t exp_s = exp.get<std::int64_t>();
        const std::int64_t now_s = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());

        return now_s >= exp_s;
    }

    // ---------------------------
    // JWT middleware (HS256)
    // ---------------------------
    inline MiddlewareFn jwt(JwtOptions opt)
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            if (opt.secret.empty())
            {
                Error e;
                e.status = 500;
                e.code = "jwt_misconfigured";
                e.message = "JWT secret is empty";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            std::optional<std::string> token;
            if (opt.extract)
                token = opt.extract(ctx.req());
            else
                token = extract_token_default(ctx.req(), opt);

            if (!token || token->empty())
            {
                Error e;
                e.status = 401;
                e.code = "missing_token";
                e.message = "Missing Bearer token";
                ctx.res().header("WWW-Authenticate", "Bearer");
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            // Split header.payload.signature
            const std::string &t = *token;
            const auto p1 = t.find('.');
            const auto p2 = (p1 == std::string::npos) ? std::string::npos : t.find('.', p1 + 1);

            if (p1 == std::string::npos || p2 == std::string::npos)
            {
                Error e;
                e.status = 401;
                e.code = "invalid_token";
                e.message = "Malformed JWT";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            const std::string_view h64 = std::string_view(t).substr(0, p1);
            const std::string_view p64 = std::string_view(t).substr(p1 + 1, p2 - (p1 + 1));
            const std::string_view s64 = std::string_view(t).substr(p2 + 1);

            auto header_raw = b64url_decode(h64);
            auto payload_raw = b64url_decode(p64);

            if (!header_raw || !payload_raw)
            {
                Error e;
                e.status = 401;
                e.code = "invalid_token";
                e.message = "Invalid base64url in JWT";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            nlohmann::json header_json;
            nlohmann::json payload_json;

            try
            {
                header_json = nlohmann::json::parse(*header_raw);
                payload_json = nlohmann::json::parse(*payload_raw);
            }
            catch (...)
            {
                Error e;
                e.status = 401;
                e.code = "invalid_token";
                e.message = "JWT JSON parse failed";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            // alg must be HS256 (v1)
            const auto alg = json_string_claim(header_json, "alg").value_or("");
            if (alg != "HS256")
            {
                Error e;
                e.status = 401;
                e.code = "invalid_token";
                e.message = "Unsupported JWT alg";
                e.details["alg"] = alg.empty() ? "missing" : alg;
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            // verify signature
            const std::string signing_input = t.substr(0, p2); // header.payload
            auto expected_sig = hmac_sha256_b64url(signing_input, opt.secret);
            if (!expected_sig || *expected_sig != std::string(s64))
            {
                Error e;
                e.status = 401;
                e.code = "invalid_token";
                e.message = "JWT signature verification failed";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            // issuer / audience (optional)
            if (!claim_matches(payload_json, "iss", opt.issuer))
            {
                Error e;
                e.status = 401;
                e.code = "invalid_token";
                e.message = "Invalid token issuer";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            // aud can be string or array — v1 simple: string only
            if (!opt.audience.empty())
            {
                bool ok = false;
                if (payload_json.contains("aud"))
                {
                    const auto &aud = payload_json["aud"];
                    if (aud.is_string())
                        ok = (aud.get<std::string>() == opt.audience);
                    else if (aud.is_array())
                    {
                        for (auto const &x : aud)
                            if (x.is_string() && x.get<std::string>() == opt.audience)
                                ok = true;
                    }
                }

                if (!ok)
                {
                    Error e;
                    e.status = 401;
                    e.code = "invalid_token";
                    e.message = "Invalid token audience";
                    ctx.send_error(normalize(std::move(e)));
                    return;
                }
            }

            // exp
            if (opt.verify_exp && is_expired(payload_json))
            {
                Error e;
                e.status = 401;
                e.code = "token_expired";
                e.message = "Token expired";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            JwtClaims claims;
            claims.payload = payload_json;
            claims.subject = json_string_claim(payload_json, "sub").value_or("");
            claims.roles = extract_roles(payload_json);

            ctx.emplace_state<JwtClaims>(std::move(claims));

            next();
        };
    }

} // namespace vix::middleware::auth
