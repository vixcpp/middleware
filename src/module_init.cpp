#include <vix/middleware/static_dir_bridge.hpp>

extern "C" void vix_middleware_module_init()
{
  vix::middleware::register_static_dir();
}
