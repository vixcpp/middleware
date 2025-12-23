#include <cassert>
#include <iostream>

#include <boost/beast/http.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/security/cors.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_preflight()
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::options, "/api", 11};
    req.set(http::field::host, "localhost");
    req.set("Origin", "https://example.com");
    req.set("Access-Control-Request-Method", "POST");
    req.prepare_payload();
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    auto raw = make_preflight();
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    vix::middleware::security::CorsOptions opt;
    opt.allowed_origins = {"https://example.com"};
    opt.allow_any_origin = false;

    HttpPipeline p;
    p.use(vix::middleware::security::cors(opt));

    int final_calls = 0;
    p.run(req, w, [&](Request &, Response &)
          { final_calls++; w.ok().text("OK"); });

    assert(final_calls == 0);
    assert(res.result_int() == 204);
    assert(res.find("Access-Control-Allow-Origin") != res.end());

    std::cout << "[OK] cors preflight\n";
    return 0;
}
