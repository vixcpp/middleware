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

static void test_hooks_begin_end_called()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/hooks");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;

    int begin = 0;
    int end = 0;
    int err = 0;

    Hooks h;
    h.on_begin = [&](Context &)
    { begin++; };
    h.on_end = [&](Context &)
    { end++; };
    h.on_error = [&](Context &, const Error &)
    { err++; };
    p.set_hooks(std::move(h));

    // trivial middleware
    p.use([](Context &, Next next)
          { next(); });

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text("OK"); });

    assert(begin == 1);
    assert(end == 1);
    assert(err == 0);
    assert(res.result_int() == 200);
    assert(res.body() == "OK");

    std::cout << "[OK] hooks begin/end called\n";
}

static void test_hooks_error_called_when_exception_escapes()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/boom");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;

    int begin = 0;
    int end = 0;
    int err = 0;
    Error last;

    Hooks h;
    h.on_begin = [&](Context &)
    { begin++; };
    h.on_end = [&](Context &)
    { end++; };
    h.on_error = [&](Context &, const Error &e)
    { err++; last = e; };
    p.set_hooks(std::move(h));

    bool threw = false;
    try
    {
        p.run(req, w, [&](Request &, Response &)
              { throw std::runtime_error("kaboom"); });
    }
    catch (...)
    {
        threw = true;
    }

    assert(threw == true);
    assert(begin == 1);
    assert(end == 0); // because exception escaped
    assert(err == 1);
    assert(last.status == 500);
    assert(last.code == "unhandled_exception");

    std::cout << "[OK] hooks on_error called when exception escapes\n";
}

int main()
{
    test_hooks_begin_end_called();
    test_hooks_error_called_when_exception_escapes();

    std::cout << "OK: hooks smoke tests passed\n";
    return 0;
}
