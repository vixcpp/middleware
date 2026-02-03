/**
 *
 *  @file metrics.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_METRICS_HPP
#define VIX_METRICS_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/observability/utils.hpp>

namespace vix::middleware::observability
{
  /**
   * @brief Metrics sink interface.
   *
   * A sink is responsible for recording counters and timing observations.
   * Implementations may export to Prometheus, OpenTelemetry, logs, etc.
   */
  class IMetricsSink
  {
  public:
    virtual ~IMetricsSink() = default;

    /**
     * @brief Increment a counter metric.
     *
     * @param name Metric name.
     * @param labels Metric labels (key/value pairs).
     * @param value Increment value (default: 1).
     */
    virtual void inc_counter(
        std::string_view name,
        std::unordered_map<std::string, std::string> labels = {},
        std::uint64_t value = 1) = 0;

    /**
     * @brief Record an observation expressed in milliseconds.
     *
     * @param name Metric name.
     * @param ms Observed duration (ms).
     * @param labels Metric labels (key/value pairs).
     */
    virtual void observe_ms(
        std::string_view name,
        double ms,
        std::unordered_map<std::string, std::string> labels = {}) = 0;
  };

  /**
   * @brief Simple in-memory metrics sink for tests and local inspection.
   *
   * Stores:
   * - counters by (name + sorted labels)
   * - the last observation for each observation name (count + last value + labels)
   */
  class InMemoryMetrics final : public IMetricsSink
  {
  public:
    /**
     * @brief Normalized label storage type.
     *
     * Labels are stored as a sorted vector to ensure stable hashing and equality.
     */
    using Labels = std::vector<std::pair<std::string, std::string>>;

    /**
     * @brief Convert an unordered map of labels to a sorted vector.
     *
     * Sorting ensures stable hashing and equality comparisons.
     *
     * @param labels Input labels.
     * @return Sorted vector of labels.
     */
    static Labels normalize_labels(std::unordered_map<std::string, std::string> labels)
    {
      Labels out;
      out.reserve(labels.size());
      for (auto &kv : labels)
        out.emplace_back(std::move(kv.first), std::move(kv.second));

      std::sort(out.begin(), out.end(),
                [](auto const &a, auto const &b)
                {
                  if (a.first != b.first)
                    return a.first < b.first;
                  return a.second < b.second;
                });

      return out;
    }

    /**
     * @brief Counter key: metric name + sorted labels.
     */
    struct CounterKey
    {
      std::string name;
      Labels labels; // sorted

      bool operator==(const CounterKey &o) const
      {
        return name == o.name && labels == o.labels;
      }
    };

    /**
     * @brief Hash for CounterKey.
     */
    struct CounterKeyHash
    {
      std::size_t operator()(const CounterKey &k) const noexcept
      {
        std::size_t h = std::hash<std::string>{}(k.name);

        for (auto const &kv : k.labels)
        {
          // stable since labels are sorted
          h ^= std::hash<std::string>{}(kv.first) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
          h ^= std::hash<std::string>{}(kv.second) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        }
        return h;
      }
    };

    /**
     * @brief Increment a counter in memory.
     *
     * Counters are stored under a key composed of metric name and normalized labels.
     */
    void inc_counter(
        std::string_view name,
        std::unordered_map<std::string, std::string> labels = {},
        std::uint64_t value = 1) override
    {
      CounterKey k{std::string(name), normalize_labels(std::move(labels))};

      std::lock_guard<std::mutex> lock(mu_);
      counters_[std::move(k)] += value;
    }

    /**
     * @brief Record an observation (ms) in memory.
     *
     * Stores count and last observed value for the given observation name.
     */
    void observe_ms(
        std::string_view name,
        double ms,
        std::unordered_map<std::string, std::string> labels = {}) override
    {
      std::lock_guard<std::mutex> lock(mu_);
      auto &r = observations_[std::string(name)];
      r.count++;
      r.last_ms = ms;
      r.labels = std::move(labels);
    }

    /**
     * @brief Sum a counter across all label sets for a given metric name.
     *
     * @param name Metric name.
     * @return Sum of all counters that match @p name.
     */
    std::uint64_t counter(std::string_view name) const
    {
      std::lock_guard<std::mutex> lock(mu_);
      std::uint64_t sum = 0;
      for (auto const &kv : counters_)
      {
        if (kv.first.name == name)
          sum += kv.second;
      }
      return sum;
    }

    /**
     * @brief Observation record stored for each observation name.
     */
    struct Obs
    {
      std::uint64_t count{0};
      double last_ms{0};
      std::unordered_map<std::string, std::string> labels{};
    };

    /**
     * @brief Get the last observation for a given observation name.
     *
     * @param name Observation metric name.
     * @return Pointer to the record or nullptr if none exists.
     *
     * @warning The returned pointer becomes invalid if the sink is mutated later.
     * It is intended for quick inspection in tests.
     */
    const Obs *last_observation(std::string_view name) const
    {
      std::lock_guard<std::mutex> lock(mu_);
      auto it = observations_.find(std::string(name));
      if (it == observations_.end())
        return nullptr;
      return &it->second;
    }

  private:
    mutable std::mutex mu_;
    std::unordered_map<CounterKey, std::uint64_t, CounterKeyHash> counters_{};
    std::unordered_map<std::string, Obs> observations_{};
  };

  /**
   * @brief Metrics collection options for HTTP middleware/hooks.
   *
   * The typical metrics names are derived from prefix:
   * - <prefix>_requests_total
   * - <prefix>_responses_total
   * - <prefix>_request_duration_ms
   * - <prefix>_exceptions_total
   */
  struct MetricsOptions
  {
    /**
     * @brief Metric name prefix.
     */
    std::string prefix{"vix_http"};

    /**
     * @brief Include "method" label (GET, POST...).
     */
    bool include_method{true};

    /**
     * @brief Include "path" label.
     *
     * Beware: raw paths may cause cardinality explosions. Prefer a template
     * or sanitized label via path_label.
     */
    bool include_path{false};

    /**
     * @brief Include "status" label on end metrics (200, 404...).
     */
    bool include_status{true};

    /**
     * @brief Optional function to derive a safe, low-cardinality path label.
     *
     * Example: return "/users/:id" instead of "/users/123".
     */
    std::function<std::string(const vix::middleware::Request &)> path_label{};
  };

  /**
   * @brief Typed request state storing start time for duration measurement.
   */
  struct MetricsStartTime
  {
    std::chrono::steady_clock::time_point t0{};
  };

  /**
   * @brief Get a safe path label depending on options.
   *
   * Returns empty if path labels are disabled.
   */
  inline std::string safe_path(
      const vix::middleware::Request &req,
      const MetricsOptions &opt)
  {
    if (!opt.include_path)
      return {};

    if (opt.path_label)
      return opt.path_label(req);

    return req.path();
  }

  /**
   * @brief Build the base labels used for "begin" metrics.
   */
  inline std::unordered_map<std::string, std::string>
  base_labels(
      const vix::middleware::Request &req,
      const MetricsOptions &opt)
  {
    std::unordered_map<std::string, std::string> labels;

    if (opt.include_method)
      labels["method"] = safe_method(req);

    if (opt.include_path)
      labels["path"] = safe_path(req, opt);

    return labels;
  }

  /**
   * @brief Build labels used for "end" metrics (base + status).
   */
  inline std::unordered_map<std::string, std::string>
  end_labels(
      const vix::middleware::Request &req,
      const vix::middleware::Response &res,
      const MetricsOptions &opt)
  {
    auto labels = base_labels(req, opt);

    if (opt.include_status)
    {
      const int sc = static_cast<int>(res.res.result_int());
      labels["status"] = std::to_string(sc);
    }

    return labels;
  }

  /**
   * @brief Build middleware hooks that record request metrics.
   *
   * Hooks can be integrated in a pipeline that supports begin/end/error events.
   *
   * Behavior:
   * - on_begin: records start time and increments "<prefix>_requests_total"
   * - on_end: records duration, increments "<prefix>_responses_total"
   *          and observes "<prefix>_request_duration_ms"
   * - on_error: increments "<prefix>_exceptions_total" with error code/status labels
   *
   * @param sink Metrics sink (shared ownership).
   * @param opt Metrics options.
   * @return Hooks configured for metrics.
   */
  inline vix::middleware::Hooks metrics_hooks(
      std::shared_ptr<IMetricsSink> sink,
      MetricsOptions opt = {})
  {
    vix::middleware::Hooks h;

    auto optp = std::make_shared<MetricsOptions>(std::move(opt));

    h.on_begin = [sink, optp](vix::middleware::Context &ctx)
    {
      if (!sink)
        return;

      ctx.set_state(MetricsStartTime{std::chrono::steady_clock::now()});

      const std::string base = optp->prefix + "_requests_total";
      sink->inc_counter(base, base_labels(ctx.req(), *optp), 1);
    };

    h.on_end = [sink, optp](vix::middleware::Context &ctx)
    {
      if (!sink)
        return;

      auto *st = ctx.try_state<MetricsStartTime>();
      if (!st)
        return;

      const auto t1 = std::chrono::steady_clock::now();
      const double ms = std::chrono::duration<double, std::milli>(t1 - st->t0).count();

      const std::string dur = optp->prefix + "_request_duration_ms";
      sink->observe_ms(dur, ms, end_labels(ctx.req(), ctx.res(), *optp));

      const std::string by_status = optp->prefix + "_responses_total";
      sink->inc_counter(by_status, end_labels(ctx.req(), ctx.res(), *optp), 1);
    };

    h.on_error = [sink, optp](vix::middleware::Context &ctx, const vix::middleware::Error &err)
    {
      if (!sink)
        return;

      auto labels = base_labels(ctx.req(), *optp);
      labels["code"] = err.code;
      labels["status"] = std::to_string(err.status);

      const std::string ex = optp->prefix + "_exceptions_total";
      sink->inc_counter(ex, std::move(labels), 1);
    };

    return h;
  }

  /**
   * @brief Middleware variant that records request metrics.
   *
   * This is a simplified form for pipelines that use MiddlewareFn directly
   * (without explicit hook integration).
   *
   * Behavior:
   * - increments "<prefix>_requests_total" before calling next()
   * - after next(), observes "<prefix>_request_duration_ms"
   * - increments "<prefix>_responses_total"
   *
   * @note Error metrics are not emitted from this function because exceptions
   * and error signaling may be handled elsewhere in the pipeline.
   *
   * @param sink Metrics sink (shared ownership).
   * @param opt Metrics options.
   * @return A middleware function (MiddlewareFn).
   */
  inline vix::middleware::MiddlewareFn metrics_mw(
      std::shared_ptr<IMetricsSink> sink,
      MetricsOptions opt = {})
  {
    auto optp = std::make_shared<MetricsOptions>(std::move(opt));

    return [sink = std::move(sink), optp](
               vix::middleware::Context &ctx,
               vix::middleware::Next next)
    {
      if (!sink)
      {
        next();
        return;
      }

      ctx.set_state(MetricsStartTime{std::chrono::steady_clock::now()});
      sink->inc_counter(optp->prefix + "_requests_total", base_labels(ctx.req(), *optp), 1);

      next();

      auto *st = ctx.try_state<MetricsStartTime>();
      if (!st)
        return;

      const auto t1 = std::chrono::steady_clock::now();
      const double ms = std::chrono::duration<double, std::milli>(t1 - st->t0).count();

      sink->observe_ms(
          optp->prefix + "_request_duration_ms",
          ms,
          end_labels(ctx.req(), ctx.res(), *optp));

      sink->inc_counter(
          optp->prefix + "_responses_total",
          end_labels(ctx.req(), ctx.res(), *optp),
          1);
    };
  }

} // namespace vix::middleware::observability

#endif // VIX_METRICS_HPP
