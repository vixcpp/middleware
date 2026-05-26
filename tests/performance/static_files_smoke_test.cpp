/**
 *
 *  @file static_compression_smoke_test.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <vix/app/App.hpp>
#include <vix/middleware/performance/static_compression.hpp>

int main()
{
  const auto root =
      std::filesystem::temp_directory_path() / "vix_static_compression_smoke";

  std::filesystem::create_directories(root);

  {
    std::ofstream f(root / "index.html");
    f << "<h1>OK</h1>";
  }

  vix::App app;

  auto handler = vix::middleware::performance::compressed_static_handler({
      .min_size = 8,
      .add_vary = true,
      .enabled = true,
  });

  const bool installed = handler(
      app,
      root,
      "/",
      "index.html",
      true,
      "public, max-age=3600",
      true);

  assert(installed);

  std::cout << "[OK] static_compression smoke\n";
  return 0;
}
