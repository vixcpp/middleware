#include <cassert>
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/http/RequestHandler.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string target = "/")
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::get, target, 11};
    req.set(http::field::host, "localhost");
    return req;
}

static void test_pipeline_order_and_final()
{
    namespace http = boost::beast::http;

    auto raw = make_req("/x");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;

    std::string trace;

    p.use([&](Request &, Response &, Next next)
          {
        trace += "A";
        next();
        trace += "a"; });

    p.use([&](Request &, Response &, Next next)
          {
        trace += "B";
        next();
        trace += "b"; });

    p.run(req, w, [&](Request &, Response &)
          {
        trace += "F";
        w.ok().text("OK"); });

    assert(trace == "ABFba");
    assert(res.result_int() == 200);
    assert(res.body() == "OK");

    std::cout << "[OK] pipeline order + final\n";
}

static void test_pipeline_short_circuit()
{
    namespace http = boost::beast::http;

    auto raw = make_req("/short");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    int called = 0;

    p.use([&](Request &, Response &, Next)
          {
              called++;
              w.status(403).text("blocked");
              // no next() => stop chain
          });

    p.use([&](Request &, Response &, Next next)
          {
        called++;
        next(); });

    p.run(req, w, [&](Request &, Response &)
          {
        called++;
        w.ok().text("final"); });

    assert(called == 1);
    assert(res.result_int() == 403);
    assert(res.body() == "blocked");

    std::cout << "[OK] pipeline short-circuit\n";
}

int main()
{
    test_pipeline_order_and_final();
    test_pipeline_short_circuit();
    std::cout << "OK: middleware pipeline smoke tests passed\n";
    return 0;
}
