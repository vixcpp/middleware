#include <cassert>
#include <iostream>

#include <boost/beast/http.hpp>
#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/security/csrf.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(bool ok)
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::post, "/x", 11};
    req.set(http::field::host, "localhost");

    if (ok)
    {
        req.set("Cookie", "csrf_token=abc");
        req.set("X-CSRF-Token", "abc");
    }
    else
    {
        req.set("Cookie", "csrf_token=abc");
        req.set("X-CSRF-Token", "nope");
    }

    req.body() = "x=1";
    req.prepare_payload();
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    {
        auto raw = make_req(false);
        http::response<http::string_body> res;
        vix::vhttp::Request req(raw, {});
        vix::vhttp::ResponseWrapper w(res);

        HttpPipeline p;
        p.use(vix::middleware::security::csrf());

        int final_calls = 0;
        p.run(req, w, [&](Request &, Response &)
              { final_calls++; w.ok().text("OK"); });

        assert(final_calls == 0);
        assert(res.result_int() == 403);
    }

    {
        auto raw = make_req(true);
        http::response<http::string_body> res;
        vix::vhttp::Request req(raw, {});
        vix::vhttp::ResponseWrapper w(res);

        HttpPipeline p;
        p.use(vix::middleware::security::csrf());

        int final_calls = 0;
        p.run(req, w, [&](Request &, Response &)
              { final_calls++; w.ok().text("OK"); });

        assert(final_calls == 1);
        assert(res.result_int() == 200);
    }

    std::cout << "[OK] csrf\n";
    return 0;
}
