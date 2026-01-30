# Vix Middleware

High-performance • Non-blocking • Composable • Production-oriented

The Vix **middleware** module provides a modern, ergonomic middleware system for the **Vix.cpp HTTP runtime**.
It is designed for real applications: security hardening, parsing, auth, request-scoped state, and safe composition.

This module supports **two middleware styles**:

* **Context-based middleware** (`MiddlewareFn`) — preferred
* **Legacy HTTP middleware** (`HttpMiddleware`) — supported via adapters

It also ships with **ready-to-use presets** (CORS, rate limit, CSRF, security headers, IP filtering, parsers, auth, HTTP cache).

---

## Why this module exists

Most C++ “middleware” systems are either:

* too framework-specific
* hard to compose
* blocking by accident
* not explicit about safety (early return, status changes, observable behavior)

Vix middleware is designed to be:

* **explicit**: every middleware decides whether to continue (`next()`) or stop
* **composable**: chain middlewares cleanly
* **safe**: defensive parsing, predictable early returns, no hidden magic
* **fast**: zero-cost abstractions where possible, non-blocking patterns by default

---

## Quick start

Run the full “mega example”:

```bash
vix run examples/http_middleware/mega_middleware_routes.cpp
```

Test quickly:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/api/ping -H "x-demo: 1"
curl -i http://127.0.0.1:8080/api/secure/whoami -H "x-api-key: dev_key_123"
curl -i http://127.0.0.1:8080/dev/trace
curl -i http://127.0.0.1:8080/api/cache/demo
curl -i http://127.0.0.1:8080/api/cache/demo -H "x-vix-cache: bypass"
```

---

## Minimal example

### 1) A tiny context-based middleware

```cpp
#include <vix.hpp>
#include <vix/middleware/app/adapter.hpp>

static vix::middleware::MiddlewareFn hello_mw()
{
  return [](vix::middleware::Context& ctx, vix::middleware::Next next) {
    ctx.res().header("x-hello", "vix");
    next();
  };
}

int main()
{
  vix::App app;

  // Adapt ctx middleware into an App middleware
  app.use(vix::middleware::app::adapt_ctx(hello_mw()));

  app.get("/", [](auto&, auto& res) {
    res.send("ok");
  });

  app.run(8080);
}
```

### 2) A legacy-style middleware (still supported)

```cpp
#include <vix.hpp>
#include <vix/middleware/app/adapter.hpp>

static vix::middleware::HttpMiddleware require_header(std::string key)
{
  return [key = std::move(key)](vix::Request& req, vix::Response& res, vix::middleware::Next next) {
    if (req.header(key).empty()) {
      res.status(401).send("missing header");
      return; // do not call next()
    }
    next();
  };
}

int main()
{
  vix::App app;

  // Adapt legacy middleware into an App middleware
  app.use(vix::middleware::app::adapt(require_header("x-demo")));

  app.get("/", [](auto&, auto& res) {
    res.send("ok");
  });

  app.run(8080);
}
```

---

## Core concepts

### 1) Context-based middleware (recommended)

A `MiddlewareFn` looks like:

```cpp
(vix::middleware::Context& ctx, vix::middleware::Next next) -> void
```

Rules:

* To continue the pipeline, call `next()`.
* To stop early, do **not** call `next()`.
* You can modify request/response and store request-scoped state.

---

### 2) RequestState (store data across middlewares + handler)

Vix provides type-based request storage (like `std::any`).

```cpp
struct AuthInfo { bool authed{}; std::string role; };

ctx.req().emplace_state<AuthInfo>(AuthInfo{true, "admin"});

if (ctx.req().has_state_type<AuthInfo>()) {
  const auto& a = ctx.req().state<AuthInfo>();
}
```

Use it to:

* pass computed data to handlers
* keep auth/session info
* track timings / trace IDs
* attach parsing results

---

### 3) Installation patterns

The app adapter layer provides installation helpers:

* `app.use(mw)` — global
* `install(app, "/prefix/", mw)` — prefix middleware
* `install_exact(app, "/path", mw)` — exact-path middleware
* `chain(mw1, mw2, ...)` — compose middlewares

This is how you build safe and readable route protection:

```cpp
using namespace vix::middleware::app;

install(app, "/api/", rate_limit_dev(120, std::chrono::minutes(1)));
install(app, "/api/secure/", api_key_dev("dev_key_123"));
install(app, "/api/admin/", chain(adapt_ctx(fake_auth()), adapt_ctx(require_admin())));
```

---

## Presets included

The module includes “batteries-included” middleware presets for common needs.
Names may vary slightly depending on your tree, but the example demonstrates:

### Security

* CORS (dev / configurable)
* Security headers
* CSRF (optional)
* IP filtering

### Rate limiting

* Simple in-memory dev limiter (good for local, demos, CI)

### Auth

* API key
* JWT (when enabled)
* RBAC-like gating patterns

### Parsers

* JSON parser
* Form parser
* Multipart parser
* Multipart save (write uploads to disk safely)

### HTTP cache (GET cache)

A cache middleware for GET endpoints, with explicit bypass controls:

* TTL-based caching
* optional bypass header (e.g. `x-vix-cache: bypass`)
* debug headers (e.g. `x-vix-cache-status: hit/miss/bypass`)

---

## Full reference example

The recommended starting point is:

* `examples/http_middleware/mega_middleware_routes.cpp`

It demonstrates:

* routes (GET/POST/etc.)
* global middleware (`App::use`)
* prefix middleware (`install`)
* exact-path middleware (`install_exact`)
* `adapt_ctx()` and `adapt()`
* middleware chaining (`chain`)
* security presets (CORS, headers, CSRF, IP filter)
* rate limiting
* parsers (JSON, form, multipart)
* auth (API key, role gating)
* HTTP cache (GET)
* RequestState patterns
* early return patterns and defensive JSON replies

---

## Directory layout

Typical layout:

```
modules/middleware/
│
├─ include/vix/middleware/
│  ├─ app/
│  │  ├─ adapter.hpp          # adapt_ctx(), adapt(), install(), chain(), ...
│  │  ├─ presets.hpp          # cors_dev(), rate_limit_dev(), api_key_dev(), ...
│  │  └─ http_cache.hpp       # install_http_cache() + config
│  ├─ Context.hpp
│  ├─ Middleware.hpp
│  └─ ...
│
└─ examples/
   └─ http_middleware/
      └─ mega_middleware_routes.cpp
```

---

## Design philosophy

* **Control plane only**: middleware decides flow explicitly.
* **No hidden blocking**: handlers stay short; heavy work should be scheduled.
* **Defensive parsing**: invalid input yields predictable errors.
* **Observable behavior**: trace IDs, debug headers, explicit cache bypass.
* **Composable by default**: chains and prefix installs build readable security.

---

## License

MIT — same as Vix.cpp

Repository: [https://github.com/vixcpp/vix](https://github.com/vixcpp/vix)
