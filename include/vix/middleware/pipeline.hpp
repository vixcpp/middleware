#pragma once

#include <vector>
#include <cstddef>
#include "vix/middleware/middleware.hpp"

namespace vix::middleware
{

    class Pipeline
    {
    public:
        void use(Middleware mw)
        {
            middlewares_.push_back(std::move(mw));
        }

        template <typename FinalHandler>
        void run(vix::http::Request &req,
                 vix::http::Response &res,
                 FinalHandler &&final_handler) const
        {
            run_impl(0, req, res, std::forward<FinalHandler>(final_handler));
        }

    private:
        template <typename FinalHandler>
        void run_impl(std::size_t index,
                      vix::http::Request &req,
                      vix::http::Response &res,
                      FinalHandler &&final_handler) const
        {
            if (index >= middlewares_.size())
            {
                final_handler(req, res);
                return;
            }

            const auto &current = middlewares_[index];

            Next next = [this, index, &req, &res, &final_handler]()
            {
                run_impl(index + 1, req, res, std::forward<FinalHandler>(final_handler));
            };

            current(req, res, std::move(next));
        }

        std::vector<Middleware> middlewares_;
    };

} // namespace vix::middleware
