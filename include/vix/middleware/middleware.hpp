/**
 *
 *  @file middleware.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_MIDDLEWARE_HPP
#define VIX_MIDDLEWARE_HPP

#include <functional>
#include <utility>

#include <vix/http/RequestHandler.hpp>

#include <vix/mw/next.hpp>
#include <vix/mw/context.hpp>
#include <vix/mw/result.hpp>
#include <vix/mw/hooks.hpp>

namespace vix::middleware
{
  using Request = vix::vhttp::Request;
  using Response = vix::vhttp::ResponseWrapper;
  using NextFn = vix::mw::NextFn;
  using Next = vix::mw::Next;
  using NextOnce = vix::mw::NextOnce;
  using Error = vix::mw::Error;
  using Services = vix::mw::Services;
  using Context = vix::mw::Context;

  /** @brief Middleware function using vix::mw Context and Next. */
  using MiddlewareFn = std::function<void(Context &, Next)>;

  /** @brief Legacy HTTP middleware function signature (Request, Response, Next). */
  using HttpMiddleware = std::function<void(Request &, Response &, Next)>;

  using Hooks = vix::mw::Hooks;
  using vix::mw::bad_request;
  using vix::mw::conflict;
  using vix::mw::fail;
  using vix::mw::forbidden;
  using vix::mw::internal;
  using vix::mw::merge_hooks;
  using vix::mw::normalize;
  using vix::mw::not_found;
  using vix::mw::ok;
  using vix::mw::to_json;
  using vix::mw::unauthorized;

  /**
   * @brief Adapt a legacy HttpMiddleware to a Context-based middleware.
   *
   * @param legacy Legacy middleware (Request, Response, Next).
   * @return MiddlewareFn Context-based middleware wrapper.
   */
  inline MiddlewareFn from_http_middleware(HttpMiddleware legacy)
  {
    return [legacy = std::move(legacy)](Context &ctx, Next next) mutable
    {
      legacy(ctx.req(), ctx.res(), std::move(next));
    };
  }

  /**
   * @brief Adapt a Context-based middleware to a legacy HttpMiddleware.
   *
   * @param mw Context-based middleware.
   * @param services Shared services injected into the Context.
   * @return HttpMiddleware Legacy middleware wrapper.
   */
  inline HttpMiddleware to_http_middleware(MiddlewareFn mw, Services &services)
  {
    return [mw = std::move(mw), &services](Request &req, Response &res, Next next) mutable
    {
      Context ctx(req, res, services);
      mw(ctx, std::move(next));
    };
  }

  /**
   * @brief No-op middleware that simply calls next().
   */
  inline MiddlewareFn noop()
  {
    return [](Context &, Next next)
    { next(); };
  }

  /**
   * @brief Conditionally enable a middleware.
   *
   * If disabled, returns a no-op middleware.
   *
   * @param enabled Whether the middleware is enabled.
   * @param mw Middleware to use when enabled.
   * @return MiddlewareFn Enabled middleware or noop().
   */
  inline MiddlewareFn use_if(bool enabled, MiddlewareFn mw)
  {
    return enabled ? std::move(mw) : noop();
  }

} // namespace vix::middleware

#endif // VIX_MIDDLEWARE_HPP
