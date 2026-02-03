/**
 *
 *  @file recovery.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_RECOVERY_HPP
#define VIX_RECOVERY_HPP

#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/basics/request_id.hpp>

namespace vix::middleware::basics
{
  /**
   * @brief Options for the recovery middleware.
   *
   * recovery catches unhandled exceptions thrown by downstream middleware/handlers
   * and turns them into a normalized 500 error response.
   */
  struct RecoveryOptions final
  {
    bool include_exception_message{false}; // prod=false, dev=true
    bool include_code_location{false};     // reserved for future (file/line)
    std::string code{"internal_server_error"};
    std::string message{"Internal Server Error"};
  };

  /**
   * @brief Optional logger interface used by recovery() to report crashes.
   */
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

  inline std::string request_id_for_log(const Context &ctx)
  {
    if (auto *rid = ctx.req().try_state<RequestId>())
      return rid->value;

    const std::string hdr = ctx.req().header("x-request-id");
    return hdr;
  }

  /**
   * @brief Catch-all recovery middleware.
   *
   * If an exception escapes the pipeline, this middleware:
   * - logs it (if IRecoveryLogger is installed in services)
   * - returns a 500 error with code/message
   * - optionally includes the exception message in details (dev mode)
   */
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
        if (auto log = ctx.services().get<IRecoveryLogger>())
        {
          std::string msg;
          msg.reserve(320);

          msg += "Unhandled exception: ";
          msg += e.what();
          msg += " (method=";
          msg += ctx.req().method();
          msg += ", path=";
          msg += ctx.req().path();

          const std::string rid = request_id_for_log(ctx);
          if (!rid.empty())
          {
            msg += ", rid=";
            msg += rid;
          }

          msg += ")";
          log->error(msg);
        }

        Error err;
        err.status = 500;
        err.code = opt.code;
        err.message = opt.message;

        if (opt.include_exception_message)
          err.details["exception"] = e.what();

        if (opt.include_code_location)
          err.details["location"] = "unavailable";

        ctx.send_error(normalize(std::move(err)));
        return;
      }
      catch (...)
      {
        if (auto log = ctx.services().get<IRecoveryLogger>())
        {
          std::string msg;
          msg.reserve(320);

          msg += "Unhandled non-std exception (method=";
          msg += ctx.req().method();
          msg += ", path=";
          msg += ctx.req().path();

          const std::string rid = request_id_for_log(ctx);
          if (!rid.empty())
          {
            msg += ", rid=";
            msg += rid;
          }

          msg += ")";
          log->error(msg);
        }

        Error err;
        err.status = 500;
        err.code = opt.code;
        err.message = opt.message;

        if (opt.include_exception_message)
          err.details["exception"] = "unknown";

        if (opt.include_code_location)
          err.details["location"] = "unavailable";

        ctx.send_error(normalize(std::move(err)));
        return;
      }
    };
  }

} // namespace vix::middleware::basics

#endif // VIX_RECOVERY_HPP
