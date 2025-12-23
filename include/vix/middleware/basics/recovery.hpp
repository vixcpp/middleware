#pragma once

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/basics/request_id.hpp> // optional: for request_id in error payload

namespace vix::middleware::basics
{
    struct RecoveryOptions final
    {
        bool include_exception_message{false}; // prod=false, dev=true
        bool include_code_location{false};     // reserved for later (stack/frame)
        std::string code{"internal_server_error"};
        std::string message{"Internal Server Error"};
    };

    // Optional logger contract (if you don't want to depend on vix::utils::Logger directly)
    struct IRecoveryLogger
    {
        virtual ~IRecoveryLogger() = default;
        virtual void error(std::string_view msg) = 0;
    };

    inline void append_if(std::string &out, std::string_view a, std::string_view b)
    {
        if (out.empty())
            out.reserve(a.size() + b.size() + 8);
        out.append(a.data(), a.size());
        out.append(b.data(), b.size());
    }

    inline MiddlewareFn recovery(RecoveryOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next)
        {
            try
            {
                next();
            }
            catch (const std::exception &e)
            {
                // Try to log through DI (optional)
                if (auto log = ctx.services().get<IRecoveryLogger>())
                {
                    std::string msg;
                    msg.reserve(256);
                    msg += "Unhandled exception: ";
                    msg += e.what();
                    msg += " (method=";
                    msg += ctx.req().method();
                    msg += ", path=";
                    msg += ctx.req().path();
                    msg += ")";
                    log->error(msg);
                }

                Error err;
                err.status = 500;
                err.code = opt.code;
                err.message = opt.message;

                if (opt.include_exception_message)
                {
                    err.details["exception"] = e.what();
                }

                ctx.send_error(err);
                return;
            }
            catch (...)
            {
                if (auto log = ctx.services().get<IRecoveryLogger>())
                {
                    std::string msg;
                    msg.reserve(256);
                    msg += "Unhandled non-std exception (method=";
                    msg += ctx.req().method();
                    msg += ", path=";
                    msg += ctx.req().path();
                    msg += ")";
                    log->error(msg);
                }

                Error err;
                err.status = 500;
                err.code = opt.code;
                err.message = opt.message;

                if (opt.include_exception_message)
                {
                    err.details["exception"] = "unknown";
                }

                ctx.send_error(err);
                return;
            }
        };
    }

} // namespace vix::middleware::basics
