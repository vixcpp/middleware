#include <cassert>
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/observability/tracing.hpp>

using namespace vix::middleware;
using namespace vix::middleware::observability;

static vix::vhttp::RawRequest make_req(boost::beast::http::verb method,
                                       std::string target,
                                       std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{method, target, 11};
    req.set(http::field::host, "localhost");
    for (auto &kv : headers)
        req.set(kv.first, kv.second);
    req.prepare_payload();
    return req;
}

static void test_tracing_hooks_generates_and_sets_headers()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/t");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.set_hooks(tracing_hooks());

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("OK"); });

    assert(res.result_int() == 200);
    assert(res.body() == "OK");

    // response headers should exist
    auto trace = res.find("x-trace-id");
    auto span = res.find("x-span-id");
    assert(trace != res.end());
    assert(span != res.end());

    std::cout << "[OK] tracing hooks headers\n";
}

static void test_tracing_mw_accepts_incoming_trace()
{
    namespace http = boost::beast::http;

    const std::string incoming_trace = "0123456789abcdef0123456789abcdef"; // 32 hex
    auto raw = make_req(http::verb::get, "/t2", {{"x-trace-id", incoming_trace}});
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.use(tracing_mw());

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("OK"); });

    auto trace = res.find("x-trace-id");
    assert(trace != res.end());

    // should keep incoming trace id
    assert(std::string(trace->value()) == incoming_trace);

    std::cout << "[OK] tracing middleware incoming trace\n";
}

int main()
{
    test_tracing_hooks_generates_and_sets_headers();
    test_tracing_mw_accepts_incoming_trace();
    std::cout << "OK: observability tracing smoke tests passed\n";
    return 0;
}
