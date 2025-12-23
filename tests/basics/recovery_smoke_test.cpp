#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/basics/request_id.hpp>
#include <vix/middleware/basics/recovery.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(
    boost::beast::http::verb method,
    std::string target,
    std::initializer_list<std::pair<std::string, std::string>> headers = {})
{
    namespace http = boost::beast::http;

    vix::vhttp::RawRequest req{method, target, 11};
    req.set(http::field::host, "localhost");
    for (auto &kv : headers)
        req.set(kv.first, kv.second);
    req.prepare_payload();
    return req;
}

// A tiny logger impl for the DI test
struct TestLogger final : vix::middleware::basics::IRecoveryLogger
{
    int errors{0};
    std::string last;

    void error(std::string_view msg) override
    {
        errors++;
        last = std::string(msg);
    }
};

static void test_recovery_catches_exception_and_returns_json()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/boom");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;

    // request id first (so recovery response includes header)
    p.use(vix::middleware::basics::request_id());

    // recovery
    vix::middleware::basics::RecoveryOptions opt;
    opt.include_exception_message = true;
    p.use(vix::middleware::basics::recovery(opt));

    // final throws
    p.run(req, w, [&](Request &, Response &)
          { throw std::runtime_error("kaboom"); });

    assert(res.result_int() == 500);

    // content-type json
    auto ct = res.find(http::field::content_type);
    assert(ct != res.end());
    assert(std::string(ct->value()).find("application/json") != std::string::npos);

    // x-request-id exists
    auto rid = res.find("x-request-id");
    assert(rid != res.end());
    assert(!std::string(rid->value()).empty());

    // body contains our fields
    const std::string body = res.body();
    assert(body.find("\"code\"") != std::string::npos);
    assert(body.find("internal_server_error") != std::string::npos);
    assert(body.find("\"exception\"") != std::string::npos);
    assert(body.find("kaboom") != std::string::npos);

    std::cout << "[OK] recovery catches exception and returns JSON\n";
}

static void test_recovery_logs_via_services_if_present()
{
    namespace http = boost::beast::http;

    auto raw = make_req(http::verb::get, "/boom2");
    http::response<http::string_body> res;

    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    HttpPipeline p;

    auto logger = std::make_shared<TestLogger>();
    p.services().provide<vix::middleware::basics::IRecoveryLogger>(logger);

    p.use(vix::middleware::basics::recovery({}));

    p.run(req, w, [&](Request &, Response &)
          { throw std::runtime_error("oops"); });

    assert(res.result_int() == 500);
    assert(logger->errors == 1);
    assert(logger->last.find("Unhandled exception") != std::string::npos);

    std::cout << "[OK] recovery logs via services (DI)\n";
}

int main()
{
    test_recovery_catches_exception_and_returns_json();
    test_recovery_logs_via_services_if_present();

    std::cout << "OK: recovery smoke tests passed\n";
    return 0;
}
