#include <cassert>
#include <iostream>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/performance/compression.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string target, std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::get, target, 11};
    req.set(http::field::host, "localhost");
    for (auto &kv : headers)
        req.set(kv.first, kv.second);
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    HttpPipeline p;
    p.use(performance::compression({.min_size = 8}));

    auto raw = make_req("/x", {{"Accept-Encoding", "gzip, br"}});
    http::response<http::string_body> res;
    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &)
          { w.ok().text(std::string(20, 'a')); });

    assert(res.result_int() == 200);
    // We don't set Content-Encoding in v1
    // But Vary must exist (append)
    assert(!std::string(res["Vary"]).empty());

#ifndef NDEBUG
    assert(std::string(res["X-Vix-Compression"]) == "planned");
#endif

    std::cout << "[OK] compression smoke\n";
    return 0;
}
