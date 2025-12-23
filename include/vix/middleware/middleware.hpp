#pragma once

#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include <vix/http/RequestHandler.hpp>
#include <vix/middleware/core/next.hpp>
#include <vix/middleware/core/context.hpp>
#include <vix/middleware/core/result.hpp>
#include <vix/middleware/core/hooks.hpp>

namespace vix::middleware
{
    using Request = vix::vhttp::Request;
    using Response = vix::vhttp::ResponseWrapper;
    using NextFn = vix::middleware::NextFn;
    using Error = vix::middleware::Error;

    class Services final
    {
    public:
        Services() = default;

        template <typename T>
        void provide(std::shared_ptr<T> svc)
        {
            data_[std::type_index(typeid(T))] = std::move(svc);
        }

        template <typename T>
        std::shared_ptr<T> get() const
        {
            auto it = data_.find(std::type_index(typeid(T)));
            if (it == data_.end())
                return {};
            return std::static_pointer_cast<T>(it->second);
        }

        template <typename T>
        bool has() const
        {
            return data_.find(std::type_index(typeid(T))) != data_.end();
        }

    private:
        std::unordered_map<std::type_index, std::shared_ptr<void>> data_{};
    };

    using Context = vix::middleware::Context;
    using MiddlewareFn = std::function<void(Context &, Next)>;
    using HttpMiddleware = std::function<void(Request &, Response &, NextFn)>;

    inline MiddlewareFn from_http_middleware(HttpMiddleware legacy)
    {
        return [legacy = std::move(legacy)](Context &ctx, Next next) mutable
        {
            legacy(ctx.req(), ctx.res(), NextFn([next]() mutable
                                                { next(); }));
        };
    }

    inline HttpMiddleware to_http_middleware(MiddlewareFn mw, Services &services)
    {
        return [mw = std::move(mw), &services](Request &req, Response &res, NextFn next) mutable
        {
            Context ctx(req, res, services);
            mw(ctx, Next(std::move(next))); // Next == NextOnce
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
