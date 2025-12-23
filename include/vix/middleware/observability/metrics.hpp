#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <vix/middleware/middleware.hpp>
#include <vix/middleware/core/hooks.hpp>
#include <vix/middleware/core/result.hpp>
#include <vix/middleware/observability/utils.hpp>

namespace vix::middleware::observability
{
    class IMetricsSink
    {
    public:
        virtual ~IMetricsSink() = default;

        virtual void inc_counter(std::string_view name,
                                 std::unordered_map<std::string, std::string> labels = {},
                                 std::uint64_t value = 1) = 0;

        virtual void observe_ms(std::string_view name,
                                double ms,
                                std::unordered_map<std::string, std::string> labels = {}) = 0;
    };

    class InMemoryMetrics final : public IMetricsSink
    {
    public:
        struct CounterKey
        {
            std::string name;
            std::unordered_map<std::string, std::string> labels;

            bool operator==(const CounterKey &o) const
            {
                return name == o.name && labels == o.labels;
            }
        };

        struct CounterKeyHash
        {
            std::size_t operator()(const CounterKey &k) const noexcept
            {
                std::size_t h = std::hash<std::string>{}(k.name);
                for (auto &kv : k.labels)
                {
                    h ^= std::hash<std::string>{}(kv.first) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                    h ^= std::hash<std::string>{}(kv.second) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                }
                return h;
            }
        };

        void inc_counter(std::string_view name,
                         std::unordered_map<std::string, std::string> labels = {},
                         std::uint64_t value = 1) override
        {
            CounterKey k{std::string(name), std::move(labels)};
            counters_[std::move(k)] += value;
        }

        void observe_ms(std::string_view name,
                        double ms,
                        std::unordered_map<std::string, std::string> labels = {}) override
        {
            auto &r = observations_[std::string(name)];
            r.count++;
            r.last_ms = ms;
            r.labels = std::move(labels);
        }

        std::uint64_t counter(std::string_view name) const
        {
            std::uint64_t sum = 0;
            for (auto &kv : counters_)
                if (kv.first.name == name)
                    sum += kv.second;
            return sum;
        }

        struct Obs
        {
            std::uint64_t count{0};
            double last_ms{0};
            std::unordered_map<std::string, std::string> labels{};
        };

        const Obs *last_observation(std::string_view name) const
        {
            auto it = observations_.find(std::string(name));
            if (it == observations_.end())
                return nullptr;
            return &it->second;
        }

    private:
        std::unordered_map<CounterKey, std::uint64_t, CounterKeyHash> counters_{};
        std::unordered_map<std::string, Obs> observations_{};
    };

    struct MetricsOptions
    {
        std::string prefix{"vix_http"};
        bool include_method{true};
        bool include_path{false};
        bool include_status{true};
        std::function<std::string(const vix::middleware::Request &)> path_label{};
    };

    struct MetricsStartTime
    {
        std::chrono::steady_clock::time_point t0{};
    };

    inline std::string safe_path(const vix::middleware::Request &req,
                                 const MetricsOptions &opt)
    {
        if (!opt.include_path)
            return {};

        if (opt.path_label)
            return opt.path_label(req);

        return req.path();
    }

    inline std::unordered_map<std::string, std::string>
    base_labels(const vix::middleware::Request &req,
                const MetricsOptions &opt)
    {
        std::unordered_map<std::string, std::string> labels;

        if (opt.include_method)
            labels["method"] = safe_method(req);

        if (opt.include_path)
            labels["path"] = safe_path(req, opt);

        return labels;
    }

    inline std::unordered_map<std::string, std::string>
    end_labels(const vix::middleware::Request &req,
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

    inline vix::middleware::Hooks metrics_hooks(std::shared_ptr<IMetricsSink> sink,
                                                MetricsOptions opt = {})
    {
        vix::middleware::Hooks h;

        h.on_begin = [sink, opt = std::move(opt)](vix::middleware::Context &ctx) mutable
        {
            if (!sink)
                return;

            ctx.set_state(MetricsStartTime{std::chrono::steady_clock::now()});

            const std::string base = opt.prefix + "_requests_total";
            sink->inc_counter(base, base_labels(ctx.req(), opt), 1);
        };

        h.on_end = [sink, opt = std::move(opt)](vix::middleware::Context &ctx) mutable
        {
            if (!sink)
                return;

            auto *st = ctx.try_state<MetricsStartTime>();
            if (!st)
                return;

            const auto t1 = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - st->t0).count();

            const std::string dur = opt.prefix + "_request_duration_ms";
            sink->observe_ms(dur, ms, end_labels(ctx.req(), ctx.res(), opt));

            const std::string by_status = opt.prefix + "_responses_total";
            sink->inc_counter(by_status, end_labels(ctx.req(), ctx.res(), opt), 1);
        };

        h.on_error = [sink, opt = std::move(opt)](vix::middleware::Context &ctx,
                                                  const vix::middleware::Error &err) mutable
        {
            if (!sink)
                return;

            auto labels = base_labels(ctx.req(), opt);
            labels["code"] = err.code;
            labels["status"] = std::to_string(err.status);

            const std::string ex = opt.prefix + "_exceptions_total";
            sink->inc_counter(ex, std::move(labels), 1);
        };

        return h;
    }

    inline vix::middleware::MiddlewareFn metrics_mw(std::shared_ptr<IMetricsSink> sink,
                                                    MetricsOptions opt = {})
    {
        return [sink = std::move(sink), opt = std::move(opt)](vix::middleware::Context &ctx,
                                                              vix::middleware::Next next) mutable
        {
            if (!sink)
            {
                next();
                return;
            }

            ctx.set_state(MetricsStartTime{std::chrono::steady_clock::now()});
            sink->inc_counter(opt.prefix + "_requests_total", base_labels(ctx.req(), opt), 1);

            next();

            auto *st = ctx.try_state<MetricsStartTime>();
            if (!st)
                return;

            const auto t1 = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - st->t0).count();

            sink->observe_ms(opt.prefix + "_request_duration_ms", ms, end_labels(ctx.req(), ctx.res(), opt));
            sink->inc_counter(opt.prefix + "_responses_total", end_labels(ctx.req(), ctx.res(), opt), 1);
        };
    }

} // namespace vix::middleware::observability
