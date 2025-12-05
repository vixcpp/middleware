#pragma once

#include <functional>
#include <memory>

namespace vix::http
{
    class Request;
    class Response;
}

namespace vix::middleware
{

    using Next = std::function<void()>;

    using MiddlewareFn = std::function<void(vix::http::Request &, vix::http::Response &, Next)>;

    class Middleware
    {
    public:
        Middleware() = default;
        explicit Middleware(MiddlewareFn fn)
            : fn_(std::move(fn)) {}

        void operator()(vix::http::Request &req,
                        vix::http::Response &res,
                        Next next) const
        {
            if (fn_)
            {
                fn_(req, res, std::move(next));
            }
            else if (next)
            {
                next();
            }
        }

        bool valid() const noexcept { return static_cast<bool>(fn_); }

    private:
        MiddlewareFn fn_;
    };

} // namespace vix::middleware
