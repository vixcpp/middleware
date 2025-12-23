#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <boost/beast/http.hpp>

#include <vix/middleware/pipeline.hpp>
#include <vix/middleware/performance/static_files.hpp>

using namespace vix::middleware;

static vix::vhttp::RawRequest make_req(std::string target)
{
    namespace http = boost::beast::http;
    vix::vhttp::RawRequest req{http::verb::get, target, 11};
    req.set(http::field::host, "localhost");
    return req;
}

int main()
{
    namespace http = boost::beast::http;

    // temp dir
    const auto root = std::filesystem::temp_directory_path() / "vix_static_smoke";
    std::filesystem::create_directories(root);
    {
        std::ofstream f(root / "index.html");
        f << "<h1>OK</h1>";
    }

    HttpPipeline p;
    p.use(performance::static_files(root, {.mount = "/", .index_file = "index.html"}));

    auto raw = make_req("/");
    http::response<http::string_body> res;
    vix::vhttp::Request req(raw, {});
    vix::vhttp::ResponseWrapper w(res);

    p.run(req, w, [&](Request &, Response &)
          { w.status(404).text("nope"); });

    assert(res.result_int() == 200);
    assert(res.body().find("OK") != std::string::npos);

    std::cout << "[OK] static_files smoke\n";
    return 0;
}
