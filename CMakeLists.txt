# ====================================================================
#  @file CMakeLists.txt
#  @author Gaspard Kirira
#
#  Copyright 2025, Gaspard Kirira. All rights reserved.
#  https://github.com/vixcpp/vix
#  Use of this source code is governed by a MIT license
#  that can be found in the License file.
#
#  Vix.cpp
#
# Purpose:
#   Build configuration for the Vix middleware module.
#   This module provides HTTP middleware building blocks on top of the
#   Vix module ecosystem.
#
# Public Targets:
#   - vix_middleware   : The actual library target (STATIC or INTERFACE)
#   - vix::middleware  : Namespaced alias for consumers
#   - vix::mw          : Short alias for consumers
#
# Required Dependencies:
#   - vix::core
#   - vix::cache
#
# Optional Dependencies:
#   - vix::utils
#   - vix::json / vix_json
#
# Options:
#   - VIX_MW_FETCH_CORE   : Auto-fetch core if missing
#   - VIX_MW_FETCH_CACHE  : Auto-fetch cache if missing
#   - VIX_MW_FETCH_UTILS  : Auto-fetch utils if missing
#   - VIX_MW_WITH_UTILS   : AUTO|ON|OFF
#   - VIX_MW_WITH_JSON    : AUTO|ON|OFF
#   - VIX_MW_BUILD_TESTS  : Build middleware tests
#   - VIX_ENABLE_SANITIZERS : Inherit sanitizers from umbrella project
#
# Installation/Export:
#   This module installs its target and headers and contributes to the
#   top-level export-set `VixTargets`. Consumers can then use:
#
#       find_package(Vix CONFIG REQUIRED)
#       target_link_libraries(app PRIVATE vix::middleware)
#
# Notes:
#   - This file is designed to be included from a superproject (preferred).
#   - Avoids per-module export-sets; relies on the umbrella's `VixTargets`.
#   - This module must not depend on Boost.
# ====================================================================

cmake_minimum_required(VERSION 3.20)
project(vix_middleware VERSION 1.0.0 LANGUAGES CXX)

set(Boost_FOUND OFF)
set(Boost_NO_BOOST_CMAKE ON)
set(Boost_NO_SYSTEM_PATHS ON)
set(CMAKE_DISABLE_FIND_PACKAGE_VixOrm ON)

include(GNUInstallDirs)

# --------------------------------------------------------------------
# Global settings
# --------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --------------------------------------------------------------------
# Sources discovery
# --------------------------------------------------------------------
file(GLOB_RECURSE MIDDLEWARE_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# --------------------------------------------------------------------
# Helper: pick the first available target from a candidate list
# --------------------------------------------------------------------
function(vix_pick_first_target out_var)
  set(_picked "")
  foreach(_cand IN LISTS ARGN)
    if (TARGET ${_cand})
      set(_picked ${_cand})
      break()
    endif()
  endforeach()
  set(${out_var} "${_picked}" PARENT_SCOPE)
endfunction()

# --------------------------------------------------------------------
# Required: Core
# --------------------------------------------------------------------
option(VIX_MW_FETCH_CORE "Auto-fetch vix::core if missing" ON)

vix_pick_first_target(VIX_CORE_TARGET
  vix::core
  vix_core
  core
)

if (NOT VIX_CORE_TARGET)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../core/CMakeLists.txt")
    message(STATUS "[middleware] Adding core from umbrella: ../core")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../core" "core")
  elseif (VIX_MW_FETCH_CORE)
    include(FetchContent)
    message(STATUS "[middleware] Fetching vix::core via FetchContent")
    FetchContent_Declare(vix_core
      GIT_REPOSITORY https://github.com/vixcpp/core.git
      GIT_TAG        dev
    )
    FetchContent_MakeAvailable(vix_core)
  else()
    message(FATAL_ERROR
      "vix::core not found. Provide ../core or enable VIX_MW_FETCH_CORE=ON."
    )
  endif()

  vix_pick_first_target(VIX_CORE_TARGET
    vix::core
    vix_core
    core
  )
endif()

if (NOT VIX_CORE_TARGET)
  message(FATAL_ERROR
    "Core target not found after dependency resolution. "
    "Expected one of: vix::core, vix_core, core."
  )
endif()

message(STATUS "[middleware] Core target: ${VIX_CORE_TARGET}")

# --------------------------------------------------------------------
# Required: Cache
# --------------------------------------------------------------------
option(VIX_MW_FETCH_CACHE "Auto-fetch vix::cache if missing" ON)

vix_pick_first_target(VIX_CACHE_TARGET
  vix::cache
  vix_cache
  cache
)

if (NOT VIX_CACHE_TARGET)
  if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../cache/CMakeLists.txt")
    message(STATUS "[middleware] Adding cache from umbrella: ../cache")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../cache" "cache")
  elseif (VIX_MW_FETCH_CACHE)
    include(FetchContent)
    message(STATUS "[middleware] Fetching vix::cache via FetchContent")
    FetchContent_Declare(vix_cache
      GIT_REPOSITORY https://github.com/vixcpp/cache.git
      GIT_TAG        v0.1.0
    )
    FetchContent_MakeAvailable(vix_cache)
  else()
    message(FATAL_ERROR
      "vix::cache not found. Provide ../cache or enable VIX_MW_FETCH_CACHE=ON."
    )
  endif()

  vix_pick_first_target(VIX_CACHE_TARGET
    vix::cache
    vix_cache
    cache
  )
endif()

if (NOT VIX_CACHE_TARGET)
  message(FATAL_ERROR
    "Cache target not found after dependency resolution. "
    "Expected one of: vix::cache, vix_cache, cache."
  )
endif()

message(STATUS "[middleware] Cache target: ${VIX_CACHE_TARGET}")

# --------------------------------------------------------------------
# Optional: Utils
# --------------------------------------------------------------------
set(VIX_MW_WITH_UTILS "AUTO" CACHE STRING "Link utils module (AUTO|ON|OFF)")
set_property(CACHE VIX_MW_WITH_UTILS PROPERTY STRINGS AUTO ON OFF)
option(VIX_MW_FETCH_UTILS "Auto-fetch vix::utils if missing (when needed)" ON)

function(vix_mw_try_link_utils onto link_scope)
  if (VIX_MW_WITH_UTILS STREQUAL "OFF")
    message(STATUS "[middleware] Utils disabled (OFF)")
    target_compile_definitions(${onto} ${link_scope} VIX_MW_NO_UTILS=1)
    return()
  endif()

  set(_utils_target "")
  vix_pick_first_target(_utils_target
    vix::utils
    vix_utils
    utils
  )

  if (NOT _utils_target)
    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../utils/CMakeLists.txt")
      message(STATUS "[middleware] Adding utils from umbrella: ../utils")
      add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../utils" "utils")
    elseif (VIX_MW_FETCH_UTILS)
      include(FetchContent)
      message(STATUS "[middleware] Fetching vix::utils via FetchContent")
      FetchContent_Declare(vix_utils
        GIT_REPOSITORY https://github.com/vixcpp/utils.git
        GIT_TAG        dev
      )
      FetchContent_MakeAvailable(vix_utils)
    endif()

    vix_pick_first_target(_utils_target
      vix::utils
      vix_utils
      utils
    )
  endif()

  if (_utils_target)
    message(STATUS "[middleware] Utils backend: ${_utils_target}")
    target_link_libraries(${onto} ${link_scope} ${_utils_target})
    target_compile_definitions(${onto} ${link_scope} VIX_MW_WITH_UTILS=1)
  else()
    if (VIX_MW_WITH_UTILS STREQUAL "ON")
      message(FATAL_ERROR
        "Utils required but not found. Expected one of: vix::utils, vix_utils, utils."
      )
    else()
      message(STATUS "[middleware] Utils not detected; building without utils.")
      target_compile_definitions(${onto} ${link_scope} VIX_MW_NO_UTILS=1)
    endif()
  endif()
endfunction()

# --------------------------------------------------------------------
# Optional: JSON backend
# --------------------------------------------------------------------
set(VIX_MW_WITH_JSON "AUTO" CACHE STRING "Link JSON backend (AUTO|ON|OFF)")
set_property(CACHE VIX_MW_WITH_JSON PROPERTY STRINGS AUTO ON OFF)

function(vix_mw_try_link_json onto link_scope)
  if (VIX_MW_WITH_JSON STREQUAL "OFF")
    message(STATUS "[middleware] JSON disabled (OFF)")
    target_compile_definitions(${onto} ${link_scope} VIX_MW_NO_JSON=1)
    return()
  endif()

  set(_json_target "")
  vix_pick_first_target(_json_target
    vix::json
    vix_json
    json
  )

  if (_json_target)
    message(STATUS "[middleware] JSON backend: ${_json_target}")
    target_link_libraries(${onto} ${link_scope} ${_json_target})
    target_compile_definitions(${onto} ${link_scope} VIX_MW_WITH_JSON=1)
  else()
    if (VIX_MW_WITH_JSON STREQUAL "ON")
      message(FATAL_ERROR
        "JSON required but not found. Expected one of: vix::json, vix_json, json."
      )
    else()
      message(STATUS "[middleware] No JSON backend detected; building without JSON.")
      target_compile_definitions(${onto} ${link_scope} VIX_MW_NO_JSON=1)
    endif()
  endif()
endfunction()

# --------------------------------------------------------------------
# Hard fail if Boost sneaks in
# --------------------------------------------------------------------
if (TARGET Boost::boost OR TARGET Boost::system)
  message(FATAL_ERROR "Boost is not allowed in vix::middleware")
endif()

# --------------------------------------------------------------------
# Library target
# --------------------------------------------------------------------
if (MIDDLEWARE_SOURCES)
  message(STATUS "[middleware] Building STATIC library with detected sources.")

  add_library(vix_middleware STATIC ${MIDDLEWARE_SOURCES})
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
      ${VIX_CACHE_TARGET}
  )

  if (TARGET vix_warnings)
    target_link_libraries(vix_middleware PUBLIC vix_warnings)

    if (VIX_ENABLE_SANITIZERS AND TARGET vix_sanitizers)
      target_link_libraries(vix_middleware PUBLIC vix_sanitizers)
    endif()
  else()
    message(STATUS "[middleware] vix_warnings not found (building standalone)")
  endif()

  vix_mw_try_link_utils(vix_middleware PUBLIC)
  vix_mw_try_link_json(vix_middleware PUBLIC)

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

else()
  message(STATUS "[middleware] Building HEADER-ONLY library (no sources).")

  add_library(vix_middleware INTERFACE)
  add_library(vix::middleware ALIAS vix_middleware)
  add_library(vix::mw         ALIAS vix_middleware)

  target_compile_features(vix_middleware INTERFACE cxx_std_20)

  target_include_directories(vix_middleware
    INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  target_link_libraries(vix_middleware
    INTERFACE
      ${VIX_CORE_TARGET}
      ${VIX_CACHE_TARGET}
  )

  if (TARGET vix_warnings)
    target_link_libraries(vix_middleware INTERFACE vix_warnings)

    if (VIX_ENABLE_SANITIZERS AND TARGET vix_sanitizers)
      target_link_libraries(vix_middleware INTERFACE vix_sanitizers)
    endif()
  endif()

  vix_mw_try_link_utils(vix_middleware INTERFACE)
  vix_mw_try_link_json(vix_middleware INTERFACE)

  install(TARGETS vix_middleware
    EXPORT VixTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()

# --------------------------------------------------------------------
# Headers installation
# --------------------------------------------------------------------
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
)

# --------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------
option(VIX_MW_BUILD_TESTS "Build middleware tests" OFF)

if (VIX_MW_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

# --------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------
message(STATUS "------------------------------------------------------")
message(STATUS "vix::middleware configured (${PROJECT_VERSION})")
if (MIDDLEWARE_SOURCES)
  message(STATUS "Mode: STATIC / sources found")
else()
  message(STATUS "Mode: HEADER-ONLY / no sources")
endif()
message(STATUS "Core target: ${VIX_CORE_TARGET}")
message(STATUS "Cache target: ${VIX_CACHE_TARGET}")
message(STATUS "Utils mode: ${VIX_MW_WITH_UTILS}")
message(STATUS "JSON mode: ${VIX_MW_WITH_JSON}")
message(STATUS "Tests: ${VIX_MW_BUILD_TESTS}")
message(STATUS "Include dir: ${CMAKE_CURRENT_SOURCE_DIR}/include")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "------------------------------------------------------")
