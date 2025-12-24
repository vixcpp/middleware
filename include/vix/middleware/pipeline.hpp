#pragma once

#include <cstddef>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/observability/tracing.hpp>
#include <vix/middleware/observability/metrics.hpp>
#include <vix/middleware/observability/debug_trace.hpp>

namespace vix::middleware
{
    class HttpPipeline
    {
    public:
        using Final = std::function<void(Request &, Response &)>;

        HttpPipeline() = default;
        Services &services() noexcept { return services_; }
        const Services &services() const noexcept { return services_; }
        Hooks &hooks() noexcept { return hooks_; }
        const Hooks &hooks() const noexcept { return hooks_; }

        HttpPipeline &set_hooks(Hooks h)
        {
            hooks_ = std::move(h);
            return *this;
        }

        struct DevObservabilitySinks
        {
            std::shared_ptr<vix::middleware::observability::IMetricsSink> metrics{};
            std::shared_ptr<vix::middleware::observability::IDebugTraceSink> debug{};

            DevObservabilitySinks() = default;
        };

        static bool env_is_dev()
        {
            // Accept: VIX_ENV=dev | development | local
            const char *v = std::getenv("VIX_ENV");
            if (!v || !*v)
                return false;

            std::string s(v);
            for (auto &c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            return (s == "dev" || s == "development" || s == "local");
        }

        HttpPipeline &enable_dev_observability(bool only_if_dev_env = true)
        {
            return enable_dev_observability(DevObservabilitySinks{}, only_if_dev_env);
        }

        HttpPipeline &enable_dev_observability(DevObservabilitySinks sinks,
                                               bool only_if_dev_env = true)
        {
            if (only_if_dev_env && !env_is_dev())
                return *this;

            using namespace vix::middleware::observability;

            if (!sinks.metrics)
                sinks.metrics = std::make_shared<InMemoryMetrics>();
            if (!sinks.debug)
                sinks.debug = std::make_shared<InMemoryDebugTrace>();

            auto h = merge_hooks(
                tracing_hooks(),
                metrics_hooks(sinks.metrics),
                debug_trace_hooks(sinks.debug));

            if (hooks_.on_begin || hooks_.on_end || hooks_.on_error)
                hooks_ = merge_hooks(hooks_, std::move(h));
            else
                hooks_ = std::move(h);

            return *this;
        }

        HttpPipeline &use(HttpMiddleware legacy)
        {
            middlewares_.push_back(from_http_middleware(std::move(legacy)));
            return *this;
        }

        HttpPipeline &use(MiddlewareFn mw)
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

            Context ctx(req, res, const_cast<Services &>(services_));
            if (hooks_.on_begin)
                hooks_.on_begin(ctx);

            std::function<void()> step;

            step = [&]()
            {
                if (idx >= n)
                {
                    if (final_handler)
                        final_handler(req, res);
                    return;
                }

                auto &mw = const_cast<MiddlewareFn &>(middlewares_[idx]);
                ++idx;

                // NextOnce prevents double next()
                mw(ctx, Next(NextFn([&]()
                                    { step(); })));
            };

            try
            {
                step();

                // Hooks end (only if no exception escaped)
                if (hooks_.on_end)
                    hooks_.on_end(ctx);
            }
            catch (const std::exception &e)
            {
                if (hooks_.on_error)
                {
                    Error err;
                    err.status = 500;
                    err.code = "unhandled_exception";
                    err.message = "Unhandled exception escaped middleware pipeline";
                    err.details["what"] = e.what();
                    hooks_.on_error(ctx, normalize(std::move(err)));
                }
                throw;
            }
            catch (...)
            {
                if (hooks_.on_error)
                {
                    Error err;
                    err.status = 500;
                    err.code = "unhandled_exception";
                    err.message = "Unknown exception escaped middleware pipeline";
                    hooks_.on_error(ctx, normalize(std::move(err)));
                }
                throw;
            }
        }

        void run(Request &req, Response &res) const
        {
            run(req, res, Final{});
        }

    private:
        Services services_{};
        Hooks hooks_{};
        std::vector<MiddlewareFn> middlewares_{};
    };

    template <typename Handler>
    auto wrap(Handler handler, HttpPipeline pipeline)
    {
        return [handler = std::move(handler),
                pipeline = std::move(pipeline)](Request &req, Response &res) mutable
        {
            pipeline.run(req, res, [&](Request &r, Response &w)
                         { handler(r, w); });
        };
    }

} // namespace vix::middleware
