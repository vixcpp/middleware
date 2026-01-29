#include <vix/middleware/static_dir_bridge.hpp>

namespace
{
  struct AutoRegisterStaticDir
  {
    AutoRegisterStaticDir()
    {
      vix::middleware::register_static_dir();
    }
  };

  static AutoRegisterStaticDir g_auto_register{};
}
