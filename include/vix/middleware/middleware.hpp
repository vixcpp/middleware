#pragma once

#include <functional>

#include <vix/http/RequestHandler.hpp>

namespace vix::middleware
{
    using Request = vix::vhttp::Request;
    using Response = vix::vhttp::ResponseWrapper;
    using Next = std::function<void()>;
    using HttpMiddleware = std::function<void(Request &, Response &, Next)>;

} // namespace vix::middleware
