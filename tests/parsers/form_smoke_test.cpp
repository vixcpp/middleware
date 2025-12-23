#include <cassert>
#include <iostream>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/parsers/form.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string body, std::string ct)
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::post, "/form", 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::content_type, ct);
    req.body() = std::move(body);
    req.prepare_payload();
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    auto raw = make_req("a=1&b=hello+world", "application/x-www-form-urlencoded");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.use(vix::middleware::parsers::form());

    p.run(req, w, [&](Request &, Response &)
          {
              auto &fb = req.state<vix::middleware::parsers::FormBody>();
              w.ok().text(fb.fields["b"]); });

    assert(res.result_int() == 200);
    assert(res.body() == "hello world");

    std::cout << "[OK] form parser\n";
    return 0;
}
