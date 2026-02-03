/**
 *
 *  @file adapter.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_ADAPTER_HPP
#define VIX_ADAPTER_HPP

#include <string>
#include <utility>

#include <vix/app/App.hpp>
#include <vix/mw/next.hpp>
#include <vix/mw/context.hpp>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::app
{
  /**
   * @brief Adapt a legacy HttpMiddleware to an App middleware.
   *
   * Converts (Request&, Response&, Next) into App::Middleware.
   */
  inline vix::App::Middleware adapt(vix::middleware::HttpMiddleware inner)
  {
    return [inner = std::move(inner)](
               vix::vhttp::Request &req,
               vix::vhttp::ResponseWrapper &res,
               vix::App::Next next) mutable
    {
      vix::mw::Next mw_next(std::move(next));
      inner(req, res, std::move(mw_next));
    };
  }

  /**
   * @brief Adapt a Context-based middleware to an App middleware.
   *
   * Wraps (Context&, Next) and creates a Context instance per request.
   */
  inline vix::App::Middleware adapt_ctx(vix::middleware::MiddlewareFn inner)
  {
    return [inner = std::move(inner),
            services = vix::mw::Services{}](
               vix::vhttp::Request &req,
               vix::vhttp::ResponseWrapper &res,
               vix::App::Next next) mutable
    {
      vix::mw::Context ctx(req, res, services);
      vix::mw::Next mw_next(std::move(next));
      inner(ctx, std::move(mw_next));
    };
  }

  /**
   * @brief Conditionally apply a middleware based on a request predicate.
   *
   * @tparam Pred Callable predicate taking (const Request&) and returning bool.
   * @param pred Predicate to decide whether to run the middleware.
   * @param mw Middleware to run when predicate matches.
   */
  template <class Pred>
  inline vix::App::Middleware when(Pred pred, vix::App::Middleware mw)
  {
    return [pred = std::move(pred),
            mw = std::move(mw)](
               vix::vhttp::Request &req,
               vix::vhttp::ResponseWrapper &res,
               vix::App::Next next) mutable
    {
      if (!pred(req))
      {
        next();
        return;
      }
      mw(req, res, std::move(next));
    };
  }

  /**
   * @brief Apply a middleware only for an exact path.
   */
  inline vix::App::Middleware protect_path(std::string path, vix::App::Middleware mw)
  {
    return when(
        [path = std::move(path)](const vix::vhttp::Request &req)
        {
          return req.path() == path;
        },
        std::move(mw));
  }

  /**
   * @brief Apply a middleware only for a path prefix.
   */
  inline vix::App::Middleware protect_prefix_mw(std::string prefix, vix::App::Middleware mw)
  {
    return when(
        [prefix = std::move(prefix)](const vix::vhttp::Request &req)
        {
          return req.path().rfind(prefix, 0) == 0;
        },
        std::move(mw));
  }

  /**
   * @brief Install a middleware that applies only to an exact path.
   */
  inline void protect(vix::App &app, std::string exact_path, vix::App::Middleware mw)
  {
    app.use(protect_path(std::move(exact_path), std::move(mw)));
  }

  /**
   * @brief Install a middleware that applies only to a path prefix.
   */
  inline void protect_prefix(vix::App &app, std::string prefix, vix::App::Middleware mw)
  {
    app.use(protect_prefix_mw(std::move(prefix), std::move(mw)));
  }

  /**
   * @brief Install a middleware on a route prefix (App::use(prefix, mw)).
   */
  inline void install(vix::App &app, std::string prefix, vix::App::Middleware mw)
  {
    app.use(std::move(prefix), std::move(mw));
  }

  /**
   * @brief Install a middleware on an exact path (alias of protect()).
   */
  inline void install_exact(vix::App &app, std::string exact_path, vix::App::Middleware mw)
  {
    protect(app, std::move(exact_path), std::move(mw));
  }

} // namespace vix::middleware::app

#endif // VIX_ADAPTER_HPP
