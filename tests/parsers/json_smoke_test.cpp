#include <cassert>
#include <iostream>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/parsers/json.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string body, std::string ct)
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::post, "/json", 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::content_type, ct);
    req.body() = std::move(body);
    req.prepare_payload();
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    auto raw = make_req(R"({"x":1})", "application/json; charset=utf-8");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;
    p.use(vix::middleware::parsers::json());

    p.run(req, w, [&](Request &, Response &)
          {
              auto &jb = req.state<vix::middleware::parsers::JsonBody>();
              w.ok().text(jb.value["x"].dump()); });

    assert(res.result_int() == 200);
    assert(res.body() == "1");

    std::cout << "[OK] json parser\n";
    return 0;
}
