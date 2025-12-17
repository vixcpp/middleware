include(CTest)

# Pipeline smoke test
add_executable(middleware_pipeline_smoke_test
  pipeline/pipeline_smoke_test.cpp
)

target_link_libraries(middleware_pipeline_smoke_test PRIVATE
  vix::middleware
)

target_compile_features(middleware_pipeline_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(NAME middleware_pipeline_smoke_test COMMAND middleware_pipeline_smoke_test)
endif()

# HTTP cache middleware smoke test
add_executable(middleware_http_cache_smoke_test
  http/http_cache_smoke_test.cpp
)

target_link_libraries(middleware_http_cache_smoke_test PRIVATE
  vix::middleware
)

target_compile_features(middleware_http_cache_smoke_test PRIVATE cxx_std_20)

if (BUILD_TESTING)
  add_test(NAME middleware_http_cache_smoke_test COMMAND middleware_http_cache_smoke_test)
endif()
