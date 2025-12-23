#include <cassert>
#include <iostream>
#include <unordered_map>

#include <vix/middleware/utils/key_builder.hpp>

int main()
{
    using namespace vix::middleware::utils;

    std::unordered_map<std::string, std::string> h{
        {"Accept", "application/json"},
        {"X-Test", "1"}};

    KeyBuilder kb;
    kb.add("GET").add("/api").add_kv("q", "x");
    kb.add_headers_sorted(h, {"Accept"});

    auto key = kb.str();
    assert(key.find("GET") != std::string::npos);
    assert(key.find("h|accept=application/json") != std::string::npos);

    std::cout << "[OK] key_builder\n";
    return 0;
}
