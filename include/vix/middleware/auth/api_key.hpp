#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <vix/middleware/core/context.hpp>
#include <vix/middleware/core/next.hpp>
#include <vix/middleware/core/result.hpp>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::auth
{
    struct ApiKey
    {
        std::string value;
    };

    struct ApiKeyOptions
    {
        // Where to read API key
        std::string header{"x-api-key"};
        std::string query_param{}; // optional
        bool required{true};

        // Validation
        std::unordered_set<std::string> allowed_keys{};

        // Custom extractor (overrides header/query)
        std::function<std::string(const vix::middleware::Request &)> extract{};

        // Custom validator
        std::function<bool(const std::string &)> validate{};
    };

    inline std::string extract_from_header_or_query(
        const vix::middleware::Request &req,
        const ApiKeyOptions &opt)
    {
        if (!opt.header.empty())
        {
            auto v = req.header(opt.header);
            if (!v.empty())
                return v;
        }

        if (!opt.query_param.empty())
        {
            auto it = req.query().find(opt.query_param);
            if (it != req.query().end())
                return it->second;
        }

        return {};
    }

    inline MiddlewareFn api_key(ApiKeyOptions opt = {})
    {
        return [opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            std::string key;

            if (opt.extract)
                key = opt.extract(ctx.req());
            else
                key = extract_from_header_or_query(ctx.req(), opt);

            if (key.empty())
            {
                if (!opt.required)
                {
                    next();
                    return;
                }

                Error e;
                e.status = 401;
                e.code = "missing_api_key";
                e.message = "API key is required";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            bool ok = true;

            if (!opt.allowed_keys.empty())
                ok = opt.allowed_keys.count(key) != 0;

            if (ok && opt.validate)
                ok = opt.validate(key);

            if (!ok)
            {
                Error e;
                e.status = 403;
                e.code = "invalid_api_key";
                e.message = "Invalid API key";
                ctx.send_error(normalize(std::move(e)));
                return;
            }

            // Inject into request state (typed)
            ctx.emplace_state<ApiKey>(ApiKey{key});

            next();
        };
    }

} // namespace vix::middleware::auth
