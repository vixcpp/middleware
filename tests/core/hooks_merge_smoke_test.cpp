#include <cassert>
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/core/hooks.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req()
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::get, "/x", 11};
    req.set(http::field::host, "localhost");
    req.prepare_payload();
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    auto raw = make_req();
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;

    std::string trace;

    Hooks a;
    a.on_begin = [&](Context &)
    { trace += "A"; };
    a.on_end = [&](Context &)
    { trace += "a"; };

    Hooks b;
    b.on_begin = [&](Context &)
    { trace += "B"; };
    b.on_end = [&](Context &)
    { trace += "b"; };

    p.set_hooks(merge_hooks(a, b));

    p.run(req, w, [&](Request &, Response &)
          {
        trace += "F";
        w.ok().text("OK"); });

    // begin: A B, end: b a
    assert(trace == "ABFba");
    std::cout << "[OK] merge_hooks order\n";
    return 0;
}
