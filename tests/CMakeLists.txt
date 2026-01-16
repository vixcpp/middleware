#  @file CMakeLists.cpp
#  @author Gaspard Kirira
#
#  Copyright 2025, Gaspard Kirira.  All rights reserved.
#  https://github.com/vixcpp/vix
#  Use of this source code is governed by a MIT license
#  that can be found in the License file.
#
#  Vix.cpp
include(CTest)

if (NOT BUILD_TESTING)
  return()
endif()

function(vix_add_test test_name test_source)
  add_executable(${test_name} ${test_source})

  target_link_libraries(${test_name} PRIVATE
    vix::middleware
  )

  target_compile_features(${test_name} PRIVATE cxx_std_20)

  add_test(
    NAME ${test_name}
    COMMAND ${test_name}
  )
endfunction()

# Pipeline
vix_add_test(middleware_pipeline_smoke_test pipeline/pipeline_smoke_test.cpp)

# HTTP
vix_add_test(middleware_http_cache_smoke_test http/http_cache_smoke_test.cpp)

# Core
vix_add_test(middleware_context_smoke_test       core/context_smoke_test.cpp)
vix_add_test(middleware_result_smoke_test        core/result_smoke_test.cpp)
vix_add_test(middleware_next_smoke_test          core/next_smoke_test.cpp)
vix_add_test(middleware_hooks_smoke_test         core/hooks_smoke_test.cpp)
vix_add_test(middleware_hooks_merge_smoke_test   core/hooks_merge_smoke_test.cpp)

# Basics
vix_add_test(middleware_recovery_smoke_test     basics/recovery_smoke_test.cpp)
vix_add_test(middleware_logger_smoke_test       basics/logger_smoke_test.cpp)
vix_add_test(middleware_timing_smoke_test       basics/timing_smoke_test.cpp)
vix_add_test(middleware_request_id_smoke_test   basics/request_id_smoke_test.cpp)
vix_add_test(middleware_body_limit_smoke_test   basics/body_limit_smoke_test.cpp)

# Security
vix_add_test(middleware_headers_smoke_test      security/headers_smoke_test.cpp)
vix_add_test(middleware_cors_smoke_test         security/cors_smoke_test.cpp)
vix_add_test(middleware_ip_filter_smoke_test    security/ip_filter_smoke_test.cpp)
vix_add_test(middleware_rate_limit_smoke_test   security/rate_limit_smoke_test.cpp)
vix_add_test(middleware_csrf_smoke_test         security/csrf_smoke_test.cpp)

# Observability
vix_add_test(middleware_debug_trace_smoke_test        observability/debug_trace_smoke_test.cpp)
vix_add_test(middleware_tracing_smoke_test            observability/tracing_smoke_test.cpp)
vix_add_test(middleware_metrics_smoke_test            observability/metrics_smoke_test.cpp)
vix_add_test(middleware_dev_observability_smoke_test  observability/dev_observability_smoke_test.cpp)

# Parsers
vix_add_test(middleware_json_parser_smoke_test       parsers/json_smoke_test.cpp)
vix_add_test(middleware_form_parser_smoke_test       parsers/form_smoke_test.cpp)
vix_add_test(middleware_multipart_parser_smoke_test  parsers/multipart_smoke_test.cpp)

# Performance
vix_add_test(middleware_etag_smoke_test          performance/etag_smoke_test.cpp)
vix_add_test(middleware_compression_smoke_test   performance/compression_smoke_test.cpp)
vix_add_test(middleware_static_files_smoke_test  performance/static_files_smoke_test.cpp)

# Utils
vix_add_test(middleware_json_writer_smoke_test   utils/json_writer_smoke_test.cpp)
vix_add_test(middleware_key_builder_smoke_test   utils/key_builder_smoke_test.cpp)
vix_add_test(middleware_token_bucket_smoke_test  utils/token_bucket_smoke_test.cpp)

# Auth
vix_add_test(middleware_api_key_smoke_test  auth/api_key_smoke_test.cpp)
vix_add_test(middleware_jwt_smoke_test      auth/jwt_smoke_test.cpp)
vix_add_test(middleware_rbac_smoke_test     auth/rbac_smoke_test.cpp)
