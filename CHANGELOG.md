# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
## [1.2.0] - 2025-12-24

### Added
- 

### Changed
- 

### Removed
- 


### Middleware v1.2.0

This release introduces a major cleanup and DX improvement for the middleware system.

#### Core changes

- Moved middleware primitives (`Context`, `Next`, `Hooks`, `Result`) from
  `middleware/core/*` into `core/mw/*`
- Removed circular dependency between `core` and `middleware`
- Middleware now depends on core primitives, never the opposite

#### API & DX improvements

- Introduced `vix::middleware::app` namespace for App-level middleware helpers
- Added Node/FastAPI-like helpers:
  - `middleware::app::http_cache(...)`
  - `install_*` helpers for App integration
- Standardized middleware adapters and conditions (`adapt`, `when`, `install`)

#### Middleware updates

- Updated all middleware modules to use new core primitives:
  - auth (jwt, api_key, rbac)
  - observability (metrics, tracing, debug)
  - parsers, security, performance, basics
- Updated pipeline and hooks integration

#### Tests

- Updated all core middleware smoke tests
- Updated http_cache demos and tests
- All middleware tests now target core primitives

This release significantly simplifies user-facing middleware usage while
making the internal architecture cleaner, safer, and future-proof.

## [1.1.1] - 2025-12-24

### Added

-

### Changed

-

### Removed

-

## [1.1.0] - 2025-12-23

### Added

-

### Changed

-

### Removed

-

## [1.0.0] - 2025-12-17

### Added

-

### Changed

-

### Removed

-

## [0.1.1] - 2025-12-14

### Added

-

### Changed

-

### Removed

-

## [0.1.0] - 2025-12-05

### Added

-

### Changed

-

### Removed

-

## [v0.1.0] - 2025-12-05

### Added

- Initial project scaffolding for the `vixcpp/middleware` module.
- CMake build system:
  - STATIC vs header-only build depending on `src/` contents.
  - Integration with `vix::core` and optional JSON backend.
  - Support for sanitizers via `VIX_ENABLE_SANITIZERS`.
- Basic repository structure:
  - `include/vix/middleware/` for public middleware API.
  - `src/` for implementation files.
- Release workflow:
  - `Makefile` with `release`, `commit`, `push`, `merge`, and `tag` targets.
  - `changelog` target wired to `scripts/update_changelog.sh`.
