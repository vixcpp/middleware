#include <cassert>
#include <iostream>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/auth/api_key.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(bool with_key)
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::get, "/secure", 11};
    req.set(http::field::host, "localhost");
    if (with_key)
        req.set("x-api-key", "secret");
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    HttpPipeline p;

    auth::ApiKeyOptions opt{};
    opt.allowed_keys.insert("secret");

    p.use(auth::api_key(opt));

    {
        auto raw = make_req(true);
        http::response<http::string_body> res;
        vix::vhttp::Request req(raw, {});
        vix::vhttp::ResponseWrapper w(res);

        p.run(req, w, [&](Request &r, Response &)
              {
                  auto &k = r.state<auth::ApiKey>();
                  assert(k.value == "secret");
                  w.ok().text("OK"); });

        assert(res.result_int() == 200);
    }

    {
        auto raw = make_req(false);
        http::response<http::string_body> res;
        vix::vhttp::Request req(raw, {});
        vix::vhttp::ResponseWrapper w(res);

        p.run(req, w, [&](Request &, Response &) {});

        assert(res.result_int() == 401);
    }

    std::cout << "[OK] api_key middleware\n";
    return 0;
}
