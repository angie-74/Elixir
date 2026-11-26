// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/diagnostics.h"

#include <dxgi.h>
#include <intrin.h>
#include <wrl/client.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

#include <cstdio>
#include <cstring>

#include "core/util.h"

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif

namespace elixir::launcher {

namespace {

using Microsoft::WRL::ComPtr;

std::wstring RegistryString(const wchar_t* name) {
  wchar_t buffer[256] = {};
  DWORD size = sizeof(buffer);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", name, RRF_RT_REG_SZ,
                   nullptr, buffer, &size) != ERROR_SUCCESS) {
    return {};
  }
  return buffer;
}

DWORD RegistryDword(const wchar_t* name) {
  DWORD value = 0;
  DWORD size = sizeof(value);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", name, RRF_RT_REG_DWORD,
                   nullptr, &value, &size) != ERROR_SUCCESS) {
    return 0;
  }
  return value;
}

__attribute__((noinline, target("fsgsbase"))) unsigned long long ReadFsBaseInstruction() {
  unsigned long long value = 0;
  __asm__ volatile("rdfsbase %0" : "=r"(value));
  return value;
}

ProbeOutcome ProbeRdfsbase(unsigned long long* value, DWORD* fault_code) {
  __try {
    *value = ReadFsBaseInstruction();
    return ProbeOutcome::kWorks;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    *fault_code = GetExceptionCode();
    return ProbeOutcome::kFaulted;
  }
}

constexpr std::uint32_t kMaxVulkanDevices = 16;

struct RawVulkanDevice {
  VkPhysicalDeviceProperties properties;
  bool has_driver_properties;
  VkPhysicalDeviceDriverProperties driver;
};

struct RawVulkanProbe {
  std::uint32_t instance_version;
  VkResult create_result;
  bool created;
  std::uint32_t device_count;
  RawVulkanDevice devices[kMaxVulkanDevices];
};

void VulkanProbeUnsafe(PFN_vkGetInstanceProcAddr get_proc, RawVulkanProbe* out) {
  out->instance_version = VK_API_VERSION_1_0;
  auto enumerate_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
      get_proc(nullptr, "vkEnumerateInstanceVersion"));
  if (enumerate_version != nullptr) enumerate_version(&out->instance_version);
  auto create_instance = reinterpret_cast<PFN_vkCreateInstance>(get_proc(nullptr, "vkCreateInstance"));
  if (create_instance == nullptr) {
    out->create_result = VK_ERROR_INITIALIZATION_FAILED;
    return;
  }
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "Elixir launcher diagnostics";
  app.applicationVersion = 1;
  app.pEngineName = "Elixir";
  app.engineVersion = 1;
  app.apiVersion = out->instance_version >= VK_API_VERSION_1_3 ? VK_API_VERSION_1_3
                   : out->instance_version >= VK_API_VERSION_1_1 ? out->instance_version
                                                                 : VK_API_VERSION_1_0;
  VkInstanceCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pApplicationInfo = &app;
  VkInstance instance = VK_NULL_HANDLE;
  out->create_result = create_instance(&create, nullptr, &instance);
  if (out->create_result != VK_SUCCESS) return;
  out->created = true;
  auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(get_proc(instance, "vkDestroyInstance"));
  auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get_proc(instance, "vkEnumeratePhysicalDevices"));
  auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
      get_proc(instance, "vkGetPhysicalDeviceProperties"));
  auto properties2 = app.apiVersion >= VK_API_VERSION_1_1
                         ? reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
                               get_proc(instance, "vkGetPhysicalDeviceProperties2"))
                         : nullptr;
  if (enumerate != nullptr && properties != nullptr) {
    VkPhysicalDevice devices[kMaxVulkanDevices];
    std::uint32_t count = kMaxVulkanDevices;
    const VkResult result = enumerate(instance, &count, devices);
    if (result == VK_SUCCESS || result == VK_INCOMPLETE) {
      out->device_count = count;
      for (std::uint32_t i = 0; i < count; ++i) {
        properties(devices[i], &out->devices[i].properties);
        if (properties2 != nullptr && out->devices[i].properties.apiVersion >= VK_API_VERSION_1_2) {
          VkPhysicalDeviceDriverProperties driver{};
          driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
          VkPhysicalDeviceProperties2 chain{};
          chain.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
          chain.pNext = &driver;
          properties2(devices[i], &chain);
          driver.pNext = nullptr;
          out->devices[i].driver = driver;
          out->devices[i].has_driver_properties = true;
        }
      }
    }
  }
  if (destroy != nullptr) destroy(instance, nullptr);
}

DWORD VulkanProbeGuarded(PFN_vkGetInstanceProcAddr get_proc, RawVulkanProbe* out) {
  __try {
    VulkanProbeUnsafe(get_proc, out);
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return GetExceptionCode();
  }
}

const char* DeviceTypeName(VkPhysicalDeviceType type) {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU (software)";
    default: return "other";
  }
}

std::string BoundedString(const char* text, std::size_t capacity) {
  std::size_t length = 0;
  while (length < capacity && text[length] != '\0') ++length;
  return std::string(text, length);
}

std::string HexId(std::uint32_t value) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "0x%04X", value);
  return buffer;
}

std::string Narrow(const std::wstring& text) { return WideToUtf8(text); }

void Line(std::string* out, const char* key, const std::string& value) {
  out->append("  ");
  out->append(key);
  const std::size_t width = std::strlen(key);
  out->append(width < 26 ? 26 - width : 1, ' ');
  out->append(value);
  out->append("\r\n");
}

}

std::string VulkanVersionText(std::uint32_t version) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%u.%u.%u", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version),
                VK_API_VERSION_PATCH(version));
  return buffer;
}

std::string VulkanDriverVersionText(std::uint32_t vendor_id, std::uint32_t version) {
  char buffer[48];
  if (vendor_id == 0x10DE) {
    std::snprintf(buffer, sizeof(buffer), "%u.%02u", (version >> 22) & 0x3FF, (version >> 14) & 0xFF);
  } else if (vendor_id == 0x8086) {
    std::snprintf(buffer, sizeof(buffer), "%u.%u", version >> 14, version & 0x3FFF);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%u.%u.%u", version >> 22, (version >> 12) & 0x3FF, version & 0xFFF);
  }
  return buffer;
}

std::wstring WindowsVersionText(const WindowsInfo& info) {
  std::wstring text = info.product_name.empty() ? L"Windows" : info.product_name;
  if (!info.display_version.empty()) text += L" " + info.display_version;
  wchar_t build[64];
  std::swprintf(build, 64, L" (version %lu.%lu, build %lu.%lu)", static_cast<unsigned long>(info.major),
                static_cast<unsigned long>(info.minor), static_cast<unsigned long>(info.build),
                static_cast<unsigned long>(info.ubr));
  return text + build;
}

WindowsInfo CollectWindowsInfo() {
  WindowsInfo info;
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  auto rtl_get_version =
      ntdll != nullptr ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
  if (rtl_get_version != nullptr) {
    OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtl_get_version(&version) == 0) {
      info.major = version.dwMajorVersion;
      info.minor = version.dwMinorVersion;
      info.build = version.dwBuildNumber;
      info.rtl_get_version_ok = true;
    }
  }
  info.ubr = RegistryDword(L"UBR");
  info.product_name = RegistryString(L"ProductName");
  info.display_version = RegistryString(L"DisplayVersion");
  info.edition = RegistryString(L"EditionID");
  if (info.build >= 22000 && info.product_name.rfind(L"Windows 10", 0) == 0) {
    info.product_name.replace(0, 10, L"Windows 11");
  }
  return info;
}

CpuInfo CollectCpuInfo() {
  CpuInfo info;
  int regs[4] = {};
  __cpuid(regs, 0);
  const int max_leaf = regs[0];
  char vendor[13] = {};
  std::memcpy(vendor + 0, &regs[1], 4);
  std::memcpy(vendor + 4, &regs[3], 4);
  std::memcpy(vendor + 8, &regs[2], 4);
  info.vendor = vendor;
  if (max_leaf >= 1) {
    __cpuid(regs, 1);
    info.sse42 = (regs[2] & (1 << 20)) != 0;
  }
  if (max_leaf >= 7) {
    __cpuidex(regs, 7, 0);
    info.fsgsbase_cpuid = (regs[1] & 1) != 0;
    info.avx2 = (regs[1] & (1 << 5)) != 0;
  }
  __cpuid(regs, static_cast<int>(0x80000000));
  if (static_cast<unsigned>(regs[0]) >= 0x80000004u) {
    char brand[49] = {};
    for (int i = 0; i < 3; ++i) {
      __cpuid(regs, static_cast<int>(0x80000002u + static_cast<unsigned>(i)));
      std::memcpy(brand + i * 16, regs, 16);
    }
    info.brand = TrimAscii(brand);
  }
  info.logical_processors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  DWORD length = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
  if (length > 0) {
    std::vector<std::byte> buffer(length);
    auto* first = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, first, &length)) {
      for (DWORD offset = 0; offset < length;) {
        auto* entry = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
        if (entry->Relationship == RelationProcessorCore) ++info.physical_cores;
        if (entry->Size == 0) break;
        offset += entry->Size;
      }
    }
  }
  if (info.fsgsbase_cpuid) {
    unsigned long long value = 0;
    DWORD code = 0;
    info.rdfsbase_probe = ProbeRdfsbase(&value, &code);
    info.rdfsbase_value = value;
    info.rdfsbase_exception = code;
  }
  return info;
}

MemoryInfo CollectMemoryInfo() {
  MemoryInfo info;
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  if (GlobalMemoryStatusEx(&status)) {
    info.total_physical = status.ullTotalPhys;
    info.available_physical = status.ullAvailPhys;
    info.load_percent = status.dwMemoryLoad;
  }
  ULONGLONG installed_kb = 0;
  if (GetPhysicallyInstalledSystemMemory(&installed_kb)) info.installed_physical = installed_kb * 1024ull;
  return info;
}

std::vector<GpuInfo> CollectGpus() {
  std::vector<GpuInfo> gpus;
  ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return gpus;
  std::vector<LUID> seen;
  for (UINT index = 0;; ++index) {
    ComPtr<IDXGIAdapter1> adapter;
    if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) break;
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc))) continue;
    bool duplicate = false;
    for (const LUID& luid : seen) {
      duplicate |= luid.LowPart == desc.AdapterLuid.LowPart && luid.HighPart == desc.AdapterLuid.HighPart;
    }
    if (duplicate) continue;
    seen.push_back(desc.AdapterLuid);
    GpuInfo gpu;
    gpu.name = desc.Description;
    gpu.vendor_id = desc.VendorId;
    gpu.device_id = desc.DeviceId;
    gpu.subsystem_id = desc.SubSysId;
    gpu.revision = desc.Revision;
    gpu.luid = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(desc.AdapterLuid.HighPart)) << 32) | desc.AdapterLuid.LowPart;
    gpu.dedicated_video_memory = desc.DedicatedVideoMemory;
    gpu.dedicated_system_memory = desc.DedicatedSystemMemory;
    gpu.shared_system_memory = desc.SharedSystemMemory;
    gpu.software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    LARGE_INTEGER umd{};
    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd))) {
      wchar_t version[64];
      std::swprintf(version, 64, L"%u.%u.%u.%u", HIWORD(umd.HighPart), LOWORD(umd.HighPart), HIWORD(umd.LowPart),
                    LOWORD(umd.LowPart));
      gpu.driver_version = version;
    }
    for (const GpuInfo& previous : gpus) {
      gpu.mirrors_previous |= previous.vendor_id == gpu.vendor_id && previous.device_id == gpu.device_id &&
                              previous.subsystem_id == gpu.subsystem_id && previous.revision == gpu.revision &&
                              previous.dedicated_video_memory == gpu.dedicated_video_memory && !gpu.software;
    }
    gpus.push_back(std::move(gpu));
  }
  return gpus;
}

VulkanInfo CollectVulkan() {
  VulkanInfo info;
  HMODULE loader = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (loader == nullptr) {
    info.error = L"vulkan-1.dll could not be loaded: " + Win32ErrorText(GetLastError());
    return info;
  }
  info.loader_found = true;
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(loader, path, MAX_PATH);
  info.loader_path = path;
  auto get_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"));
  if (get_proc == nullptr) {
    info.error = L"vulkan-1.dll does not export vkGetInstanceProcAddr";
    FreeLibrary(loader);
    return info;
  }
  auto* raw = new RawVulkanProbe{};
  const DWORD exception = VulkanProbeGuarded(get_proc, raw);
  info.instance_version = raw->instance_version;
  info.instance_version_text = VulkanVersionText(raw->instance_version);
  info.create_result = raw->create_result;
  info.instance_created = raw->created;
  if (exception != 0) {
    wchar_t text[96];
    std::swprintf(text, 96, L"A Vulkan layer or driver raised exception 0x%08lX during enumeration",
                  static_cast<unsigned long>(exception));
    info.error = text;
  } else if (!raw->created) {
    info.error = L"vkCreateInstance failed with VkResult " + std::to_wstring(static_cast<int>(raw->create_result));
  }
  for (std::uint32_t i = 0; i < raw->device_count && i < kMaxVulkanDevices; ++i) {
    const RawVulkanDevice& source = raw->devices[i];
    VulkanDeviceInfo device;
    device.name = BoundedString(source.properties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
    device.api_version = source.properties.apiVersion;
    device.driver_version = source.properties.driverVersion;
    device.vendor_id = source.properties.vendorID;
    device.device_id = source.properties.deviceID;
    device.device_type = DeviceTypeName(source.properties.deviceType);
    device.api_version_text = VulkanVersionText(device.api_version);
    device.driver_version_text = VulkanDriverVersionText(device.vendor_id, device.driver_version);
    if (source.has_driver_properties) {
      device.driver_name = BoundedString(source.driver.driverName, VK_MAX_DRIVER_NAME_SIZE);
      device.driver_info = BoundedString(source.driver.driverInfo, VK_MAX_DRIVER_INFO_SIZE);
    }
    info.devices.push_back(std::move(device));
  }
  delete raw;
  return info;
}

DiagnosticsReport CollectDiagnostics(const LauncherPaths& paths) {
  DiagnosticsReport report;
  report.generated_at = UtcIsoTimestamp();
  report.windows = CollectWindowsInfo();
  report.cpu = CollectCpuInfo();
  report.memory = CollectMemoryInfo();
  report.gpus = CollectGpus();
  report.vulkan = CollectVulkan();
  report.paths = paths;
  report.runtime_present = PathIsFile(paths.runtime_exe);
  if (PathIsDirectory(paths.guest_runtime_dir)) {
    report.guest_runtime_present = true;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(JoinPath(paths.guest_runtime_dir, L"*.so").c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
      do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++report.guest_runtime_libraries;
      } while (FindNextFileW(find, &data));
      FindClose(find);
    }
  }
  return report;
}

std::string FormatDiagnosticsReport(const DiagnosticsReport& report, const LauncherConfig& config) {
  std::string out;
  out += "Elixir diagnostics report\r\n";
  out += "Launcher version " ELIXIR_VERSION ", generated " + report.generated_at + "\r\n\r\n";

  out += "Windows\r\n";
  Line(&out, "Version", Narrow(WindowsVersionText(report.windows)));
  Line(&out, "Edition", Narrow(report.windows.edition));
  Line(&out, "RtlGetVersion", report.windows.rtl_get_version_ok ? "ok" : "failed");
  out += "\r\n";

  out += "CPU\r\n";
  Line(&out, "Brand", report.cpu.brand.empty() ? "(unavailable)" : report.cpu.brand);
  Line(&out, "Vendor", report.cpu.vendor);
  Line(&out, "Logical processors", std::to_string(report.cpu.logical_processors));
  Line(&out, "Physical cores", std::to_string(report.cpu.physical_cores));
  Line(&out, "FSGSBASE (CPUID.7:EBX[0])", report.cpu.fsgsbase_cpuid ? "supported" : "not supported");
  {
    std::string probe;
    char buffer[96];
    switch (report.cpu.rdfsbase_probe) {
      case ProbeOutcome::kNotRun: probe = "not run (CPU lacks FSGSBASE)"; break;
      case ProbeOutcome::kWorks:
        std::snprintf(buffer, sizeof(buffer), "works in user mode (FS base = 0x%llx)",
                      static_cast<unsigned long long>(report.cpu.rdfsbase_value));
        probe = buffer;
        break;
      case ProbeOutcome::kFaulted:
        std::snprintf(buffer, sizeof(buffer), "faulted with exception 0x%08lX (OS has not enabled FSGSBASE)",
                      static_cast<unsigned long>(report.cpu.rdfsbase_exception));
        probe = buffer;
        break;
    }
    Line(&out, "RDFSBASE probe", probe);
  }
  Line(&out, "SSE4.2 / AVX2", std::string(report.cpu.sse42 ? "yes" : "no") + " / " + (report.cpu.avx2 ? "yes" : "no"));
  out += "\r\n";

  out += "Memory\r\n";
  Line(&out, "Installed RAM",
       report.memory.installed_physical > 0 ? Narrow(FormatByteSize(report.memory.installed_physical))
                                            : std::string("(not reported by the firmware)"));
  Line(&out, "Usable by Windows", Narrow(FormatByteSize(report.memory.total_physical)));
  Line(&out, "Available", Narrow(FormatByteSize(report.memory.available_physical)) + " (" +
                              std::to_string(report.memory.load_percent) + "% in use)");
  out += "\r\n";

  out += "Graphics adapters (DXGI)\r\n";
  if (report.gpus.empty()) out += "  (none enumerated)\r\n";
  for (const GpuInfo& gpu : report.gpus) {
    Line(&out, "Adapter", Narrow(gpu.name) + (gpu.software ? " [software]" : ""));
    Line(&out, "  Vendor / device", HexId(gpu.vendor_id) + " / " + HexId(gpu.device_id));
    {
      char luid[48];
      std::snprintf(luid, sizeof(luid), "%016llx", static_cast<unsigned long long>(gpu.luid));
      Line(&out, "  Adapter LUID", std::string(luid) + (gpu.mirrors_previous ? " (identical to an adapter above; likely the same GPU via a virtual display driver)" : ""));
    }
    Line(&out, "  Dedicated video memory", Narrow(FormatByteSize(gpu.dedicated_video_memory)));
    Line(&out, "  Shared system memory", Narrow(FormatByteSize(gpu.shared_system_memory)));
    Line(&out, "  Driver (UMD) version", gpu.driver_version.empty() ? "(unavailable)" : Narrow(gpu.driver_version));
  }
  out += "\r\n";

  out += "Vulkan\r\n";
  Line(&out, "Loader", report.vulkan.loader_found ? Narrow(report.vulkan.loader_path) : "not found");
  if (report.vulkan.loader_found) Line(&out, "Instance version", report.vulkan.instance_version_text);
  if (!report.vulkan.error.empty()) Line(&out, "Error", Narrow(report.vulkan.error));
  for (const VulkanDeviceInfo& device : report.vulkan.devices) {
    Line(&out, "Device", device.name + " (" + device.device_type + ")");
    Line(&out, "  API version", device.api_version_text);
    Line(&out, "  Driver version", device.driver_version_text +
                                       (device.driver_info.empty() ? "" : " (" + device.driver_info + ")"));
    if (!device.driver_name.empty()) Line(&out, "  Driver", device.driver_name);
    Line(&out, "  Vendor / device", HexId(device.vendor_id) + " / " + HexId(device.device_id));
  }
  out += "\r\n";

  out += "Elixir\r\n";
  Line(&out, "Mode", report.paths.portable ? "portable (portable.txt beside Elixir.exe)" : "installed");
  Line(&out, "Launcher directory", Narrow(report.paths.exe_dir));
  Line(&out, "Data directory", Narrow(report.paths.data_root));
  Line(&out, "Config file", Narrow(report.paths.config_file));
  Line(&out, "FFlags file", config.advanced.fflags_path.empty() ? Narrow(report.paths.fflags_file)
                                                                 : config.advanced.fflags_path);
  Line(&out, "Apps directory", Narrow(report.paths.apps_dir));
  Line(&out, "Crash directory", Narrow(report.paths.crashes_dir));
  Line(&out, "Runtime", report.runtime_present ? Narrow(report.paths.runtime_exe) + " (present)"
                                               : "elixir-runtime.exe not present (not built yet)");
  Line(&out, "Guest runtime libraries", report.guest_runtime_present
                                            ? std::to_string(report.guest_runtime_libraries) + " in " +
                                                  Narrow(report.paths.guest_runtime_dir)
                                            : "not present");
  if (config.installed.present()) {
    Line(&out, "Installed app", config.installed.package + " " + config.installed.version_name + " (versionCode " +
                                    std::to_string(config.installed.version_code) + ")");
  } else {
    Line(&out, "Installed app", "none");
  }
  return out;
}

Readiness EvaluateReadiness(const DiagnosticsReport* report, const LauncherConfig& config,
                            const LauncherPaths& paths) {
  Readiness readiness;
  const bool runtime_present = PathIsFile(paths.runtime_exe);
  ReadinessCheck runtime{L"Elixir runtime", L"", CheckStatus::kFail};
  if (!runtime_present) {
    runtime.detail = L"Part of Elixir is missing. Reinstall Elixir.";
  } else if (!PathIsFile(JoinPath(paths.guest_runtime_dir, kGuestExtensionLibrary))) {
    runtime.detail = L"libelixir-extension.so is missing; Roblox cannot load without it. Reinstall Elixir.";
  } else {
    runtime.detail = L"The runtime and its Android libraries are in place.";
    runtime.status = CheckStatus::kPass;
  }
  readiness.checks.push_back(std::move(runtime));

  if (report == nullptr) {
    readiness.checks.push_back({L"Processor", L"Measuring\u2026", CheckStatus::kPending});
    readiness.checks.push_back({L"Vulkan", L"Measuring\u2026", CheckStatus::kPending});
    readiness.checks.push_back({L"Windows", L"Measuring\u2026", CheckStatus::kPending});
  } else {
    ReadinessCheck cpu{L"Processor: FSGSBASE", L"", CheckStatus::kFail};
    if (!report->cpu.fsgsbase_cpuid) {
      cpu.detail = L"This CPU does not report FSGSBASE (CPUID.7:EBX bit 0), which the guest thread pointer needs.";
    } else if (report->cpu.rdfsbase_probe == ProbeOutcome::kWorks) {
      cpu.status = CheckStatus::kPass;
      cpu.detail = L"CPUID reports FSGSBASE and a user-mode RDFSBASE probe succeeded.";
    } else {
      wchar_t text[160];
      std::swprintf(text, 160, L"The CPU has FSGSBASE but RDFSBASE faulted (0x%08lX): Windows has not enabled it.",
                    static_cast<unsigned long>(report->cpu.rdfsbase_exception));
      cpu.detail = text;
    }
    readiness.checks.push_back(cpu);

    ReadinessCheck vulkan{L"Vulkan", L"", CheckStatus::kFail};
    const VulkanDeviceInfo* best = nullptr;
    for (const VulkanDeviceInfo& device : report->vulkan.devices) {
      if (device.device_type == "CPU (software)") continue;
      if (best == nullptr || device.api_version > best->api_version) best = &device;
    }
    if (!report->vulkan.loader_found) {
      vulkan.detail = L"No Vulkan loader (vulkan-1.dll). Install your GPU vendor's driver.";
    } else if (best == nullptr) {
      vulkan.detail = report->vulkan.error.empty() ? L"No hardware Vulkan device was found." : report->vulkan.error;
    } else if (best->api_version < VK_API_VERSION_1_1) {
      vulkan.status = CheckStatus::kWarn;
      vulkan.detail = Utf8ToWide(best->name) + L" only supports Vulkan " + Utf8ToWide(best->api_version_text) +
                      L"; 1.1 or newer is required.";
    } else {
      vulkan.status = CheckStatus::kPass;
      vulkan.detail = Utf8ToWide(best->name) + L", Vulkan " + Utf8ToWide(best->api_version_text) + L", driver " +
                      Utf8ToWide(best->driver_info.empty() ? best->driver_version_text : best->driver_info) + L".";
    }
    readiness.checks.push_back(vulkan);

    ReadinessCheck windows{L"Windows", WindowsVersionText(report->windows) + L".", CheckStatus::kPass};
    if (report->windows.major < 10) {
      windows.status = CheckStatus::kFail;
      windows.detail += L" Windows 10 or 11 is required.";
    } else if (report->windows.build < 22000) {
      windows.status = CheckStatus::kWarn;
      windows.detail += L" Elixir is developed and tested on Windows 11; Windows 10 has not been tested yet.";
    }
    readiness.checks.push_back(windows);
  }

  ReadinessCheck app{L"Android app", L"", CheckStatus::kWarn};
  if (!config.installed.present()) {
    app.detail = L"No APK installed yet. Use Install APK to add one.";
  } else if (!PathIsDirectory(Utf8ToWide(config.installed.install_dir))) {
    app.status = CheckStatus::kFail;
    app.detail = Utf8ToWide(config.installed.package) + L" is recorded but its files are missing from " +
                 Utf8ToWide(config.installed.install_dir) + L".";
  } else {
    app.status = CheckStatus::kPass;
    app.detail = Utf8ToWide(config.installed.package) + L" " + Utf8ToWide(config.installed.version_name) +
                 L" (versionCode " + std::to_wstring(config.installed.version_code) + L").";
  }
  readiness.checks.push_back(app);

  bool any_fail = false;
  bool any_pending = false;
  for (const ReadinessCheck& check : readiness.checks) {
    any_fail |= check.status == CheckStatus::kFail;
    any_pending |= check.status == CheckStatus::kPending;
  }
  readiness.ready_to_play = runtime_present && !any_fail && !any_pending && config.installed.present();
  if (!runtime_present) {
    readiness.summary = L"Not playable yet: the Elixir runtime has not been built. Roblox does not run on Elixir yet.";
  } else if (any_pending) {
    readiness.summary = L"Checking this PC\u2026";
  } else if (any_fail) {
    readiness.summary = L"Not ready: some requirements are not met.";
  } else if (!config.installed.present()) {
    readiness.summary = L"Install an APK to play.";
  } else {
    readiness.summary = L"Ready to launch.";
  }
  return readiness;
}

}
