#include <cassert>
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/basics/body_limit.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(boost::beast::http::verb method,
                                       std::string target,
                                       std::string body = "",
                                       std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{method, target, 11};
    req.set(http::field::host, "localhost");

    for (auto &kv : headers)
        req.set(kv.first, kv.second);

    req.body() = std::move(body);
    req.prepare_payload();
    return req;
}

static void test_rejects_large_body()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::post, "/upload", std::string(20, 'x'));
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.use(vix::middleware::basics::body_limit({.max_bytes = 10}));

    int final_calls = 0;
    p.run(req, w, [&](Request &, Response &)
          {
              final_calls++;
              w.ok().text("OK"); });

    assert(final_calls == 0);
    assert(res.result_int() == 413);
    assert(res.find("content-type") != res.end());

    std::cout << "[OK] body_limit rejects large body\n";
}

static void test_allows_small_body()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::post, "/upload", "hello");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.use(vix::middleware::basics::body_limit({.max_bytes = 10}));

    int final_calls = 0;
    p.run(req, w, [&](Request &, Response &)
          {
              final_calls++;
              w.ok().text("OK"); });

    assert(final_calls == 1);
    assert(res.result_int() == 200);
    assert(res.body() == "OK");

    std::cout << "[OK] body_limit allows small body\n";
}

int main()
{
    test_rejects_large_body();
    test_allows_small_body();
    std::cout << "OK: body_limit smoke tests passed\n";
    return 0;
}
