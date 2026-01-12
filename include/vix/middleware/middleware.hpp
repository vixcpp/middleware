#ifndef MIDDLEWARE_HPP
#define MIDDLEWARE_HPP

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
    using MiddlewareFn = std::function<void(Context &, Next)>;
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

    inline MiddlewareFn from_http_middleware(HttpMiddleware legacy)
    {
        return [legacy = std::move(legacy)](Context &ctx, Next next) mutable
        {
            legacy(ctx.req(), ctx.res(), std::move(next));
        };
    }

    inline HttpMiddleware to_http_middleware(MiddlewareFn mw, Services &services)
    {
        return [mw = std::move(mw), &services](Request &req, Response &res, Next next) mutable
        {
            Context ctx(req, res, services);
            mw(ctx, std::move(next));
        };
    }

    inline MiddlewareFn noop()
    {
        return [](Context &, Next next)
        { next(); };
    }

    inline MiddlewareFn use_if(bool enabled, MiddlewareFn mw)
    {
        return enabled ? std::move(mw) : noop();
    }

} // namespace vix::middleware

#endif