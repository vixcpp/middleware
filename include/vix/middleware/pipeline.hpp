#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware
{
    class HttpPipeline
    {
    public:
        using Final = std::function<void(Request &, Response &)>;

        HttpPipeline() = default;

        HttpPipeline &use(HttpMiddleware mw)
        {
            middlewares_.push_back(std::move(mw));
            return *this;
        }

        std::size_t size() const noexcept { return middlewares_.size(); }

        void clear() { middlewares_.clear(); }

        void run(Request &req, Response &res, Final final_handler) const
        {
            const std::size_t n = middlewares_.size();
            std::size_t idx = 0;

            Next next = [&]()
            {
                if (idx >= n)
                {
                    if (final_handler)
                        final_handler(req, res);
                    return;
                }

                auto &mw = const_cast<HttpMiddleware &>(middlewares_[idx]);
                ++idx;
                mw(req, res, next);
            };

            next();
        }

        void run(Request &req, Response &res) const
        {
            run(req, res, Final{});
        }

    private:
        std::vector<HttpMiddleware> middlewares_;
    };

    template <typename Handler>
    auto wrap(Handler handler, HttpPipeline pipeline)
    {
        return [handler = std::move(handler), pipeline = std::move(pipeline)](Request &req, Response &res) mutable
        {
            pipeline.run(req, res, [&](Request &r, Response &w)
                         { handler(r, w); });
        };
    }

} // namespace vix::middleware
