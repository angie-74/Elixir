// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_RUNTIME_CONFIG_FILE_H_
#define MOCKTAIL_RUNTIME_RUNTIME_CONFIG_FILE_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "runtime/environment.h"
#include "runtime/runtime_config.h"

namespace mocktail {
namespace runtime {

#if defined(_WIN32)
struct LauncherConfigValues {
  int schema = 0;
  std::string renderer = "vulkan";
  std::string present_mode = "fifo";
  double resolution_scale = 1.0;
  double stretch = 0.0;
  bool high_priority = false;
  bool disable_power_throttling = true;
  bool timer_resolution_1ms = true;
  bool mmcss = true;
  std::string audio_output_device_id;
  std::string audio_input_device_id;
  std::string fflags_path;
  std::string installed_package;
  std::int64_t installed_version_code = 0;
  std::string installed_install_dir;
};
#endif

struct RuntimeConfigLoadResult {
  RuntimeConfig config;
  bool file_loaded = false;
  std::string error;
#if defined(_WIN32)
  LauncherConfigValues launcher;
  std::vector<std::string> warnings;
#endif

  explicit operator bool() const { return error.empty(); }
};

RuntimeConfigLoadResult LoadRuntimeConfig(
    const Environment& environment, const std::filesystem::path& config_file);

bool ExportRuntimeConfigEnvironment(const RuntimeConfig& config,
                                    std::string* error = nullptr);
bool ExportNetworkProxyEnvironment(const RuntimeConfig& config,
                                   std::string* error = nullptr);

}
}

#endif
