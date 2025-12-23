#include <cassert>
#include <iostream>

#include <vix/middleware/utils/json_writer.hpp>

int main()
{
    using namespace vix::middleware::utils;

    JsonWriter w;
    w.begin_obj();
    w.key("ok");
    w.boolean(true);
    w.key("msg");
    w.string("hello");
    w.end_obj();

    auto s = w.str();
    assert(s.find("\"ok\":true") != std::string::npos);
    assert(s.find("\"msg\":\"hello\"") != std::string::npos);

    std::cout << "[OK] json_writer\n";
    return 0;
}
