#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <string_view>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::observability
{
    inline std::string safe_method(const vix::middleware::Request &req)
    {
        const auto &m = req.method();
        return m.empty() ? std::string("GET") : m;
    }

    inline std::string safe_path(const vix::middleware::Request &req)
    {
        const auto &p = req.path();
        return p.empty() ? std::string("/") : p;
    }
}

#endif