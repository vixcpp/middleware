#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <vix/middleware/core/context.hpp>
#include <vix/middleware/core/next.hpp>
#include <vix/middleware/middleware.hpp>

namespace vix::middleware::performance
{
    struct StaticFilesOptions
    {
        std::string mount{"/"};               // URL prefix
        std::string index_file{"index.html"}; // directory => index.html
        bool add_cache_control{true};
        std::string cache_control{"public, max-age=3600"};
        bool fallthrough{true}; // if not found => next()
    };

    inline bool starts_with(std::string_view s, std::string_view p)
    {
        return s.size() >= p.size() && s.substr(0, p.size()) == p;
    }

    inline bool contains_dotdot(std::string_view p)
    {
        return p.find("..") != std::string_view::npos;
    }

    inline std::string ext_to_mime(std::string_view ext)
    {
        static const std::unordered_map<std::string, std::string> m{
            {".html", "text/html; charset=utf-8"},
            {".css", "text/css; charset=utf-8"},
            {".js", "application/javascript; charset=utf-8"},
            {".json", "application/json; charset=utf-8"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".svg", "image/svg+xml"},
            {".ico", "image/x-icon"},
            {".txt", "text/plain; charset=utf-8"},
            {".woff", "font/woff"},
            {".woff2", "font/woff2"},
        };

        auto it = m.find(std::string(ext));
        return it == m.end() ? "application/octet-stream" : it->second;
    }

    inline bool read_file_to_string(const std::filesystem::path &p, std::string &out)
    {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            return false;

        f.seekg(0, std::ios::end);
        const std::streamsize n = f.tellg();
        if (n < 0)
            return false;
        f.seekg(0, std::ios::beg);

        out.resize(static_cast<std::size_t>(n));
        if (n > 0)
            f.read(out.data(), n);

        return true;
    }

    inline MiddlewareFn static_files(std::filesystem::path root, StaticFilesOptions opt = {})
    {
        return [root = std::move(root), opt = std::move(opt)](Context &ctx, Next next) mutable
        {
            const auto &m = ctx.req().method();
            if (!(m == "GET" || m == "HEAD"))
            {
                next();
                return;
            }

            const std::string path = ctx.req().path(); // no query
            if (!starts_with(path, opt.mount))
            {
                next();
                return;
            }

            std::string rel = path.substr(opt.mount.size());
            if (!rel.empty() && rel.front() == '/')
                rel.erase(rel.begin());

            if (rel.empty())
                rel = opt.index_file;

            if (contains_dotdot(rel))
            {
                ctx.res().status(400).text("Bad path");
                return;
            }

            std::filesystem::path full = root / rel;

            // directory => index
            if (std::filesystem::is_directory(full))
                full /= opt.index_file;

            if (!std::filesystem::exists(full) || !std::filesystem::is_regular_file(full))
            {
                if (opt.fallthrough)
                {
                    next();
                    return;
                }
                ctx.res().status(404).text("Not Found");
                return;
            }

            std::string body;
            if (!read_file_to_string(full, body))
            {
                ctx.res().status(500).text("Static read error");
                return;
            }

            const std::string mime = ext_to_mime(full.extension().string());
            ctx.res().header("Content-Type", mime);

            if (opt.add_cache_control)
                ctx.res().header("Cache-Control", opt.cache_control);

            // HEAD => no body
            if (m == "HEAD")
            {
                ctx.res().status(200);
                ctx.res().res.body().clear();
                ctx.res().res.prepare_payload();
                return;
            }

            ctx.res().status(200).send(body);
        };
    }

} // namespace vix::middleware::performance
