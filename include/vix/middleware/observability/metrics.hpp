#ifndef METRICS_HPP
#define METRICS_HPP

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
        using Labels = std::vector<std::pair<std::string, std::string>>;

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

        struct CounterKey
        {
            std::string name;
            Labels labels; // sorted

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

                for (auto const &kv : k.labels)
                {
                    // stable since labels are sorted
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
            CounterKey k{std::string(name), normalize_labels(std::move(labels))};

            std::lock_guard<std::mutex> lock(mu_);
            counters_[std::move(k)] += value;
        }

        void observe_ms(std::string_view name,
                        double ms,
                        std::unordered_map<std::string, std::string> labels = {}) override
        {
            std::lock_guard<std::mutex> lock(mu_);
            auto &r = observations_[std::string(name)];
            r.count++;
            r.last_ms = ms;
            r.labels = std::move(labels);
        }

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

        struct Obs
        {
            std::uint64_t count{0};
            double last_ms{0};
            std::unordered_map<std::string, std::string> labels{};
        };

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

    inline vix::middleware::MiddlewareFn metrics_mw(std::shared_ptr<IMetricsSink> sink,
                                                    MetricsOptions opt = {})
    {
        auto optp = std::make_shared<MetricsOptions>(std::move(opt));

        return [sink = std::move(sink), optp](vix::middleware::Context &ctx,
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

            sink->observe_ms(optp->prefix + "_request_duration_ms", ms, end_labels(ctx.req(), ctx.res(), *optp));
            sink->inc_counter(optp->prefix + "_responses_total", end_labels(ctx.req(), ctx.res(), *optp), 1);
        };
    }

} // namespace vix::middleware::observability

#endif