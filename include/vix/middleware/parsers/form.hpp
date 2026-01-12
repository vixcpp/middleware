#ifndef FORM_HPP
#define FORM_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/helpers/strings/strings.hpp>

namespace vix::middleware::parsers
{
    struct FormBody
    {
        std::unordered_map<std::string, std::string> fields{};
    };

    struct FormParserOptions
    {
        bool require_content_type{true};
        std::size_t max_bytes{0}; // 0 => no limit
        bool store_in_state{true};
    };

    inline std::string url_decode(std::string_view in)
    {
        std::string out;
        out.reserve(in.size());

        auto hex = [](unsigned char ch) -> int
        {
            if (ch >= '0' && ch <= '9')
                return ch - '0';
            if (ch >= 'a' && ch <= 'f')
                return 10 + (ch - 'a');
            if (ch >= 'A' && ch <= 'F')
                return 10 + (ch - 'A');
            return -1;
        };

        for (std::size_t i = 0; i < in.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(in[i]);
            if (c == '+')
            {
                out.push_back(' ');
            }
            else if (c == '%' && i + 2 < in.size())
            {
                int hi = hex(static_cast<unsigned char>(in[i + 1]));
                int lo = hex(static_cast<unsigned char>(in[i + 2]));
                if (hi >= 0 && lo >= 0)
                {
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                }
                else
                {
                    out.push_back(static_cast<char>(c));
                }
            }
            else
            {
                out.push_back(static_cast<char>(c));
            }
        }

        return out;
    }

    inline std::unordered_map<std::string, std::string>
    parse_urlencoded(std::string_view qs)
    {
        std::unordered_map<std::string, std::string> out;

        std::size_t pos = 0;
        while (pos < qs.size())
        {
            std::size_t amp = qs.find('&', pos);
            if (amp == std::string_view::npos)
                amp = qs.size();

            std::string_view pair = qs.substr(pos, amp - pos);
            if (!pair.empty())
            {
                std::size_t eq = pair.find('=');
                std::string_view key, val;
                if (eq == std::string_view::npos)
                {
                    key = pair;
                    val = std::string_view{};
                }
                else
                {
                    key = pair.substr(0, eq);
                    val = pair.substr(eq + 1);
                }

                auto k = url_decode(key);
                auto v = url_decode(val);

                if (!k.empty())
                    out[std::move(k)] = std::move(v);
            }

            if (amp == qs.size())
                break;
            pos = amp + 1;
        }

        return out;
    }

    inline MiddlewareFn form(FormParserOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            auto &req = ctx.req();

            const std::string body = req.body();

            if (opt.max_bytes > 0 && body.size() > opt.max_bytes)
            {
                Error e;
                e.status = 413;
                e.code = "payload_too_large";
                e.message = "Request body exceeds form parser limit";
                e.details["max_bytes"] = std::to_string(opt.max_bytes);
                e.details["got_bytes"] = std::to_string(body.size());
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            if (opt.require_content_type)
            {
                const std::string ct = req.header("content-type");
                if (ct.empty() || !helpers::strings::starts_with_icase(ct, "application/x-www-form-urlencoded"))
                {
                    Error e;
                    e.status = 415;
                    e.code = "unsupported_media_type";
                    e.message = "Content-Type must be application/x-www-form-urlencoded";
                    if (!ct.empty())
                        e.details["content_type"] = ct;
                    ctx.send_error(normalize(std::move(e)));
                    return;
                }
            }

            FormBody fb;
            fb.fields = parse_urlencoded(body);

            if (opt.store_in_state)
                ctx.set_state<FormBody>(std::move(fb));

            next();
        };
    }

} // namespace vix::middleware::parsers

#endif