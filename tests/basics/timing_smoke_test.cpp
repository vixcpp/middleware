#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/basics/timing.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(
    boost::beast::http::verb method,
    std::string target)
{
    namespace http = boost::beast::http;

    vix::vhttp::RawRequest req{method, target, 11};
    req.set(http::field::host, "localhost");
    req.prepare_payload();
    return req;
}

static void test_timing_sets_headers_and_state()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/slow");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.use(vix::middleware::basics::timing({}));

    p.run(req, w, [&](Request &, Response &)
          {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        w.ok().text("OK"); });

    // x-response-time
    auto xrt = res.find("x-response-time");
    assert(xrt != res.end());
    const std::string xrtv = std::string(xrt->value());
    assert(xrtv.size() >= 3);
    assert(xrtv.find("ms") != std::string::npos);

    // server-timing
    auto st = res.find("server-timing");
    assert(st != res.end());
    const std::string stv = std::string(st->value());
    assert(stv.find("total;dur=") != std::string::npos);

    // typed state
    auto *t = req.try_state<vix::middleware::basics::Timing>();
    assert(t != nullptr);
    assert(t->total_ms >= 0);

    std::cout << "[OK] timing sets headers + stores state\n";
}

static void test_timing_can_be_disabled()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/x");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    vix::middleware::basics::TimingOptions opt;
    opt.set_x_response_time = false;
    opt.set_server_timing = false;

    HttpPipeline p;
    p.use(vix::middleware::basics::timing(opt));

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("OK"); });

    assert(res.find("x-response-time") == res.end());
    assert(res.find("server-timing") == res.end());

    // still stores state by default (store_in_state=true)
    auto *t = req.try_state<vix::middleware::basics::Timing>();
    assert(t != nullptr);

    std::cout << "[OK] timing can disable headers\n";
}

int main()
{
    test_timing_sets_headers_and_state();
    test_timing_can_be_disabled();

    std::cout << "OK: timing smoke tests passed\n";
    return 0;
}
