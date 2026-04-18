/**
 *
 *  @file middleware/all.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Internal aggregation header for the Vix middleware module.
 *
 *  This file includes the full middleware surface, including the core
 *  middleware API, authentication, parsers, security, observability,
 *  performance, HTTP helpers, periodic jobs, and utilities.
 *
 *  For most use cases, prefer:
 *    #include <vix/middleware.hpp>
 *
 *  Copyright 2025, Gaspard Kirira.
 *  All rights reserved.
 *  https://github.com/vixcpp/vix
 *
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_MIDDLEWARE_ALL_HPP
#define VIX_MIDDLEWARE_ALL_HPP

// Core
#include <vix/middleware/middleware.hpp>
#include <vix/middleware/module_init.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/periodic.hpp>
#include <vix/middleware/http_cache.hpp>
#include <vix/middleware/static_dir_bridge.hpp>

// app
#include <vix/middleware/app/adapter.hpp>
#include <vix/middleware/app/app_middleware.hpp>
#include <vix/middleware/app/http_cache.hpp>
#include <vix/middleware/app/presets.hpp>

// auth
#include <vix/middleware/auth/api_key.hpp>
#include <vix/middleware/auth/jwt.hpp>
#include <vix/middleware/auth/rbac.hpp>
#include <vix/middleware/auth/session.hpp>

// basics
#include <vix/middleware/basics/body_limit.hpp>
#include <vix/middleware/basics/logger.hpp>
#include <vix/middleware/basics/recovery.hpp>
#include <vix/middleware/basics/request_id.hpp>
#include <vix/middleware/basics/timing.hpp>

// http
#include <vix/middleware/http/cookies.hpp>

// observability
#include <vix/middleware/observability/debug_trace.hpp>
#include <vix/middleware/observability/metrics.hpp>
#include <vix/middleware/observability/tracing.hpp>
#include <vix/middleware/observability/utils.hpp>

// parsers
#include <vix/middleware/parsers/form.hpp>
#include <vix/middleware/parsers/json.hpp>
#include <vix/middleware/parsers/multipart.hpp>
#include <vix/middleware/parsers/multipart_save.hpp>

// performance
#include <vix/middleware/performance/compression.hpp>
#include <vix/middleware/performance/etag.hpp>
#include <vix/middleware/performance/static_files.hpp>

// security
#include <vix/middleware/security/cors.hpp>
#include <vix/middleware/security/csrf.hpp>
#include <vix/middleware/security/headers.hpp>
#include <vix/middleware/security/ip_filter.hpp>
#include <vix/middleware/security/rate_limit.hpp>

// utils
#include <vix/middleware/utils/clock.hpp>
#include <vix/middleware/utils/header_utils.hpp>
#include <vix/middleware/utils/json_writer.hpp>
#include <vix/middleware/utils/key_builder.hpp>
#include <vix/middleware/utils/token_bucket.hpp>

#endif // VIX_MIDDLEWARE_ALL_HPP
