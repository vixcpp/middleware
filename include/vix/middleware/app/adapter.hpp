#pragma once

#include <string>
#include <utility>
#include <vix/app/App.hpp>
#include <vix/mw/next.hpp>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::app
{
    inline vix::App::Middleware adapt(vix::middleware::HttpMiddleware inner)
    {
        return [inner = std::move(inner)](vix::Request &req, vix::Response &res, vix::App::Next next) mutable
        {
            vix::mw::Next mw_next(std::move(next));
            inner(req, res, std::move(mw_next));
        };
    }

    // (ex: only GET, only /api/*, etc.)
    template <class Pred>
    inline vix::App::Middleware when(Pred pred, vix::App::Middleware mw)
    {
        return [pred = std::move(pred), mw = std::move(mw)](vix::Request &req, vix::Response &res, vix::App::Next next) mutable
        {
            if (!pred(req))
            {
                next();
                return;
            }
            mw(req, res, std::move(next));
        };
    }

    // Install (global or scoped)
    inline void install(vix::App &app, std::string prefix, vix::App::Middleware mw)
    {
        if (!prefix.empty())
            app.use(std::move(prefix), std::move(mw));
        else
            app.use(std::move(mw));
    }
}
