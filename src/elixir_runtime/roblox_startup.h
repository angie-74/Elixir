// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_ROBLOX_STARTUP_H_
#define ELIXIR_RUNTIME_ROBLOX_STARTUP_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "runtime/display_size.h"
#include "runtime/runtime_config.h"

namespace jnivm {
class VM;
}

namespace elixir::runtime {

void RegisterRobloxJavaClasses(jnivm::VM* vm);

struct RobloxStartupOptions {
  std::string package_name = "com.roblox.client";
  std::string version_name;
  std::string asset_path;
  std::string client_settings_json;
  std::string client_settings_overrides_json = "{}";
  std::string fast_flags_json = "{}";
  std::string user_agent;
  mocktail::runtime::DeviceProfile device;
  mocktail::runtime::InputCapabilityConfig input;
  std::uint64_t host_memory_bytes = 0;
  mocktail::runtime::DisplaySize display;
  mocktail::runtime::DisplaySize window;
  float dpi_scale = 1.0f;
  float surface_scale = 1.0f;
  bool headless = false;

  void* native_window = nullptr;
  float refresh_hz = 0.0f;
  std::vector<float> supported_refresh_hz;
  std::string theme = "Dark";
};

void RunPendingMainThreadWork();

struct RobloxStartupResult {
  bool ok = false;
  bool stopped = false;
  bool timed_out = false;
  std::vector<std::string> missing;
  std::string step;
  std::string error;
  int steps_run = 0;
  std::uint64_t elapsed_ms = 0;
  void* surface = nullptr;
};

RobloxStartupResult RunRobloxStartup(void* roblox, jnivm::VM* vm, const RobloxStartupOptions& options,
                                     const std::atomic<bool>& stop, unsigned timeout_ms,
                                     const std::function<void()>& pump = {});

bool PassDisplayRefreshRates(void* roblox, jnivm::VM* vm, float current_hz, const std::vector<float>& supported_hz);
std::string DescribeRefreshRates(float current_hz, const std::vector<float>& supported_hz);

bool ShutdownRobloxApp(void* roblox, jnivm::VM* vm, std::string* error);

}

#endif
