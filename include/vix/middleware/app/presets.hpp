/**
 *
 *  @file presets.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_MIDDLEWARE_APP_PRESETS_HPP
#define VIX_MIDDLEWARE_APP_PRESETS_HPP

#include <chrono>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <vix/app/App.hpp>
#include <vix/json/Simple.hpp>

#include <vix/middleware/app/adapter.hpp>
#include <vix/middleware/security/cors.hpp>
#include <vix/middleware/security/rate_limit.hpp>
#include <vix/middleware/security/csrf.hpp>
#include <vix/middleware/security/headers.hpp>
#include <vix/middleware/security/ip_filter.hpp>
#include <vix/middleware/basics/body_limit.hpp>

#include <vix/middleware/parsers/form.hpp>
#include <vix/middleware/parsers/json.hpp>
#include <vix/middleware/parsers/multipart.hpp>
#include <vix/middleware/parsers/multipart_save.hpp>
#include <vix/middleware/auth/jwt.hpp>
#include <vix/middleware/auth/api_key.hpp>
#include <vix/middleware/auth/rbac.hpp>
#include <vix/middleware/app/app_middleware.hpp>
#include <vix/middleware/http/cookies.hpp>
#include <vix/middleware/auth/session.hpp>

namespace vix::middleware::app
{
  /**
   * @brief Convert an initializer list of strings to a std::vector.
   */
  inline std::vector<std::string> _to_vec(std::initializer_list<std::string> xs)
  {
    return {xs.begin(), xs.end()};
  }

  /**
   * @brief Body size limit preset (dev-friendly defaults).
   *
   * Adapts the Context-based middleware to App middleware.
   */
  inline vix::App::Middleware body_limit_dev(
      std::size_t max_bytes = 16,
      bool apply_to_get = false,
      bool allow_chunked = true,
      std::function<bool(const vix::middleware::Context &)> should_apply = {})
  {
    vix::middleware::basics::BodyLimitOptions opt{};
    opt.max_bytes = max_bytes;
    opt.apply_to_get = apply_to_get;
    opt.allow_chunked = allow_chunked;
    opt.should_apply = std::move(should_apply);

    return adapt_ctx(vix::middleware::basics::body_limit(std::move(opt)));
  }

  /**
   * @brief Body limit preset that applies only to write methods (POST/PUT/PATCH).
   */
  inline vix::App::Middleware body_limit_write_dev(std::size_t max_bytes)
  {
    return body_limit_dev(
        max_bytes, false, true,
        [](const vix::middleware::Context &ctx)
        {
          const auto m = ctx.req().method();
          return (m == "POST" || m == "PUT" || m == "PATCH");
        });
  }

  /**
   * @brief IP filter preset with explicit allow and deny lists.
   */
  inline vix::App::Middleware ip_filter_allow_deny_dev(
      std::string header_name,
      std::initializer_list<std::string> allow,
      std::initializer_list<std::string> deny,
      bool use_remote_addr_fallback = true)
  {
    vix::middleware::security::IpFilterOptions opt{};
    opt.header_name = std::move(header_name);
    opt.allow = _to_vec(allow);
    opt.deny = _to_vec(deny);
    opt.use_remote_addr_fallback = use_remote_addr_fallback;

    return adapt_ctx(vix::middleware::security::ip_filter(std::move(opt)));
  }

  /**
   * @brief IP filter preset with a deny list.
   */
  inline vix::App::Middleware ip_filter_dev(
      std::string header_name = "x-vix-ip",
      std::initializer_list<std::string> deny = {"1.2.3.4"},
      bool use_remote_addr_fallback = true)
  {
    vix::middleware::security::IpFilterOptions opt{};
    opt.header_name = std::move(header_name);
    opt.deny = _to_vec(deny);
    opt.use_remote_addr_fallback = use_remote_addr_fallback;

    return adapt_ctx(vix::middleware::security::ip_filter(std::move(opt)));
  }

  /**
   * @brief CORS preset for demo setups that also expects an IP header.
   */
  inline vix::App::Middleware cors_ip_demo(
      std::initializer_list<std::string> origins = {
          "http://localhost:5173",
          "http://0.0.0.0:5173",
          "https://example.com"})
  {
    vix::middleware::security::CorsOptions opt{};
    opt.allowed_origins = _to_vec(origins);
    opt.allow_any_origin = false;
    opt.allow_credentials = true;

    opt.allow_headers = {
        "Content-Type",
        "Authorization",
        "X-Requested-With",
        "X-Vix-Ip",
        "Accept",
        "Origin"};

    opt.allow_methods = {"GET", "POST", "OPTIONS"};
    opt.expose_headers = {"X-Request-Id"};

    return adapt_ctx(vix::middleware::security::cors(std::move(opt)));
  }

  /**
   * @brief IP allowlist preset.
   */
  inline vix::App::Middleware ip_allowlist_dev(
      std::string header_name,
      std::initializer_list<std::string> allow,
      bool use_remote_addr_fallback = true)
  {
    vix::middleware::security::IpFilterOptions opt{};
    opt.header_name = std::move(header_name);
    opt.allow = _to_vec(allow);
    opt.use_remote_addr_fallback = use_remote_addr_fallback;

    return adapt_ctx(vix::middleware::security::ip_filter(std::move(opt)));
  }

  /**
   * @brief Custom rate-limit preset.
   */
  inline vix::App::Middleware rate_limit_custom_dev(
      double capacity,
      double refill_per_sec,
      std::string key_header = "x-forwarded-for",
      bool add_headers = true)
  {
    vix::middleware::security::RateLimitOptions opt{};
    opt.capacity = capacity;
    opt.refill_per_sec = refill_per_sec;
    opt.add_headers = add_headers;
    opt.key_header = std::move(key_header);
    return adapt_ctx(vix::middleware::security::rate_limit(std::move(opt)));
  }

  /**
   * @brief Security headers preset.
   */
  inline vix::App::Middleware security_headers_dev(bool hsts = false)
  {
    vix::middleware::security::SecurityHeadersOptions opt{};
    opt.x_content_type_options = true;
    opt.x_frame_options = true;
    opt.referrer_policy = true;
    opt.permissions_policy = true;
    opt.hsts = hsts;

    return adapt_ctx(vix::middleware::security::headers(std::move(opt)));
  }

  /**
   * @brief CSRF protection preset using cookie + header token.
   */
  inline vix::App::Middleware csrf_dev(
      std::string cookie_name = "csrf_token",
      std::string header_name = "x-csrf-token",
      bool protect_get = false)
  {
    vix::middleware::security::CsrfOptions opt{};
    opt.cookie_name = std::move(cookie_name);
    opt.header_name = std::move(header_name);
    opt.protect_get = protect_get;

    return adapt_ctx(vix::middleware::security::csrf(std::move(opt)));
  }

  /**
   * @brief Strict CSRF preset that also protects GET requests.
   */
  inline vix::App::Middleware csrf_strict_dev(
      std::string cookie_name = "csrf_token",
      std::string header_name = "x-csrf-token")
  {
    return csrf_dev(std::move(cookie_name), std::move(header_name), true);
  }

  /**
   * @brief Install a middleware on a route prefix.
   *
   * @param app Target app.
   * @param prefix Route prefix.
   * @param mw Middleware to install.
   */
  inline void use_on_prefix(vix::App &app, std::string prefix, vix::App::Middleware mw)
  {
    install(app, std::move(prefix), std::move(mw));
  }

  /**
   * @brief CORS preset for local development.
   */
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

  /**
   * @brief Rate-limit preset defined by capacity and time window.
   */
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

  /**
   * @brief JSON parser preset.
   */
  inline vix::App::Middleware json_dev(
      std::size_t max_bytes = 256,
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

  /**
   * @brief Strict JSON parser preset (no empty body).
   */
  inline vix::App::Middleware json_strict_dev(std::size_t max_bytes = 256)
  {
    return json_dev(max_bytes, true, false);
  }

  /**
   * @brief Form parser preset.
   */
  inline vix::App::Middleware form_dev(
      std::size_t max_bytes = 128,
      bool require_content_type = true)
  {
    vix::middleware::parsers::FormParserOptions opt{};
    opt.require_content_type = require_content_type;
    opt.max_bytes = max_bytes;
    opt.store_in_state = true;

    return adapt_ctx(vix::middleware::parsers::form(std::move(opt)));
  }

  /**
   * @brief Multipart parser preset.
   */
  inline vix::App::Middleware multipart_dev(
      std::size_t max_bytes = 256,
      bool require_boundary = true)
  {
    vix::middleware::parsers::MultipartOptions opt{};
    opt.require_boundary = require_boundary;
    opt.max_bytes = max_bytes;
    opt.store_in_state = true;

    return adapt_ctx(vix::middleware::parsers::multipart(std::move(opt)));
  }

  /**
   * @brief Multipart parser preset that saves uploaded files.
   */
  inline vix::App::Middleware multipart_save_dev(
      std::string upload_dir = "uploads",
      std::size_t max_bytes = 5 * 1024 * 1024)
  {
    vix::middleware::parsers::MultipartSaveOptions opt{};
    opt.require_boundary = true;
    opt.max_bytes = max_bytes;
    opt.upload_dir = std::move(upload_dir);
    opt.store_in_state = true;

    return adapt_ctx(vix::middleware::parsers::multipart_save(std::move(opt)));
  }

  /**
   * @brief Build a JSON response from a parsed multipart form.
   *
   * @param form Parsed multipart form.
   * @param upload_dir Upload directory used by the saver.
   */
  inline vix::json::kvs multipart_json(
      const vix::middleware::parsers::MultipartForm &form,
      std::string upload_dir = "uploads")
  {
    std::vector<vix::json::token> files_vec;
    files_vec.reserve(form.files.size());

    for (const auto &f : form.files)
    {
      files_vec.emplace_back(
          vix::json::obj({"field_name", f.field_name,
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

  /**
   * @brief JWT auth middleware preset.
   */
  inline vix::App::Middleware jwt_auth(std::string secret)
  {
    vix::middleware::auth::JwtOptions opt{};
    opt.secret = std::move(secret);
    return adapt_ctx(vix::middleware::auth::jwt(std::move(opt)));
  }

  /**
   * @brief JWT auth middleware preset with explicit options.
   */
  inline vix::App::Middleware jwt_auth(vix::middleware::auth::JwtOptions opt)
  {
    return adapt_ctx(vix::middleware::auth::jwt(std::move(opt)));
  }

  /**
   * @brief JWT preset with optional expiration verification.
   */
  inline vix::App::Middleware jwt_dev(
      std::string secret,
      bool verify_exp = false)
  {
    vix::middleware::auth::JwtOptions opt{};
    opt.secret = std::move(secret);
    opt.verify_exp = verify_exp;

    return adapt_ctx(vix::middleware::auth::jwt(std::move(opt)));
  }

  /**
   * @brief Session middleware preset using signed cookies.
   */
  inline vix::App::Middleware session_dev(
      std::string secret,
      std::string cookie_name = "sid",
      std::chrono::seconds ttl = std::chrono::hours(24 * 7),
      bool secure = false,
      std::string same_site = "Lax",
      bool http_only = true,
      std::string cookie_path = "/",
      bool auto_create = true)
  {
    vix::middleware::auth::SessionOptions opt{};
    opt.secret = std::move(secret);
    opt.cookie_name = std::move(cookie_name);
    opt.cookie_path = std::move(cookie_path);
    opt.secure = secure;
    opt.http_only = http_only;
    opt.same_site = std::move(same_site);
    opt.ttl = ttl;
    opt.auto_create = auto_create;

    return adapt_ctx(vix::middleware::auth::session(std::move(opt)));
  }

  /**
   * @brief Strict session preset (Secure + SameSite=None for cross-site cookies).
   */
  inline vix::App::Middleware session_strict(
      std::string secret,
      std::string cookie_name = "sid",
      std::chrono::seconds ttl = std::chrono::hours(24 * 7))
  {
    vix::middleware::auth::SessionOptions opt{};
    opt.secret = std::move(secret);
    opt.cookie_name = std::move(cookie_name);
    opt.cookie_path = "/";

    opt.secure = true;
    opt.http_only = true;
    opt.same_site = "None";
    opt.ttl = ttl;
    opt.auto_create = true;

    return adapt_ctx(vix::middleware::auth::session(std::move(opt)));
  }

  /**
   * @brief Set a cookie after the downstream middleware/handler runs.
   */
  inline vix::App::Middleware set_cookie_dev(
      std::string name,
      std::string value,
      int max_age = 3600,
      bool secure = false,
      std::string same_site = "Lax",
      bool http_only = true,
      std::string path = "/")
  {
    return adapt_ctx(
        [name = std::move(name),
         value = std::move(value),
         max_age,
         secure,
         same_site = std::move(same_site),
         http_only,
         path = std::move(path)](vix::middleware::Context &ctx, vix::middleware::Next next) mutable
        {
          next();

          vix::middleware::cookies::Cookie c;
          c.name = std::move(name);
          c.value = std::move(value);
          c.max_age = max_age;
          c.secure = secure;
          c.same_site = std::move(same_site);
          c.http_only = http_only;
          c.path = std::move(path);

          vix::middleware::cookies::set(ctx.res(), c);
        });
  }

  /**
   * @brief API key auth middleware preset.
   */
  inline vix::App::Middleware api_key_auth(vix::middleware::auth::ApiKeyOptions opt)
  {
    return adapt_ctx(vix::middleware::auth::api_key(std::move(opt)));
  }

  /**
   * @brief API key preset using x-api-key header (and api_key query param).
   */
  inline vix::App::Middleware api_key_dev(std::string key)
  {
    vix::middleware::auth::ApiKeyOptions opt{};
    opt.header = "x-api-key";
    opt.query_param = "api_key";
    opt.required = true;
    opt.allowed_keys.insert(std::move(key));

    return api_key_auth(std::move(opt));
  }

  /**
   * @brief RBAC preset that requires the "admin" role.
   */
  inline vix::App::Middleware rbac_admin()
  {
    return chain(
        adapt_ctx(auth::rbac_context()),
        adapt_ctx(auth::require_role("admin")));
  }

} // namespace vix::middleware::app

#endif // VIX_MIDDLEWARE_APP_PRESETS_HPP
