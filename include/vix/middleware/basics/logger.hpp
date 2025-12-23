#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/basics/request_id.hpp>
#include <vix/middleware/basics/timing.hpp>

namespace vix::middleware::basics
{
    struct ILogger
    {
        virtual ~ILogger() = default;
        virtual void info(std::string_view msg) = 0;
        virtual void warn(std::string_view msg) = 0;
        virtual void error(std::string_view msg) = 0;
    };

    enum class LogFormat
    {
        Text,
        Json
    };

    struct LoggerOptions final
    {
        LogFormat format{LogFormat::Text};
        bool log_request_id{true};
        bool log_timing{true};

        // If true, status >= 400 -> warn/error (>=500)
        bool level_from_status{true};

        // If true, add simple user-agent and remote headers if present
        bool include_user_agent{false};
        bool include_forwarded_for{false};

        // If timing middleware not installed, duration_ms = -1
        bool require_timing{false};
    };

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

    inline std::string build_text_line(const Context &ctx,
                                       int status_code,
                                       std::int64_t duration_ms,
                                       const LoggerOptions &opt)
    {
        std::string line;
        line.reserve(256);

        // method path status duration
        line += ctx.req().method();
        line += " ";
        line += ctx.req().path();
        line += " ";
        line += std::to_string(status_code);

        if (!opt.require_timing || duration_ms >= 0)
        {
            line += " ";
            line += std::to_string(duration_ms);
            line += "ms";
        }

        if (opt.log_request_id)
        {
            if (auto *rid = ctx.req().try_state<RequestId>())
            {
                line += " rid=";
                line += rid->value;
            }
            else
            {
                // fallback: header (if request_id middleware sets it)
                const std::string hdr = ctx.req().header("x-request-id");
                if (!hdr.empty())
                {
                    line += " rid=";
                    line += hdr;
                }
            }
        }

        if (opt.include_user_agent)
        {
            const std::string ua = ctx.req().header("user-agent");
            if (!ua.empty())
            {
                line += " ua=\"";
                line += ua;
                line += "\"";
            }
        }

        if (opt.include_forwarded_for)
        {
            const std::string fwd = ctx.req().header("x-forwarded-for");
            if (!fwd.empty())
            {
                line += " ip=";
                line += fwd;
            }
        }

        return line;
    }

    inline std::string build_json_line(const Context &ctx,
                                       int status_code,
                                       std::int64_t duration_ms,
                                       const LoggerOptions &opt)
    {
        std::string json;
        json.reserve(320);

        json += "{";
        json += "\"method\":\"" + json_escape(ctx.req().method()) + "\",";
        json += "\"path\":\"" + json_escape(ctx.req().path()) + "\",";
        json += "\"status\":" + std::to_string(status_code);

        if (!opt.require_timing || duration_ms >= 0)
        {
            json += ",\"duration_ms\":" + std::to_string(duration_ms);
        }

        if (opt.log_request_id)
        {
            std::string rid;
            if (auto *r = ctx.req().try_state<RequestId>())
                rid = r->value;
            else
                rid = ctx.req().header("x-request-id");

            if (!rid.empty())
            {
                json += ",\"request_id\":\"" + json_escape(rid) + "\"";
            }
        }

        if (opt.include_user_agent)
        {
            const std::string ua = ctx.req().header("user-agent");
            if (!ua.empty())
            {
                json += ",\"user_agent\":\"" + json_escape(ua) + "\"";
            }
        }

        if (opt.include_forwarded_for)
        {
            const std::string fwd = ctx.req().header("x-forwarded-for");
            if (!fwd.empty())
            {
                json += ",\"x_forwarded_for\":\"" + json_escape(fwd) + "\"";
            }
        }

        json += "}";
        return json;
    }

    inline MiddlewareFn logger(LoggerOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next)
        {
            // run handler
            next();

            // Get response status code from underlying Beast response
            const int status_code = static_cast<int>(ctx.res().res.result_int());

            // Duration from Timing middleware
            std::int64_t duration_ms = -1;
            if (opt.log_timing)
            {
                if (auto *t = ctx.req().try_state<Timing>())
                    duration_ms = t->total_ms;
                else if (opt.require_timing)
                    duration_ms = -1;
                else
                    duration_ms = 0;
            }

            // Build message
            std::string msg;
            if (opt.format == LogFormat::Json)
                msg = build_json_line(ctx, status_code, duration_ms, opt);
            else
                msg = build_text_line(ctx, status_code, duration_ms, opt);

            // Send to DI logger if present
            auto log = ctx.services().get<ILogger>();
            if (!log)
                return;

            if (!opt.level_from_status)
            {
                log->info(msg);
                return;
            }

            if (status_code >= 500)
                log->error(msg);
            else if (status_code >= 400)
                log->warn(msg);
            else
                log->info(msg);
        };
    }

} // namespace vix::middleware::basics
