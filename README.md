# Vix.cpp Middleware Module

Composable HTTP middleware for Vix.cpp.

The Middleware module provides a clean middleware layer for Vix applications, with support for context-based middleware, legacy HTTP middleware adapters, request-scoped state, middleware chaining, route protection, security presets, parsers, authentication helpers, and HTTP cache.

## Documentation

Full documentation will be available here:

https://docs.vixcpp.com/modules/middleware/

API reference:

https://docs.vixcpp.com/modules/middleware/api-reference

## What Middleware provides

- Context-based middleware
- Legacy HTTP middleware adapters
- Middleware chaining
- Conditional middleware
- Prefix-based middleware installation
- Exact-path route protection
- Request-scoped context
- Shared middleware services
- Security presets
- CORS helpers
- Rate limiting
- CSRF protection
- Security headers
- IP filtering
- Body size limits
- JSON, form, and multipart parsers
- API key authentication
- JWT authentication
- Session helpers
- RBAC helpers
- HTTP cache middleware

## Public headers

```cpp
#include <vix/middleware/middleware.hpp>
#include <vix/middleware/app/adapter.hpp>
#include <vix/middleware/app/app_middleware.hpp>
```

For ready-to-use presets:

```cpp
#include <vix/middleware/app/presets.hpp>
```

For HTTP cache:

```cpp
#include <vix/middleware/app/http_cache.hpp>
```

## Basic middleware

```cpp
#include <vix.hpp>
#include <vix/middleware/app/adapter.hpp>
#include <vix/middleware/middleware.hpp>

static vix::middleware::MiddlewareFn hello_middleware()
{
  return [](vix::middleware::Context &ctx, vix::middleware::Next next)
  {
    ctx.res().header("x-vix-middleware", "active");

    next();
  };
}

int main()
{
  vix::App app;

  app.use(vix::middleware::app::adapt_ctx(hello_middleware()));

  app.get("/", [](vix::Request &req, vix::Response &res)
  {
    (void)req;

    res.text("Hello from Vix middleware");
  });

  app.run(8080);

  return 0;
}
```

Run:

```bash
vix run main.cpp
```

## Legacy HTTP middleware

```cpp
#include <vix.hpp>
#include <vix/middleware/app/adapter.hpp>
#include <vix/middleware/middleware.hpp>

static vix::middleware::HttpMiddleware require_header(std::string header)
{
  return [header = std::move(header)](
      vix::Request &req,
      vix::Response &res,
      vix::middleware::Next next)
  {
    if (req.header(header).empty())
    {
      res.status(401).text("missing required header");
      return;
    }

    next();
  };
}

int main()
{
  vix::App app;

  app.use(vix::middleware::app::adapt(require_header("x-api-key")));

  app.get("/", [](vix::Request &req, vix::Response &res)
  {
    (void)req;

    res.text("OK");
  });

  app.run(8080);

  return 0;
}
```

## Middleware chaining

```cpp
#include <vix.hpp>
#include <vix/middleware/app/app_middleware.hpp>
#include <vix/middleware/app/presets.hpp>

int main()
{
  vix::App app;

  auto secure_api = vix::middleware::app::chain(
      vix::middleware::app::security_headers_dev(),
      vix::middleware::app::rate_limit_dev(120, std::chrono::minutes(1)),
      vix::middleware::app::api_key_dev("dev_key_123"));

  vix::middleware::app::install(app, "/api/", std::move(secure_api));

  app.get("/api/status", [](vix::Request &req, vix::Response &res)
  {
    (void)req;

    res.json({
      {"status", "ok"}
    });
  });

  app.run(8080);

  return 0;
}
```

## Route protection

Apply middleware to an exact path:

```cpp
vix::middleware::app::install_exact(
    app,
    "/admin",
    vix::middleware::app::api_key_dev("dev_key_123"));
```

Apply middleware to a prefix:

```cpp
vix::middleware::app::install(
    app,
    "/api/",
    vix::middleware::app::rate_limit_dev());
```

## HTTP cache

```cpp
#include <vix.hpp>
#include <vix/middleware/app/http_cache.hpp>

int main()
{
  vix::App app;

  vix::middleware::app::HttpCacheConfig cfg;
  cfg.prefix = "/api/";
  cfg.ttl_ms = 30'000;
  cfg.add_debug_header = true;

  vix::middleware::app::use_http_cache(app, cfg);

  app.get("/api/cache/demo", [](vix::Request &req, vix::Response &res)
  {
    (void)req;

    res.json({
      {"cached", true}
    });
  });

  app.run(8080);

  return 0;
}
```

## Middleware architecture

```text
App
  -> App middleware
  -> adapter
  -> Context
  -> Next
  -> user middleware
  -> handler
```

For cache middleware:

```text
Request
  -> cache middleware
  -> cache key
  -> hit or miss
  -> handler
  -> cached response
```

## Build

Contributors should use the Vix CLI to build this module.

Vix wraps the C++ build workflow with project detection, presets, Ninja builds, clean logs, caching, and focused diagnostics. This keeps the contributor workflow consistent and helps avoid hidden C++ build issues.

### Build the project

```bash
git clone https://github.com/vixcpp/vix.git
cd vix
vix build
```

### Build all targets

Use this before running the full test suite, install workflows, or release checks:

```bash
vix build --build-target all
```

### Clean rebuild

Use this when the local CMake cache or build directory may be stale:

```bash
vix build --clean
```

### Release build

```bash
vix build --preset release
```

## Tests

Build all targets first, then run tests:

```bash
vix build --build-target all
vix tests
```

Before opening a pull request, use:

```bash
vix fmt --check
vix build --build-target all
vix tests
```

## Useful links

- Middleware documentation: https://docs.vixcpp.com/modules/middleware/
- Middleware API reference: https://docs.vixcpp.com/modules/middleware/api-reference
- Build command: https://docs.vixcpp.com/cli/build
- Tests command: https://docs.vixcpp.com/cli/tests
- Documentation: https://docs.vixcpp.com/
- Engineering notes: https://blog.vixcpp.com/
- Registry: https://registry.vixcpp.com/
- GitHub: https://github.com/vixcpp/vix

## License

MIT License.

See [`LICENSE`](../../LICENSE) for details.
