#pragma once
#ifndef VIX_MIDDLEWARE_APP_PRESETS_HPP
#define VIX_MIDDLEWARE_APP_PRESETS_HPP

#include <chrono>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <vix/app/App.hpp>
#include <vix/json/Simple.hpp>

#include <vix/middleware/app/adapter.hpp> // adapt_ctx()
#include <vix/middleware/security/cors.hpp>
#include <vix/middleware/security/rate_limit.hpp>

#include <vix/middleware/parsers/form.hpp>
#include <vix/middleware/parsers/json.hpp>
#include <vix/middleware/parsers/multipart.hpp>
#include <vix/middleware/parsers/multipart_save.hpp>

#ifdef VIX_ENABLE_JWT
#include <vix/middleware/auth/jwt.hpp>
#endif

namespace vix::middleware::app
{
    // helpers
    inline std::vector<std::string> _to_vec(std::initializer_list<std::string> xs)
    {
        return {xs.begin(), xs.end()};
    }

    // cors_dev()
    inline vix::App::Middleware cors_dev(
        std::initializer_list<std::string> origins = {
            "http://localhost:5173",
            "http://0.0.0.0:5173"})
    {
        vix::middleware::security::CorsOptions opt{};
        opt.allowed_origins = _to_vec(origins);
        opt.allow_any_origin = false;
        opt.allow_credentials = true;

        opt.allow_headers = {
            "Content-Type",
            "Authorization",
            "X-Requested-With",
            "Accept",
            "Origin"};

        opt.allow_methods = {"GET", "POST", "OPTIONS"};
        opt.expose_headers = {"X-Request-Id"};

        return adapt_ctx(vix::middleware::security::cors(std::move(opt)));
    }

    // rate_limit_dev()
    inline vix::App::Middleware rate_limit_dev(
        std::size_t capacity = 60,
        std::chrono::milliseconds window = std::chrono::minutes(1))
    {
        vix::middleware::security::RateLimitOptions opt{};
        opt.capacity = static_cast<double>(capacity);

        const double seconds =
            (window.count() > 0) ? (static_cast<double>(window.count()) / 1000.0) : 60.0;

        opt.refill_per_sec =
            (seconds > 0.0) ? (static_cast<double>(capacity) / seconds) : 1.0;

        opt.add_headers = true;
        opt.key_header = "x-forwarded-for";

        return adapt_ctx(vix::middleware::security::rate_limit(std::move(opt)));
    }

    // json_dev()  (application/json parser preset)
    inline vix::App::Middleware json_dev(std::size_t max_bytes = 256,
                                         bool require_content_type = true,
                                         bool allow_empty = true)
    {
        vix::middleware::parsers::JsonParserOptions opt{};
        opt.require_content_type = require_content_type;
        opt.allow_empty = allow_empty;
        opt.max_bytes = max_bytes;
        opt.store_in_state = true;

        return adapt_ctx(vix::middleware::parsers::json(std::move(opt)));
    }

    inline vix::App::Middleware json_strict_dev(std::size_t max_bytes = 256)
    {
        return json_dev(max_bytes, true, false);
    }

    // form_dev()  (x-www-form-urlencoded preset)
    inline vix::App::Middleware form_dev(std::size_t max_bytes = 128,
                                         bool require_content_type = true)
    {
        vix::middleware::parsers::FormParserOptions opt{};
        opt.require_content_type = require_content_type;
        opt.max_bytes = max_bytes;
        opt.store_in_state = true;

        return adapt_ctx(vix::middleware::parsers::form(std::move(opt)));
    }

    // multipart_dev() (boundary + max_bytes validation)
    inline vix::App::Middleware multipart_dev(std::size_t max_bytes = 256,
                                              bool require_boundary = true)
    {
        vix::middleware::parsers::MultipartOptions opt{};
        opt.require_boundary = require_boundary;
        opt.max_bytes = max_bytes;
        opt.store_in_state = true;

        return adapt_ctx(vix::middleware::parsers::multipart(std::move(opt)));
    }

    // multipart_save_dev() (save files + MultipartForm in state)
    inline vix::App::Middleware multipart_save_dev(
        std::string upload_dir = "uploads",
        std::size_t max_bytes = 5 * 1024 * 1024
        /* si ton struct supporte: max_files, max_file_bytes => on les ajoutera */
    )
    {
        vix::middleware::parsers::MultipartSaveOptions opt{};
        opt.require_boundary = true;
        opt.max_bytes = max_bytes;
        opt.upload_dir = std::move(upload_dir);
        opt.store_in_state = true;

        return adapt_ctx(vix::middleware::parsers::multipart_save(std::move(opt)));
    }

    // multipart_json() (Simple JSON builder from MultipartForm)
    inline vix::json::kvs multipart_json(const vix::middleware::parsers::MultipartForm &form,
                                         std::string upload_dir = "uploads")
    {
        std::vector<vix::json::token> files_vec;
        files_vec.reserve(form.files.size());

        for (const auto &f : form.files)
        {
            files_vec.emplace_back(vix::json::obj({"field_name", f.field_name,
                                                   "filename", f.filename,
                                                   "content_type", f.content_type,
                                                   "saved_path", f.saved_path,
                                                   "bytes", (long long)f.bytes}));
        }

        std::vector<vix::json::token> fields_flat;
        fields_flat.reserve(form.fields.size() * 2);

        for (const auto &kv : form.fields)
        {
            fields_flat.emplace_back(kv.first);
            fields_flat.emplace_back(kv.second);
        }

        return vix::json::obj(
            {"ok", true,
             "upload_dir", std::move(upload_dir),
             "files_count", (long long)form.files.size(),
             "fields_count", (long long)form.fields.size(),
             "total_bytes", (long long)form.total_bytes,
             "total_files_bytes", (long long)form.total_files_bytes,
             "files", vix::json::array(std::move(files_vec)),
             "fields", vix::json::obj(std::move(fields_flat))});
    }

#ifdef VIX_ENABLE_JWT
    // jwt_auth()
    inline vix::App::Middleware jwt_auth(std::string secret)
    {
        vix::middleware::auth::JwtOptions opt{};
        opt.secret = std::move(secret);
        return adapt_ctx(vix::middleware::auth::jwt(std::move(opt)));
    }

    inline vix::App::Middleware jwt_auth(vix::middleware::auth::JwtOptions opt)
    {
        return adapt_ctx(vix::middleware::auth::jwt(std::move(opt)));
    }
#endif

} // namespace vix::middleware::app

#endif // VIX_MIDDLEWARE_APP_PRESETS_HPP
