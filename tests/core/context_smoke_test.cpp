#include <cassert>
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/middleware.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(boost::beast::http::verb method, std::string target)
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{method, target, 11};
    req.set(http::field::host, "localhost");
    req.prepare_payload();
    return req;
}

struct FooState
{
    int x{0};
};

static void test_context_state_roundtrip()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/ctx");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;

    p.use([](Context &ctx, Next next)
          {
        ctx.set_state(FooState{42});
        next(); });

    p.run(req, w, [&](Request &, Response &)
          {
        auto* st = req.try_state<FooState>();
        assert(st != nullptr);
        assert(st->x == 42);
        w.ok().text("OK"); });

    assert(res.result_int() == 200);
    assert(res.body() == "OK");

    std::cout << "[OK] Context state roundtrip\n";
}

static void test_context_send_error()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/ctx-error");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;

    p.use([](Context &ctx, Next)
          {
              Error e;
              e.status = 401;
              e.code = "unauthorized";
              e.message = "Missing token";
              e.details["hint"] = "Provide Authorization: Bearer <token>";
              ctx.send_error(e);
              // no next => short-circuit
          });

    p.run(req, w, [&](Request &, Response &)
          {
        // should not execute
        w.ok().text("NO"); });

    assert(res.result_int() == 401);
    assert(res.body().find("\"code\"") != std::string::npos);
    assert(res.body().find("unauthorized") != std::string::npos);
    assert(res.body().find("Missing token") != std::string::npos);

    std::cout << "[OK] Context send_error\n";
}

int main()
{
    test_context_state_roundtrip();
    test_context_send_error();

    std::cout << "OK: core context smoke tests passed\n";
    return 0;
}
