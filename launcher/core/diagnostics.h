// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_DIAGNOSTICS_H_
#define ELIXIR_LAUNCHER_CORE_DIAGNOSTICS_H_

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/config.h"
#include "core/paths.h"

namespace elixir::launcher {

struct WindowsInfo {
  DWORD major = 0;
  DWORD minor = 0;
  DWORD build = 0;
  DWORD ubr = 0;
  std::wstring product_name;
  std::wstring display_version;
  std::wstring edition;
  bool rtl_get_version_ok = false;
};

enum class ProbeOutcome { kNotRun, kWorks, kFaulted };

struct CpuInfo {
  std::string vendor;
  std::string brand;
  unsigned logical_processors = 0;
  unsigned physical_cores = 0;
  bool fsgsbase_cpuid = false;
  ProbeOutcome rdfsbase_probe = ProbeOutcome::kNotRun;
  DWORD rdfsbase_exception = 0;
  std::uint64_t rdfsbase_value = 0;
  bool sse42 = false;
  bool avx2 = false;
};

struct MemoryInfo {
  std::uint64_t installed_physical = 0;
  std::uint64_t total_physical = 0;
  std::uint64_t available_physical = 0;
  DWORD load_percent = 0;
};

struct GpuInfo {
  std::wstring name;
  std::uint32_t vendor_id = 0;
  std::uint32_t device_id = 0;
  std::uint32_t subsystem_id = 0;
  std::uint32_t revision = 0;
  std::uint64_t luid = 0;
  std::uint64_t dedicated_video_memory = 0;
  std::uint64_t dedicated_system_memory = 0;
  std::uint64_t shared_system_memory = 0;
  std::wstring driver_version;
  bool software = false;
  bool mirrors_previous = false;
};

struct VulkanDeviceInfo {
  std::string name;
  std::uint32_t api_version = 0;
  std::uint32_t driver_version = 0;
  std::uint32_t vendor_id = 0;
  std::uint32_t device_id = 0;
  std::string device_type;
  std::string api_version_text;
  std::string driver_version_text;
  std::string driver_name;
  std::string driver_info;
};

struct VulkanInfo {
  bool loader_found = false;
  std::wstring loader_path;
  std::uint32_t instance_version = 0;
  std::string instance_version_text;
  bool instance_created = false;
  int create_result = 0;
  std::vector<VulkanDeviceInfo> devices;
  std::wstring error;
};

struct DiagnosticsReport {
  std::string generated_at;
  WindowsInfo windows;
  CpuInfo cpu;
  MemoryInfo memory;
  std::vector<GpuInfo> gpus;
  VulkanInfo vulkan;
  LauncherPaths paths;
  bool runtime_present = false;
  bool guest_runtime_present = false;
  unsigned guest_runtime_libraries = 0;
};

WindowsInfo CollectWindowsInfo();
CpuInfo CollectCpuInfo();
MemoryInfo CollectMemoryInfo();
std::vector<GpuInfo> CollectGpus();
VulkanInfo CollectVulkan();
DiagnosticsReport CollectDiagnostics(const LauncherPaths& paths);

std::string VulkanVersionText(std::uint32_t version);
std::string VulkanDriverVersionText(std::uint32_t vendor_id, std::uint32_t version);
std::wstring WindowsVersionText(const WindowsInfo& info);

std::string FormatDiagnosticsReport(const DiagnosticsReport& report, const LauncherConfig& config);

enum class CheckStatus { kPass, kWarn, kFail, kPending };

struct ReadinessCheck {
  std::wstring title;
  std::wstring detail;
  CheckStatus status = CheckStatus::kPending;
};

struct Readiness {
  std::vector<ReadinessCheck> checks;
  bool ready_to_play = false;
  std::wstring summary;
};

Readiness EvaluateReadiness(const DiagnosticsReport* report, const LauncherConfig& config,
                            const LauncherPaths& paths);

}

#endif
