/**
 *
 *  @file pipeline.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_PIPELINE_HPP
#define VIX_PIPELINE_HPP

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
  /**
   * @brief Lightweight middleware pipeline for HTTP requests.
   *
   * Runs a sequence of middleware functions and an optional final handler.
   * Supports hooks for tracing, metrics and debug tracing.
   */
  class HttpPipeline
  {
  public:
    /** @brief Final handler signature (executed after all middleware). */
    using Final = std::function<void(Request &, Response &)>;

    HttpPipeline() = default;

    /** @brief Access services container (mutable). */
    Services &services() noexcept { return services_; }

    /** @brief Access services container (const). */
    const Services &services() const noexcept { return services_; }

    /** @brief Access pipeline hooks (mutable). */
    Hooks &hooks() noexcept { return hooks_; }

    /** @brief Access pipeline hooks (const). */
    const Hooks &hooks() const noexcept { return hooks_; }

    /**
     * @brief Replace hooks with the provided set.
     */
    HttpPipeline &set_hooks(Hooks h)
    {
      hooks_ = std::move(h);
      return *this;
    }

    /**
     * @brief Optional sinks used by dev observability.
     */
    struct DevObservabilitySinks
    {
      std::shared_ptr<observability::IMetricsSink> metrics{};
      std::shared_ptr<observability::IDebugTraceSink> debug{};

      DevObservabilitySinks() = default;
    };

    /**
     * @brief Return true if VIX_ENV indicates a development environment.
     */
    static bool env_is_dev()
    {
      const char *v = std::getenv("VIX_ENV");
      if (!v || !*v)
        return false;

      std::string s(v);
      for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

      return (s == "dev" || s == "development" || s == "local");
    }

    /**
     * @brief Enable tracing/metrics/debug trace hooks with default in-memory sinks.
     *
     * @param only_if_dev_env If true, only enable when env_is_dev() is true.
     */
    HttpPipeline &enable_dev_observability(bool only_if_dev_env = true)
    {
      return enable_dev_observability(DevObservabilitySinks{}, only_if_dev_env);
    }

    /**
     * @brief Enable tracing/metrics/debug trace hooks with custom sinks.
     *
     * @param sinks Observability sinks (optional).
     * @param only_if_dev_env If true, only enable when env_is_dev() is true.
     */
    HttpPipeline &enable_dev_observability(
        DevObservabilitySinks sinks,
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

    /**
     * @brief Add a legacy HTTP middleware (Request, Response, Next).
     */
    HttpPipeline &use(HttpMiddleware legacy)
    {
      middlewares_.push_back(from_http_middleware(std::move(legacy)));
      return *this;
    }

    /**
     * @brief Add a Context-based middleware.
     */
    HttpPipeline &use(MiddlewareFn mw)
    {
      middlewares_.push_back(std::move(mw));
      return *this;
    }

    /** @brief Number of middleware in the pipeline. */
    std::size_t size() const noexcept { return middlewares_.size(); }

    /** @brief Clear all middleware. */
    void clear() { middlewares_.clear(); }

    /**
     * @brief Run the pipeline and optionally execute a final handler.
     */
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

    /**
     * @brief Run the pipeline with no final handler.
     */
    void run(Request &req, Response &res) const
    {
      run(req, res, Final{});
    }

  private:
    Services services_{};
    Hooks hooks_{};
    std::vector<MiddlewareFn> middlewares_{};
  };

  /**
   * @brief Wrap a handler with a pipeline to produce a (Request, Response) callable.
   */
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

#endif // VIX_PIPELINE_HPP
