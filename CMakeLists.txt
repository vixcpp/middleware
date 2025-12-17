# ====================================================================
# Vix.cpp — Middleware Module
# ====================================================================
# Purpose:
#   Build configuration for the Vix middleware module:
#   - Defines the middleware abstraction (request/response + next)
#   - Provides a pipeline to compose multiple middlewares
#   - Hosts common built-in middlewares (logging, CORS, timing, etc.)
#
# Public Targets:
#   - vix_middleware    : The actual library target (STATIC or INTERFACE)
#   - vix::middleware   : Canonical namespaced alias
#   - vix::mw           : Short ergonomic alias
#
# Public Dependencies:
#   - vix::core   (HTTP server, Request/Response, routing)
#   - vix::utils  (logging, helpers)
#   - optional JSON backend (vix::json / vix_json)
#
# Options:
#   - VIX_MIDDLEWARE_WITH_JSON : AUTO|ON|OFF (default: AUTO)
#   - VIX_MIDDLEWARE_FETCH_CORE: Auto-fetch vix::core if missing (default ON)
#
# Installation/Export:
#   - Contributes to the umbrella export-set `VixTargets`
#   - Headers installed under <prefix>/include/vix/middleware/...
# ====================================================================

cmake_minimum_required(VERSION 3.20)
project(vix_middleware VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_DISABLE_FIND_PACKAGE_VixOrm ON)


include(GNUInstallDirs)

# ------------------------ Global settings ----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ------------------------ Sources discovery --------------------------
file(GLOB_RECURSE MIDDLEWARE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# ------------------------ Core dependency ----------------------------
option(VIX_MIDDLEWARE_FETCH_CORE "Auto-fetch vix::core if missing" ON)

if (NOT TARGET vix::core)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../core/CMakeLists.txt")
    message(STATUS "[middleware] Adding core from umbrella: ../core")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../core" "core")
  elseif (VIX_MIDDLEWARE_FETCH_CORE)
    include(FetchContent)
    message(STATUS "[middleware] Fetching vix::core via FetchContent")
    FetchContent_Declare(vix_core
      GIT_REPOSITORY https://github.com/vixcpp/core.git
      GIT_TAG        v1.5.2
    )
    FetchContent_MakeAvailable(vix_core)
  else()
    message(FATAL_ERROR
      "vix::core not found. Provide vix::core before middleware "
      "or enable VIX_MIDDLEWARE_FETCH_CORE=ON.")
  endif()
endif()

set(VIX_CORE_TARGET vix::core)

# ------------------------ JSON linkage policy ------------------------
set(VIX_MIDDLEWARE_WITH_JSON "AUTO" CACHE STRING "Link JSON backend (AUTO|ON|OFF)")
set_property(CACHE VIX_MIDDLEWARE_WITH_JSON PROPERTY STRINGS AUTO ON OFF)

function(vix_middleware_try_link_json onto link_scope)
  if (VIX_MIDDLEWARE_WITH_JSON STREQUAL "OFF")
    message(STATUS "[middleware] JSON disabled (OFF)")
    target_compile_definitions(${onto} ${link_scope} VIX_MIDDLEWARE_NO_JSON=1)
    return()
  endif()

  set(_json_candidates vix::json vix_json)
  set(_json_found "")
  foreach(t IN LISTS _json_candidates)
    if (TARGET ${t})
      set(_json_found ${t})
      break()
    endif()
  endforeach()

  if (_json_found)
    message(STATUS "[middleware] JSON backend: ${_json_found}")
    target_link_libraries(${onto} ${link_scope} ${_json_found})
  else()
    if (VIX_MIDDLEWARE_WITH_JSON STREQUAL "ON")
      message(FATAL_ERROR
        "JSON required but not found (expected: vix::json or vix_json).")
    else()
      message(STATUS "[middleware] No JSON backend detected; building without JSON.")
      target_compile_definitions(${onto} ${link_scope} VIX_MIDDLEWARE_NO_JSON=1)
    endif()
  endif()
endfunction()


# ============================== STATIC ===============================
if (MIDDLEWARE_SOURCES)
  message(STATUS "[middleware] Building STATIC library with detected sources.")

  add_library(vix_middleware STATIC ${MIDDLEWARE_SOURCES})
  if (TARGET vix_warnings)
    target_link_libraries(vix_middleware PUBLIC vix_warnings)
  endif()

  add_library(vix::middleware ALIAS vix_middleware)
  add_library(vix::mw         ALIAS vix_middleware)
  target_compile_features(vix_middleware PUBLIC cxx_std_20)

  target_include_directories(vix_middleware
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  target_link_libraries(vix_middleware
    PUBLIC
      ${VIX_CORE_TARGET}
  )

  vix_middleware_try_link_json(vix_middleware PUBLIC)
  if (VIX_ENABLE_SANITIZERS AND TARGET vix_sanitizers)
    target_link_libraries(vix_middleware PUBLIC vix_sanitizers)
  endif()

  set_target_properties(vix_middleware PROPERTIES
    OUTPUT_NAME vix_middleware
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    EXPORT_NAME middleware
  )

  install(TARGETS vix_middleware
    EXPORT VixTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

# ============================ HEADER-ONLY ============================
else()
  message(STATUS "[middleware] Building HEADER-ONLY library (no sources).")

  add_library(vix_middleware INTERFACE)

  if (TARGET vix_warnings)
    target_link_libraries(vix_middleware INTERFACE vix_warnings)
  endif()

  add_library(vix::middleware ALIAS vix_middleware)
  add_library(vix::mw         ALIAS vix_middleware)

  target_compile_features(vix_middleware INTERFACE cxx_std_20)

  target_include_directories(vix_middleware INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  target_link_libraries(vix_middleware INTERFACE
    ${VIX_CORE_TARGET}
  )

  vix_middleware_try_link_json(vix_middleware INTERFACE)
  if (VIX_ENABLE_SANITIZERS AND TARGET vix_sanitizers)
    target_link_libraries(vix_middleware INTERFACE vix_sanitizers)
  endif()

  install(TARGETS vix_middleware
    EXPORT VixTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
endif()

option(VIX_MIDDLEWARE_BUILD_TESTS "Build middleware tests" OFF)

if (VIX_MIDDLEWARE_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

# ----------------------------- Summary -------------------------------
message(STATUS "------------------------------------------------------")
message(STATUS "vix::middleware configured (${PROJECT_VERSION})")
if (MIDDLEWARE_SOURCES)
  message(STATUS "Mode: STATIC / sources found")
else()
  message(STATUS "Mode: HEADER-ONLY / no sources")
endif()
message(STATUS "JSON policy:        ${VIX_MIDDLEWARE_WITH_JSON}")
message(STATUS "Core target:        ${VIX_CORE_TARGET}")
message(STATUS "Include dir:        ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "Sanitizers enabled: ${VIX_ENABLE_SANITIZERS}")
message(STATUS "Build type:         ${CMAKE_BUILD_TYPE}")
message(STATUS "------------------------------------------------------")
