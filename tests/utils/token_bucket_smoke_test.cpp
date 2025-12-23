#include <cassert>
#include <iostream>

#include <vix/middleware/utils/token_bucket.hpp>

int main()
{
    using vix::middleware::utils::TokenBucket;

    TokenBucket b(2.0, 0.0);

    assert(b.try_consume(1.0) == true);
    assert(b.try_consume(1.0) == true);
    assert(b.try_consume(1.0) == false);

    std::cout << "[OK] token_bucket basic\n";
    return 0;
}
