#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <vix/middleware/middleware.hpp>

namespace vix::middleware::basics
{
    struct Timing final
    {
        std::int64_t total_ms{0};
    };

    struct TimingOptions final
    {
        bool set_x_response_time{true};
        bool set_server_timing{true};

        std::string x_response_time_header{"x-response-time"};
        std::string server_timing_header{"server-timing"};

        std::string server_timing_metric{"total"}; // "total;dur=12"
        bool store_in_state{true};                 // store Timing{ms} in RequestState
    };

    inline std::string to_lower_ascii(std::string s)
    {
        for (char &c : s)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            if (u >= 'A' && u <= 'Z')
                c = static_cast<char>(u - 'A' + 'a');
        }
        return s;
    }

    inline MiddlewareFn timing(TimingOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next)
        {
            using Clock = std::chrono::steady_clock;
            const auto t0 = Clock::now();

            next();

            const auto t1 = Clock::now();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

            if (opt.store_in_state)
            {
                ctx.set_state(Timing{static_cast<std::int64_t>(ms)});
            }

            // If response already finalized, adding headers is still OK in Beast if not sent yet.
            // In your ResponseWrapper, body is set and prepare_payload() is usually called later.
            if (opt.set_x_response_time && !opt.x_response_time_header.empty())
            {
                const std::string h = to_lower_ascii(opt.x_response_time_header);
                ctx.res().header(h, std::to_string(ms) + "ms");
            }

            if (opt.set_server_timing && !opt.server_timing_header.empty())
            {
                const std::string h = to_lower_ascii(opt.server_timing_header);
                std::string value;
                value.reserve(64);
                value += opt.server_timing_metric.empty() ? "total" : opt.server_timing_metric;
                value += ";dur=";
                value += std::to_string(ms);
                ctx.res().header(h, value);
            }
        };
    }

} // namespace vix::middleware::basics
