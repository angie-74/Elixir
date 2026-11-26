// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <intrin.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>

#include "bionic/core/core.h"
#include "bionic/files/files.h"
#include "bionic/files/files_internal.h"

#pragma comment(lib, "advapi32.lib")

namespace elixir::bionic::files {
namespace {

constexpr const char* kKernelRelease = "6.6.30-elixir";
constexpr const char* kKernelVersionLine =
    "Linux version 6.6.30-elixir (elixir@windows) (Android clang version 21) #1 SMP PREEMPT Thu Jan  1 00:00:00 UTC "
    "2026\n";

constexpr std::uint8_t kDtDir = guest::kDT_DIR, kDtReg = guest::kDT_REG, kDtLnk = guest::kDT_LNK,
                       kDtChr = guest::kDT_CHR;

std::string Format(const char* format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  const int length = std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (length < 0) return {};
  return std::string(buffer, static_cast<std::size_t>(std::min(length, static_cast<int>(sizeof(buffer)) - 1)));
}

std::vector<std::string> Split(const std::string& path) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < path.size()) {
    while (i < path.size() && path[i] == '/') ++i;
    std::size_t j = i;
    while (j < path.size() && path[j] != '/') ++j;
    if (j > i) out.push_back(path.substr(i, j - i));
    i = j;
  }
  return out;
}

bool ParseUnsigned(const std::string& text, unsigned* value) {
  if (text.empty() || text.size() > 9) return false;
  unsigned result = 0;
  for (char c : text) {
    if (c < '0' || c > '9') return false;
    result = result * 10 + static_cast<unsigned>(c - '0');
  }
  if (text.size() > 1 && text[0] == '0') return false;
  *value = result;
  return true;
}

unsigned Pid() { return GetCurrentProcessId(); }

bool IsOwnPid(const std::string& component) {
  if (component == "self") return true;
  unsigned value = 0;
  return ParseUnsigned(component, &value) && value == Pid();
}

unsigned CpuCount() {
  const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  return count == 0 ? 1u : static_cast<unsigned>(count);
}

std::string CpuRange() {
  const unsigned count = CpuCount();
  return count <= 1 ? std::string("0\n") : Format("0-%u\n", count - 1);
}

std::string ProcessName() {
  std::string name = Options().package_name;
  if (name.size() > 15) name.resize(15);
  return name;
}

DWORD RegistryMhz(unsigned cpu) {
  wchar_t key[96];
  std::swprintf(key, 96, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\%u", cpu);
  DWORD value = 0;
  DWORD size = sizeof(value);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, key, L"~MHz", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
    if (cpu != 0) return RegistryMhz(0);
    return 0;
  }
  return value;
}

struct CpuidFlag {
  std::uint8_t source;
  std::uint8_t bit;
  const char* name;
};

enum : std::uint8_t {
  k1Edx, kExtEdx, kSynth, k1Ecx, kExtEcx, k7Ebx, kDEax, k71Eax, k7Ecx, k7Edx, kSourceCount
};

constexpr CpuidFlag kFlags[] = {
    {k1Edx, 0, "fpu"}, {k1Edx, 1, "vme"}, {k1Edx, 2, "de"}, {k1Edx, 3, "pse"}, {k1Edx, 4, "tsc"},
    {k1Edx, 5, "msr"}, {k1Edx, 6, "pae"}, {k1Edx, 7, "mce"}, {k1Edx, 8, "cx8"}, {k1Edx, 9, "apic"},
    {k1Edx, 11, "sep"}, {k1Edx, 12, "mtrr"}, {k1Edx, 13, "pge"}, {k1Edx, 14, "mca"}, {k1Edx, 15, "cmov"},
    {k1Edx, 16, "pat"}, {k1Edx, 17, "pse36"}, {k1Edx, 18, "pn"}, {k1Edx, 19, "clflush"}, {k1Edx, 21, "dts"},
    {k1Edx, 22, "acpi"}, {k1Edx, 23, "mmx"}, {k1Edx, 24, "fxsr"}, {k1Edx, 25, "sse"}, {k1Edx, 26, "sse2"},
    {k1Edx, 27, "ss"}, {k1Edx, 28, "ht"}, {k1Edx, 29, "tm"}, {k1Edx, 30, "ia64"}, {k1Edx, 31, "pbe"},
    {kExtEdx, 11, "syscall"}, {kExtEdx, 19, "mp"}, {kExtEdx, 20, "nx"}, {kExtEdx, 22, "mmxext"},
    {kExtEdx, 25, "fxsr_opt"}, {kExtEdx, 26, "pdpe1gb"}, {kExtEdx, 27, "rdtscp"}, {kExtEdx, 29, "lm"},
    {kExtEdx, 30, "3dnowext"}, {kExtEdx, 31, "3dnow"},
    {kSynth, 0, "constant_tsc"}, {kSynth, 1, "nonstop_tsc"}, {kSynth, 2, "cpuid"},
    {k1Ecx, 0, "pni"}, {k1Ecx, 1, "pclmulqdq"}, {k1Ecx, 2, "dtes64"}, {k1Ecx, 3, "monitor"},
    {k1Ecx, 4, "ds_cpl"}, {k1Ecx, 5, "vmx"}, {k1Ecx, 6, "smx"}, {k1Ecx, 7, "est"}, {k1Ecx, 8, "tm2"},
    {k1Ecx, 9, "ssse3"}, {k1Ecx, 10, "cid"}, {k1Ecx, 11, "sdbg"}, {k1Ecx, 12, "fma"}, {k1Ecx, 13, "cx16"},
    {k1Ecx, 14, "xtpr"}, {k1Ecx, 15, "pdcm"}, {k1Ecx, 17, "pcid"}, {k1Ecx, 18, "dca"}, {k1Ecx, 19, "sse4_1"},
    {k1Ecx, 20, "sse4_2"}, {k1Ecx, 21, "x2apic"}, {k1Ecx, 22, "movbe"}, {k1Ecx, 23, "popcnt"},
    {k1Ecx, 24, "tsc_deadline_timer"}, {k1Ecx, 25, "aes"}, {k1Ecx, 26, "xsave"}, {k1Ecx, 28, "avx"},
    {k1Ecx, 29, "f16c"}, {k1Ecx, 30, "rdrand"}, {k1Ecx, 31, "hypervisor"},
    {kExtEcx, 0, "lahf_lm"}, {kExtEcx, 1, "cmp_legacy"}, {kExtEcx, 2, "svm"}, {kExtEcx, 3, "extapic"},
    {kExtEcx, 4, "cr8_legacy"}, {kExtEcx, 5, "abm"}, {kExtEcx, 6, "sse4a"}, {kExtEcx, 7, "misalignsse"},
    {kExtEcx, 8, "3dnowprefetch"}, {kExtEcx, 9, "osvw"}, {kExtEcx, 10, "ibs"}, {kExtEcx, 11, "xop"},
    {kExtEcx, 12, "skinit"}, {kExtEcx, 13, "wdt"}, {kExtEcx, 15, "lwp"}, {kExtEcx, 16, "fma4"},
    {kExtEcx, 17, "tce"}, {kExtEcx, 19, "nodeid_msr"}, {kExtEcx, 21, "tbm"}, {kExtEcx, 22, "topoext"},
    {kExtEcx, 23, "perfctr_core"}, {kExtEcx, 24, "perfctr_nb"}, {kExtEcx, 26, "bpext"}, {kExtEcx, 27, "ptsc"},
    {kExtEcx, 28, "perfctr_llc"}, {kExtEcx, 29, "mwaitx"},
    {k7Ebx, 0, "fsgsbase"}, {k7Ebx, 1, "tsc_adjust"}, {k7Ebx, 2, "sgx"}, {k7Ebx, 3, "bmi1"}, {k7Ebx, 4, "hle"},
    {k7Ebx, 5, "avx2"}, {k7Ebx, 7, "smep"}, {k7Ebx, 8, "bmi2"}, {k7Ebx, 9, "erms"}, {k7Ebx, 10, "invpcid"},
    {k7Ebx, 11, "rtm"}, {k7Ebx, 12, "cqm"}, {k7Ebx, 14, "mpx"}, {k7Ebx, 15, "rdt_a"}, {k7Ebx, 16, "avx512f"},
    {k7Ebx, 17, "avx512dq"}, {k7Ebx, 18, "rdseed"}, {k7Ebx, 19, "adx"}, {k7Ebx, 20, "smap"},
    {k7Ebx, 21, "avx512ifma"}, {k7Ebx, 23, "clflushopt"}, {k7Ebx, 24, "clwb"}, {k7Ebx, 25, "intel_pt"},
    {k7Ebx, 26, "avx512pf"}, {k7Ebx, 27, "avx512er"}, {k7Ebx, 28, "avx512cd"}, {k7Ebx, 29, "sha_ni"},
    {k7Ebx, 30, "avx512bw"}, {k7Ebx, 31, "avx512vl"},
    {kDEax, 0, "xsaveopt"}, {kDEax, 1, "xsavec"}, {kDEax, 2, "xgetbv1"}, {kDEax, 3, "xsaves"},
    {k71Eax, 4, "avx_vnni"}, {k71Eax, 5, "avx512_bf16"},
    {k7Ecx, 1, "avx512vbmi"}, {k7Ecx, 2, "umip"}, {k7Ecx, 3, "pku"}, {k7Ecx, 4, "ospke"}, {k7Ecx, 5, "waitpkg"},
    {k7Ecx, 6, "avx512_vbmi2"}, {k7Ecx, 8, "gfni"}, {k7Ecx, 9, "vaes"}, {k7Ecx, 10, "vpclmulqdq"},
    {k7Ecx, 11, "avx512_vnni"}, {k7Ecx, 12, "avx512_bitalg"}, {k7Ecx, 13, "tme"}, {k7Ecx, 14, "avx512_vpopcntdq"},
    {k7Ecx, 16, "la57"}, {k7Ecx, 22, "rdpid"}, {k7Ecx, 25, "cldemote"}, {k7Ecx, 27, "movdiri"},
    {k7Ecx, 28, "movdir64b"},
    {k7Edx, 2, "avx512_4vnniw"}, {k7Edx, 3, "avx512_4fmaps"}, {k7Edx, 4, "fsrm"},
    {k7Edx, 8, "avx512_vp2intersect"}, {k7Edx, 10, "md_clear"}, {k7Edx, 14, "serialize"},
    {k7Edx, 16, "tsxldtrk"}, {k7Edx, 18, "pconfig"}, {k7Edx, 19, "arch_lbr"}, {k7Edx, 22, "amx_bf16"},
    {k7Edx, 23, "avx512_fp16"}, {k7Edx, 24, "amx_tile"}, {k7Edx, 25, "amx_int8"}, {k7Edx, 28, "flush_l1d"},
    {k7Edx, 29, "arch_capabilities"},
};

struct CpuIdentity {
  char vendor[13] = {};
  std::string model_name;
  unsigned family = 0, model = 0, stepping = 0, max_leaf = 0;
  unsigned clflush = 64, physical_bits = 48, virtual_bits = 48;
  std::uint32_t regs[kSourceCount] = {};
};

CpuIdentity QueryCpu() {
  CpuIdentity cpu;
  int r[4];
  __cpuid(r, 0);
  cpu.max_leaf = static_cast<unsigned>(r[0]);
  std::memcpy(cpu.vendor + 0, &r[1], 4);
  std::memcpy(cpu.vendor + 4, &r[3], 4);
  std::memcpy(cpu.vendor + 8, &r[2], 4);
  __cpuid(r, 1);
  const unsigned eax = static_cast<unsigned>(r[0]);
  const unsigned base_family = (eax >> 8) & 0xf;
  cpu.family = base_family == 0xf ? base_family + ((eax >> 20) & 0xff) : base_family;
  cpu.model = (eax >> 4) & 0xf;
  if (base_family == 0x6 || base_family == 0xf) cpu.model |= ((eax >> 16) & 0xf) << 4;
  cpu.stepping = eax & 0xf;
  cpu.clflush = ((static_cast<unsigned>(r[1]) >> 8) & 0xff) * 8;
  cpu.regs[k1Ecx] = static_cast<std::uint32_t>(r[2]);
  cpu.regs[k1Edx] = static_cast<std::uint32_t>(r[3]);
  if (cpu.max_leaf >= 7) {
    __cpuidex(r, 7, 0);
    cpu.regs[k7Ebx] = static_cast<std::uint32_t>(r[1]);
    cpu.regs[k7Ecx] = static_cast<std::uint32_t>(r[2]);
    cpu.regs[k7Edx] = static_cast<std::uint32_t>(r[3]);
    if (r[0] >= 1) {
      __cpuidex(r, 7, 1);
      cpu.regs[k71Eax] = static_cast<std::uint32_t>(r[0]);
    }
  }
  if (cpu.max_leaf >= 0xd) {
    __cpuidex(r, 0xd, 1);
    cpu.regs[kDEax] = static_cast<std::uint32_t>(r[0]);
  }
  cpu.regs[kSynth] = 1u << 2;
  __cpuid(r, static_cast<int>(0x80000000));
  const unsigned max_ext = static_cast<unsigned>(r[0]);
  if (max_ext >= 0x80000001) {
    __cpuid(r, static_cast<int>(0x80000001));
    cpu.regs[kExtEcx] = static_cast<std::uint32_t>(r[2]);
    cpu.regs[kExtEdx] = static_cast<std::uint32_t>(r[3]);
  }
  if (max_ext >= 0x80000004) {
    char brand[49] = {};
    for (int i = 0; i < 3; ++i) {
      __cpuid(r, static_cast<int>(0x80000002 + i));
      std::memcpy(brand + i * 16, r, 16);
    }
    cpu.model_name = brand;
    const std::size_t first = cpu.model_name.find_first_not_of(' ');
    cpu.model_name = first == std::string::npos ? std::string() : cpu.model_name.substr(first);
    while (!cpu.model_name.empty() && cpu.model_name.back() == ' ') cpu.model_name.pop_back();
  }
  if (max_ext >= 0x80000007) {
    __cpuid(r, static_cast<int>(0x80000007));
    if ((r[3] & (1 << 8)) != 0) cpu.regs[kSynth] |= 0x3;
  }
  if (max_ext >= 0x80000008) {
    __cpuid(r, static_cast<int>(0x80000008));
    cpu.physical_bits = static_cast<unsigned>(r[0]) & 0xff;
    cpu.virtual_bits = (static_cast<unsigned>(r[0]) >> 8) & 0xff;
  }
  if (cpu.model_name.empty()) cpu.model_name = "Unknown x86_64 CPU";
  return cpu;
}

struct CpuTopology {
  std::vector<unsigned> core_of;
  std::vector<unsigned> package_of;
  unsigned cores = 1;
  std::uint64_t cache_kb = 0;
};

template <typename Visit>
void ForEachProcessorRelation(LOGICAL_PROCESSOR_RELATIONSHIP relation, Visit visit) {
  DWORD length = 0;
  GetLogicalProcessorInformationEx(relation, nullptr, &length);
  if (length == 0) return;
  std::vector<std::uint8_t> buffer(length);
  auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
  if (!GetLogicalProcessorInformationEx(relation, info, &length)) return;
  for (DWORD offset = 0; offset < length;) {
    auto* entry = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
    visit(*entry);
    if (entry->Size == 0) break;
    offset += entry->Size;
  }
}

CpuTopology QueryTopology(unsigned count) {
  CpuTopology topology;
  topology.core_of.assign(count, 0);
  topology.package_of.assign(count, 0);
  unsigned core_index = 0;
  ForEachProcessorRelation(RelationProcessorCore, [&](const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX& entry) {
    for (WORD g = 0; g < entry.Processor.GroupCount; ++g) {
      const GROUP_AFFINITY& affinity = entry.Processor.GroupMask[g];
      for (unsigned bit = 0; bit < 64; ++bit) {
        if ((affinity.Mask >> bit) & 1) {
          const unsigned cpu = affinity.Group * 64u + bit;
          if (cpu < count) topology.core_of[cpu] = core_index;
        }
      }
    }
    ++core_index;
  });
  if (core_index > 0) topology.cores = core_index;
  unsigned package_index = 0;
  ForEachProcessorRelation(RelationProcessorPackage, [&](const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX& entry) {
    for (WORD g = 0; g < entry.Processor.GroupCount; ++g) {
      const GROUP_AFFINITY& affinity = entry.Processor.GroupMask[g];
      for (unsigned bit = 0; bit < 64; ++bit) {
        if ((affinity.Mask >> bit) & 1) {
          const unsigned cpu = affinity.Group * 64u + bit;
          if (cpu < count) topology.package_of[cpu] = package_index;
        }
      }
    }
    ++package_index;
  });
  unsigned best_level = 0;
  ForEachProcessorRelation(RelationCache, [&](const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX& entry) {
    if (entry.Cache.Type == CacheInstruction) return;
    if (entry.Cache.Level > best_level ||
        (entry.Cache.Level == best_level && entry.Cache.CacheSize / 1024 > topology.cache_kb)) {
      best_level = entry.Cache.Level;
      topology.cache_kb = entry.Cache.CacheSize / 1024;
    }
  });
  return topology;
}

std::string CpuInfo() {
  const CpuIdentity cpu = QueryCpu();
  const unsigned count = CpuCount();
  const CpuTopology topology = QueryTopology(count);
  std::string flags;
  for (const CpuidFlag& flag : kFlags) {
    if ((cpu.regs[flag.source] >> flag.bit) & 1) {
      if (!flags.empty()) flags.push_back(' ');
      flags += flag.name;
    }
  }
  std::string out;
  for (unsigned i = 0; i < count; ++i) {
    const DWORD mhz = RegistryMhz(i);
    out += Format("processor\t: %u\n", i);
    out += Format("vendor_id\t: %s\n", cpu.vendor);
    out += Format("cpu family\t: %u\n", cpu.family);
    out += Format("model\t\t: %u\n", cpu.model);
    out += "model name\t: " + cpu.model_name + "\n";
    out += Format("stepping\t: %u\n", cpu.stepping);
    out += Format("cpu MHz\t\t: %lu.000\n", static_cast<unsigned long>(mhz));
    out += Format("cache size\t: %llu KB\n", static_cast<unsigned long long>(topology.cache_kb));
    out += Format("physical id\t: %u\n", topology.package_of[i]);
    out += Format("siblings\t: %u\n", count);
    out += Format("core id\t\t: %u\n", topology.core_of[i]);
    out += Format("cpu cores\t: %u\n", topology.cores);
    out += Format("apicid\t\t: %u\n", i);
    out += Format("initial apicid\t: %u\n", i);
    out += "fpu\t\t: yes\nfpu_exception\t: yes\n";
    out += Format("cpuid level\t: %u\n", cpu.max_leaf);
    out += "wp\t\t: yes\n";
    out += "flags\t\t: " + flags + "\n";
    out += "bugs\t\t:\n";
    out += Format("bogomips\t: %lu.00\n", static_cast<unsigned long>(mhz * 2));
    out += Format("clflush size\t: %u\n", cpu.clflush);
    out += Format("cache_alignment\t: %u\n", cpu.clflush);
    out += Format("address sizes\t: %u bits physical, %u bits virtual\n", cpu.physical_bits, cpu.virtual_bits);
    out += "power management:\n\n";
  }
  return out;
}

std::string MemInfo() {
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  const unsigned long long total = status.ullTotalPhys / 1024;
  const unsigned long long available = status.ullAvailPhys / 1024;
  const unsigned long long commit_total = status.ullTotalPageFile / 1024;
  const unsigned long long commit_free = status.ullAvailPageFile / 1024;
  const unsigned long long swap_total = commit_total > total ? commit_total - total : 0;
  const unsigned long long swap_free =
      commit_free > available ? std::min(commit_free - available, swap_total) : 0;
  auto line = [](const char* label, unsigned long long kb) {
    std::string name = std::string(label) + ":";
    while (name.size() < 16) name.push_back(' ');
    return name + Format("%8llu kB\n", kb);
  };
  std::string out;
  out += line("MemTotal", total);
  out += line("MemFree", available);
  out += line("MemAvailable", available);
  out += line("Buffers", 0);
  out += line("Cached", 0);
  out += line("SwapCached", 0);
  out += line("Active", total - available);
  out += line("Inactive", 0);
  out += line("SwapTotal", swap_total);
  out += line("SwapFree", swap_free);
  out += line("Dirty", 0);
  out += line("Shmem", 0);
  out += line("CommitLimit", commit_total);
  out += line("Committed_AS", commit_total - commit_free);
  out += line("VmallocTotal", 34359738367ull);
  return out;
}

struct ProcessorTimes {
  LARGE_INTEGER idle, kernel, user, dpc, interrupt;
  ULONG interrupt_count;
};

using NtQuerySystemInformationFn = LONG(WINAPI*)(ULONG, PVOID, ULONG, PULONG);

std::vector<ProcessorTimes> QueryProcessorTimes() {
  static const auto query = reinterpret_cast<NtQuerySystemInformationFn>(
      reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation")));
  std::vector<ProcessorTimes> times(std::max(1u, std::min(CpuCount(), 64u)));
  if (query == nullptr) return {};
  ULONG returned = 0;
  if (query(8 , times.data(),
            static_cast<ULONG>(times.size() * sizeof(ProcessorTimes)), &returned) < 0) {
    return {};
  }
  times.resize(returned / sizeof(ProcessorTimes));
  return times;
}

std::uint64_t BootTimeUnix() {
  const std::int64_t now_ft = NowFileTime();
  guest::Timespec now;
  FileTimeToTimespec(now_ft, &now);
  return static_cast<std::uint64_t>(now.tv_sec) - GetTickCount64() / 1000;
}

std::string Stat() {
  const std::vector<ProcessorTimes> times = QueryProcessorTimes();
  auto ticks = [](std::int64_t hundred_ns) { return static_cast<unsigned long long>(hundred_ns / 100000); };
  std::uint64_t sum[5] = {};
  std::string per_cpu;
  for (std::size_t i = 0; i < times.size(); ++i) {
    const ProcessorTimes& t = times[i];
    std::int64_t system = t.kernel.QuadPart - t.idle.QuadPart - t.dpc.QuadPart - t.interrupt.QuadPart;
    if (system < 0) system = 0;
    const unsigned long long values[5] = {ticks(t.user.QuadPart), ticks(system), ticks(t.idle.QuadPart),
                                          ticks(t.interrupt.QuadPart), ticks(t.dpc.QuadPart)};
    for (int k = 0; k < 5; ++k) sum[k] += values[k];
    per_cpu += Format("cpu%zu %llu 0 %llu %llu 0 %llu %llu 0 0 0\n", i, values[0], values[1], values[2], values[3],
                      values[4]);
  }
  std::string out = Format("cpu  %llu 0 %llu %llu 0 %llu %llu 0 0 0\n", static_cast<unsigned long long>(sum[0]),
                           static_cast<unsigned long long>(sum[1]), static_cast<unsigned long long>(sum[2]),
                           static_cast<unsigned long long>(sum[3]), static_cast<unsigned long long>(sum[4]));
  out += per_cpu;
  out += "intr 0\nctxt 0\n";
  out += Format("btime %llu\n", static_cast<unsigned long long>(BootTimeUnix()));
  out += "processes 1\nprocs_running 1\nprocs_blocked 0\nsoftirq 0 0 0 0 0 0 0 0 0 0 0\n";
  return out;
}

std::string Uptime() {
  const ULONGLONG ms = GetTickCount64();
  std::vector<ProcessorTimes> times = QueryProcessorTimes();
  double idle = 0;
  for (const auto& t : times) idle += static_cast<double>(t.idle.QuadPart) / 1e7;
  return Format("%llu.%02llu %.2f\n", static_cast<unsigned long long>(ms / 1000),
                static_cast<unsigned long long>((ms % 1000) / 10), idle);
}

unsigned ToolhelpThreadCount() {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return 1;
  THREADENTRY32 entry{};
  entry.dwSize = sizeof(entry);
  unsigned count = 0;
  const DWORD pid = GetCurrentProcessId();
  if (Thread32First(snapshot, &entry)) {
    do {
      if (entry.th32OwnerProcessID == pid) ++count;
    } while (Thread32Next(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return count == 0 ? 1 : count;
}

using NtGetNextThreadFn = LONG(NTAPI*)(HANDLE process, HANDLE thread, ACCESS_MASK access, ULONG attributes,
                                       ULONG flags, PHANDLE next);

unsigned WalkThreadCount() {
  static const auto next_thread = reinterpret_cast<NtGetNextThreadFn>(
      reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtGetNextThread")));
  if (next_thread == nullptr) return ToolhelpThreadCount();
  unsigned count = 0;
  HANDLE current = nullptr;
  for (;;) {
    HANDLE following = nullptr;
    const LONG status =
        next_thread(GetCurrentProcess(), current, THREAD_QUERY_LIMITED_INFORMATION, 0, 0, &following);
    if (current != nullptr) CloseHandle(current);
    current = nullptr;
    if (status < 0 || following == nullptr) break;
    current = following;
    if (++count >= 1000000) {
      CloseHandle(current);
      break;
    }
  }
  return count == 0 ? ToolhelpThreadCount() : count;
}

unsigned ThreadCount() {
  static std::mutex lock;
  static ULONGLONG refreshed_ms = 0;
  static unsigned cached = 1;
  std::lock_guard<std::mutex> guard(lock);
  const ULONGLONG now = GetTickCount64();
  if (refreshed_ms == 0 || now - refreshed_ms >= 500) {
    cached = WalkThreadCount();
    refreshed_ms = now;
  }
  return cached;
}

PROCESS_MEMORY_COUNTERS_EX MemoryCounters() {
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));
  return counters;
}

std::string Status() {
  const PROCESS_MEMORY_COUNTERS_EX m = MemoryCounters();
  const unsigned pid = Pid();
  const unsigned threads = ThreadCount();
  const unsigned count = CpuCount();
  std::string mask;
  for (unsigned i = 0; i < (count + 3) / 4; ++i) {
    const unsigned bits = std::min(4u, count - i * 4);
    mask.insert(mask.begin(), "0123456789abcdef"[(1u << bits) - 1]);
  }
  std::string out;
  out += "Name:\t" + ProcessName() + "\n";
  out += Format("Umask:\t%04o\n", CurrentUmask());
  out += "State:\tR (running)\n";
  out += Format("Tgid:\t%u\nNgid:\t0\nPid:\t%u\nPPid:\t1\nTracerPid:\t0\n", pid, pid);
  out += Format("Uid:\t%u\t%u\t%u\t%u\n", kGuestUid, kGuestUid, kGuestUid, kGuestUid);
  out += Format("Gid:\t%u\t%u\t%u\t%u\n", kGuestGid, kGuestGid, kGuestGid, kGuestGid);
  out += "FDSize:\t256\nGroups:\t\n";
  out += Format("VmPeak:\t%8llu kB\n", static_cast<unsigned long long>(m.PeakPagefileUsage / 1024));
  out += Format("VmSize:\t%8llu kB\n", static_cast<unsigned long long>(m.PagefileUsage / 1024));
  out += "VmLck:\t       0 kB\nVmPin:\t       0 kB\n";
  out += Format("VmHWM:\t%8llu kB\n", static_cast<unsigned long long>(m.PeakWorkingSetSize / 1024));
  out += Format("VmRSS:\t%8llu kB\n", static_cast<unsigned long long>(m.WorkingSetSize / 1024));
  out += Format("VmData:\t%8llu kB\n", static_cast<unsigned long long>(m.PrivateUsage / 1024));
  out += "VmStk:\t     132 kB\nVmExe:\t       0 kB\nVmLib:\t       0 kB\nVmSwap:\t       0 kB\n";
  out += Format("Threads:\t%u\n", threads);
  out += "SigQ:\t0/63432\nSigPnd:\t0000000000000000\nShdPnd:\t0000000000000000\n";
  out += "SigBlk:\t0000000000000000\nSigIgn:\t0000000000001000\nSigCgt:\t0000000000000000\n";
  out += "CapInh:\t0000000000000000\nCapPrm:\t0000000000000000\nCapEff:\t0000000000000000\n";
  out += "CapBnd:\t0000000000000000\nCapAmb:\t0000000000000000\nNoNewPrivs:\t0\nSeccomp:\t2\n";
  out += "Cpus_allowed:\t" + mask + "\n";
  out += "Cpus_allowed_list:\t" + CpuRange();
  out += "voluntary_ctxt_switches:\t0\nnonvoluntary_ctxt_switches:\t0\n";
  return out;
}

unsigned long long FileTimeTicks(const FILETIME& time) {
  const unsigned long long value = (static_cast<unsigned long long>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
  return value / 100000;
}

std::string ProcStat() {
  const PROCESS_MEMORY_COUNTERS_EX m = MemoryCounters();
  FILETIME creation, exit_time, kernel, user;
  GetProcessTimes(GetCurrentProcess(), &creation, &exit_time, &kernel, &user);
  const std::int64_t creation_ft =
      (static_cast<std::int64_t>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
  const std::int64_t boot_ft = NowFileTime() - static_cast<std::int64_t>(GetTickCount64()) * 10000;
  const unsigned long long start = creation_ft > boot_ft ? static_cast<unsigned long long>(creation_ft - boot_ft) / 100000 : 0;
  const unsigned pid = Pid();
  return Format(
      "%u (%s) R 1 %u %u 0 -1 4194624 %lu 0 0 0 %llu %llu 0 0 20 0 %u 0 %llu %llu %llu 18446744073709551615 "
      "0 0 0 0 0 0 0 4096 1073775864 0 0 0 17 %lu 0 0 0 0 0 0 0 0 0 0 0 0 0\n",
      pid, ProcessName().c_str(), pid, pid, static_cast<unsigned long>(m.PageFaultCount), FileTimeTicks(user),
      FileTimeTicks(kernel), ThreadCount(), start, static_cast<unsigned long long>(m.PagefileUsage),
      static_cast<unsigned long long>(m.WorkingSetSize / 4096), static_cast<unsigned long>(GetCurrentProcessorNumber()));
}

std::string ProcStatm() {
  const PROCESS_MEMORY_COUNTERS_EX m = MemoryCounters();
  return Format("%llu %llu 0 0 0 %llu 0\n", static_cast<unsigned long long>(m.PagefileUsage / 4096),
                static_cast<unsigned long long>(m.WorkingSetSize / 4096),
                static_cast<unsigned long long>(m.PrivateUsage / 4096));
}

std::string OomScore() {
  const PROCESS_MEMORY_COUNTERS_EX m = MemoryCounters();
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  const unsigned long long total_pages = std::max<unsigned long long>(
      std::max<unsigned long long>(status.ullTotalPageFile, status.ullTotalPhys) / 4096, 1);
  const unsigned long long resident_pages = std::min<unsigned long long>(m.WorkingSetSize / 4096, total_pages);
  return Format("%llu\n", (1000 + resident_pages * 1000 / total_pages) * 2 / 3);
}

std::string OomScoreAdj() { return "0\n"; }

std::string Mounts() {
  std::string out = "rootfs / rootfs ro,relatime 0 0\n";
  out += "proc /proc proc rw,relatime 0 0\n";
  out += "sysfs /sys sysfs rw,relatime 0 0\n";
  out += "tmpfs /dev tmpfs rw,nosuid,relatime,mode=755 0 0\n";
  unsigned index = 0;
  for (const auto& [guest, read_only] : MountList()) {
    out += Format("/dev/block/elixir%u ", index++) + guest + (read_only ? " ext4 ro" : " ext4 rw") +
           ",nosuid,nodev,noatime 0 0\n";
  }
  return out;
}

std::string UuidText(const unsigned char bytes[16]) {
  return Format("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n", bytes[0], bytes[1], bytes[2],
                bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
                bytes[13], bytes[14], bytes[15]);
}

std::string RandomUuid() {
  unsigned char bytes[16] = {};
  FillRandom(bytes, sizeof(bytes));
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
  return UuidText(bytes);
}

std::string BootId() {
  static const std::string id = RandomUuid();
  return id;
}

struct SegmentRegistry {
  std::mutex lock;
  std::vector<MappedSegment> segments;
};
SegmentRegistry& Segments() {
  static SegmentRegistry* registry = new SegmentRegistry();
  return *registry;
}

struct MapLine {
  std::uintptr_t start, end;
  char perms[5];
  std::uint64_t offset;
  std::string path;
};

void Perms(DWORD protect, DWORD state, DWORD type, char out[5]) {
  bool r = false, w = false, x = false;
  if (state == MEM_COMMIT && (protect & PAGE_GUARD) == 0) {
    switch (protect & 0xff) {
      case PAGE_READONLY: r = true; break;
      case PAGE_READWRITE: case PAGE_WRITECOPY: r = w = true; break;
      case PAGE_EXECUTE: x = true; break;
      case PAGE_EXECUTE_READ: r = x = true; break;
      case PAGE_EXECUTE_READWRITE: case PAGE_EXECUTE_WRITECOPY: r = w = x = true; break;
      default: break;
    }
  }
  out[0] = r ? 'r' : '-';
  out[1] = w ? 'w' : '-';
  out[2] = x ? 'x' : '-';
  out[3] = type == MEM_MAPPED && (protect & (PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) == 0 ? 's' : 'p';
  out[4] = '\0';
}

std::string DevicePathToGuest(const std::wstring& device_path, const std::vector<std::pair<std::wstring, std::wstring>>& devices) {
  for (const auto& [device, drive] : devices) {
    if (device_path.size() > device.size() && device_path.compare(0, device.size(), device) == 0 &&
        device_path[device.size()] == L'\\') {
      return vfs::HostToGuest(WideToUtf8(drive + device_path.substr(device.size())));
    }
  }
  return WideToUtf8(device_path);
}

std::string Maps() {
  std::vector<std::pair<std::wstring, std::wstring>> devices;
  wchar_t drives[512];
  const DWORD drives_length = GetLogicalDriveStringsW(511, drives);
  for (const wchar_t* drive = drives; drives_length > 0 && drives_length < 512 && *drive != L'\0';
       drive += wcslen(drive) + 1) {
    const std::wstring letter(drive, 2);
    wchar_t target[MAX_PATH];
    if (QueryDosDeviceW(letter.c_str(), target, MAX_PATH) != 0) devices.emplace_back(target, letter);
  }
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  std::vector<MapLine> lines;
  std::map<void*, std::string> names;
  auto address = reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
  const auto limit = reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);
  while (address < limit) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) break;
    const auto base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
    const std::uintptr_t end = base + mbi.RegionSize;
    if (end <= address) break;
    if (mbi.State != MEM_FREE) {
      MapLine line{};
      line.start = base;
      line.end = end;
      Perms(mbi.Protect, mbi.State, mbi.Type, line.perms);
      if (mbi.Type == MEM_IMAGE || mbi.Type == MEM_MAPPED) {
        auto it = names.find(mbi.AllocationBase);
        if (it == names.end()) {
          wchar_t mapped[1024];
          const DWORD length = GetMappedFileNameW(GetCurrentProcess(), mbi.BaseAddress, mapped, 1024);
          it = names.emplace(mbi.AllocationBase, length > 0 ? DevicePathToGuest(std::wstring(mapped, length), devices)
                                                            : std::string())
                   .first;
        }
        line.path = it->second;
        line.offset = base - reinterpret_cast<std::uintptr_t>(mbi.AllocationBase);
      }
      lines.push_back(std::move(line));
    }
    address = end;
  }
  std::vector<MappedSegment> segments;
  {
    SegmentRegistry& registry = Segments();
    std::lock_guard<std::mutex> guard(registry.lock);
    segments = registry.segments;
  }
  for (const MappedSegment& segment : segments) {
    std::vector<MapLine> next;
    char perms[5] = {'-', '-', '-', 'p', '\0'};
    for (MapLine& line : lines) {
      if (line.end <= segment.start || line.start >= segment.end) {
        next.push_back(std::move(line));
        continue;
      }
      if (line.start <= segment.start) std::memcpy(perms, line.perms, 5);
      if (line.start < segment.start) {
        MapLine head = line;
        head.end = segment.start;
        next.push_back(std::move(head));
      }
      if (line.end > segment.end) {
        MapLine tail = line;
        tail.offset += segment.end - line.start;
        tail.start = segment.end;
        next.push_back(std::move(tail));
      }
    }
    MapLine mapped{};
    mapped.start = segment.start;
    mapped.end = segment.end;
    std::memcpy(mapped.perms, perms, 5);
    mapped.perms[3] = 'p';
    mapped.offset = segment.file_offset;
    mapped.path = segment.guest_path;
    next.push_back(std::move(mapped));
    lines = std::move(next);
  }
  std::sort(lines.begin(), lines.end(), [](const MapLine& a, const MapLine& b) { return a.start < b.start; });
  std::string out;
  for (const MapLine& line : lines) {
    std::string text = Format("%08llx-%08llx %s %08llx 00:00 0", static_cast<unsigned long long>(line.start),
                              static_cast<unsigned long long>(line.end), line.perms,
                              static_cast<unsigned long long>(line.offset));
    if (!line.path.empty()) {
      while (text.size() < 72) text.push_back(' ');
      text.push_back(' ');
      text += line.path;
    }
    text.push_back('\n');
    out += text;
  }
  return out;
}

std::uint64_t DevFor(const std::vector<std::string>& c) {
  if (c.empty()) return MakeDev(0, 1);
  if (c[0] == "proc") return MakeDev(0, 22);
  if (c[0] == "sys") return MakeDev(0, 21);
  return MakeDev(0, 6);
}

bool CpuIndex(const std::string& component, unsigned* cpu) {
  if (component.size() < 4 || component.compare(0, 3, "cpu") != 0) return false;
  return ParseUnsigned(component.substr(3), cpu) && *cpu < CpuCount();
}

SynthType Classify(const std::vector<std::string>& c, std::string* content, bool want_content,
                   std::vector<std::pair<std::string, std::uint8_t>>* children, SynthNode* node) {
  const std::size_t n = c.size();
  auto file = [&](auto produce, std::uint32_t mode = 0444) {
    node->mode = mode;
    if (want_content) *content = produce();
    return SynthType::kFile;
  };
  auto memory_stats = [&](ProcSample sample) {
    node->sample = sample;
    return file(sample);
  };
  auto dir = [&](std::initializer_list<std::pair<const char*, std::uint8_t>> list) {
    if (children != nullptr) {
      for (const auto& [name, type] : list) children->emplace_back(name, type);
    }
    return SynthType::kDir;
  };
  if (n == 0) return SynthType::kNone;
  if (c[0] == "dev") {
    if (n == 1) {
      return dir({{"null", kDtChr}, {"zero", kDtChr}, {"random", kDtChr}, {"urandom", kDtChr},
                  {"stdin", kDtLnk}, {"stdout", kDtLnk}, {"stderr", kDtLnk}, {"fd", kDtLnk}});
    }
    if (n != 2) return SynthType::kNone;
    if (c[1] == "null") { node->rdev = MakeDev(1, 3); return SynthType::kNull; }
    if (c[1] == "zero") { node->rdev = MakeDev(1, 5); return SynthType::kZero; }
    if (c[1] == "random") { node->rdev = MakeDev(1, 8); return SynthType::kRandom; }
    if (c[1] == "urandom") { node->rdev = MakeDev(1, 9); return SynthType::kRandom; }
    if (c[1] == "stdin") { node->link_target = "/proc/self/fd/0"; return SynthType::kLink; }
    if (c[1] == "stdout") { node->link_target = "/proc/self/fd/1"; return SynthType::kLink; }
    if (c[1] == "stderr") { node->link_target = "/proc/self/fd/2"; return SynthType::kLink; }
    if (c[1] == "fd") { node->link_target = "/proc/self/fd"; return SynthType::kLink; }
    return SynthType::kNone;
  }
  if (c[0] == "proc") {
    if (n == 1) {
      if (children != nullptr) children->emplace_back(std::to_string(Pid()), kDtDir);
      return dir({{"self", kDtLnk}, {"cpuinfo", kDtReg}, {"meminfo", kDtReg}, {"stat", kDtReg},
                  {"version", kDtReg}, {"uptime", kDtReg}, {"loadavg", kDtReg}, {"mounts", kDtLnk},
                  {"sys", kDtDir}});
    }
    if (n == 2) {
      if (c[1] == "self") { node->link_target = std::to_string(Pid()); return SynthType::kLink; }
      if (c[1] == "mounts") { node->link_target = "self/mounts"; return SynthType::kLink; }
      if (c[1] == "cpuinfo") return file(CpuInfo);
      if (c[1] == "meminfo") return memory_stats(MemInfo);
      if (c[1] == "stat") return file(Stat);
      if (c[1] == "version") return file([] { return std::string(kKernelVersionLine); });
      if (c[1] == "uptime") return file(Uptime);
      if (c[1] == "loadavg") return file([] { return Format("0.00 0.00 0.00 1/%u %u\n", ThreadCount(), Pid()); });
      if (c[1] == "sys") return dir({{"kernel", kDtDir}});
    }
    if (c[1] == "sys") {
      if (n == 3 && c[2] == "kernel") {
        return dir({{"random", kDtDir}, {"osrelease", kDtReg}, {"ostype", kDtReg}, {"pid_max", kDtReg}});
      }
      if (n == 4 && c[2] == "kernel") {
        if (c[3] == "random") return dir({{"boot_id", kDtReg}, {"uuid", kDtReg}});
        if (c[3] == "osrelease") return file([] { return std::string(kKernelRelease) + "\n"; });
        if (c[3] == "ostype") return file([] { return std::string("Linux\n"); });
        if (c[3] == "pid_max") return file([] { return std::string("4194304\n"); });
      }
      if (n == 5 && c[2] == "kernel" && c[3] == "random") {
        if (c[4] == "boot_id") return file(BootId);
        if (c[4] == "uuid") return file(RandomUuid);
      }
      return SynthType::kNone;
    }
    if (!IsOwnPid(c[1])) return SynthType::kNone;
    if (n == 2) {
      return dir({{"auxv", kDtReg}, {"maps", kDtReg}, {"status", kDtReg}, {"cmdline", kDtReg}, {"stat", kDtReg},
                  {"statm", kDtReg}, {"comm", kDtReg}, {"mounts", kDtReg}, {"oom_score", kDtReg},
                  {"oom_score_adj", kDtReg}, {"oom_adj", kDtReg}, {"exe", kDtLnk}, {"cwd", kDtLnk},
                  {"fd", kDtDir}});
    }
    if (n == 3) {
      if (c[2] == "auxv") return file(core::GuestAuxiliaryVector, 0400);
      if (c[2] == "maps") return file(Maps);
      if (c[2] == "status") return memory_stats(Status);
      if (c[2] == "cmdline") return file([] { return Options().package_name + std::string(1, '\0'); });
      if (c[2] == "stat") return memory_stats(ProcStat);
      if (c[2] == "statm") return memory_stats(ProcStatm);
      if (c[2] == "comm") return file([] { return ProcessName() + "\n"; });
      if (c[2] == "mounts") return file(Mounts);
      if (c[2] == "oom_score") return memory_stats(OomScore);
      if (c[2] == "oom_score_adj" || c[2] == "oom_adj") return file(OomScoreAdj);
      if (c[2] == "exe") { node->link_target = "/system/bin/app_process64"; return SynthType::kLink; }
      if (c[2] == "cwd") { node->link_target = GuestCwd(); return SynthType::kLink; }
      if (c[2] == "fd") {
        if (children != nullptr) {
          for (int fd : OpenFds()) children->emplace_back(std::to_string(fd), kDtLnk);
        }
        return SynthType::kDir;
      }
      return SynthType::kNone;
    }
    if (n == 4 && c[2] == "fd") {
      unsigned fd = 0;
      if (!ParseUnsigned(c[3], &fd) || fd >= static_cast<unsigned>(kMaxFds) ||
          FindDescription(static_cast<int>(fd)) == nullptr) {
        return SynthType::kNone;
      }
      node->alias_fd = static_cast<int>(fd);
      return SynthType::kFdAlias;
    }
    return SynthType::kNone;
  }
  if (c[0] == "sys") {
    static const char* const kPrefix[] = {"sys", "devices", "system", "cpu"};
    for (std::size_t i = 1; i < n && i < 4; ++i) {
      if (c[i] != kPrefix[i]) return SynthType::kNone;
    }
    if (n == 1) return dir({{"devices", kDtDir}});
    if (n == 2) return dir({{"system", kDtDir}});
    if (n == 3) return dir({{"cpu", kDtDir}});
    if (n == 4) {
      if (children != nullptr) {
        for (unsigned i = 0; i < CpuCount(); ++i) children->emplace_back("cpu" + std::to_string(i), kDtDir);
      }
      return dir({{"possible", kDtReg}, {"present", kDtReg}, {"online", kDtReg}, {"offline", kDtReg},
                  {"kernel_max", kDtReg}});
    }
    if (n == 5) {
      if (c[4] == "possible" || c[4] == "present" || c[4] == "online") return file(CpuRange);
      if (c[4] == "offline") return file([] { return std::string("\n"); });
      if (c[4] == "kernel_max") return file([] { return std::string("255\n"); });
    }
    unsigned cpu = 0;
    if (!CpuIndex(c[4], &cpu)) return SynthType::kNone;
    if (n == 5) return dir({{"online", kDtReg}, {"cpufreq", kDtDir}, {"topology", kDtDir}});
    if (n == 6) {
      if (c[5] == "online") return file([] { return std::string("1\n"); });
      if (c[5] == "cpufreq") {
        return dir({{"cpuinfo_max_freq", kDtReg}, {"cpuinfo_min_freq", kDtReg}, {"scaling_cur_freq", kDtReg},
                    {"scaling_max_freq", kDtReg}, {"scaling_min_freq", kDtReg}});
      }
      if (c[5] == "topology") {
        return dir({{"core_id", kDtReg}, {"physical_package_id", kDtReg}});
      }
      return SynthType::kNone;
    }
    if (n == 7 && c[5] == "cpufreq") {
      const DWORD mhz = RegistryMhz(cpu);
      if (c[6] == "cpuinfo_max_freq" || c[6] == "scaling_max_freq" || c[6] == "scaling_cur_freq") {
        return file([mhz] { return Format("%lu\n", static_cast<unsigned long>(mhz) * 1000ul); });
      }
      if (c[6] == "cpuinfo_min_freq" || c[6] == "scaling_min_freq") {
        return file([mhz] { return Format("%lu\n", static_cast<unsigned long>(mhz / 4) * 1000ul); });
      }
      return SynthType::kNone;
    }
    if (n == 7 && c[5] == "topology") {
      if (c[6] == "core_id") {
        return file([cpu] { return Format("%u\n", QueryTopology(CpuCount()).core_of[cpu]); });
      }
      if (c[6] == "physical_package_id") {
        return file([cpu] { return Format("%u\n", QueryTopology(CpuCount()).package_of[cpu]); });
      }
    }
    return SynthType::kNone;
  }
  return SynthType::kNone;
}

}

bool SynthLookup(const std::string& guest, SynthNode* node) {
  const std::vector<std::string> components = Split(guest);
  SynthNode result;
  const SynthType type = Classify(components, nullptr, false, nullptr, &result);
  if (type == SynthType::kNone) return false;
  result.type = type;
  result.ino = HashString(guest) | 1;
  result.dev = DevFor(components);
  switch (type) {
    case SynthType::kDir: result.mode = 0555; break;
    case SynthType::kNull: case SynthType::kZero: case SynthType::kRandom: result.mode = 0666; break;
    case SynthType::kLink: case SynthType::kFdAlias: result.mode = 0777; break;
    default: break;
  }
  if (type == SynthType::kDir && !components.empty() && components[0] == "dev" && components.size() == 1) {
    result.mode = 0755;
  }
  if (type == SynthType::kFile && !components.empty() && components[0] == "sys") result.size = 4096;
  if (type == SynthType::kLink) result.size = static_cast<std::int64_t>(result.link_target.size());
  *node = std::move(result);
  return true;
}

std::string SynthContent(const std::string& guest) {
  std::string content;
  SynthNode node;
  Classify(Split(guest), &content, true, nullptr, &node);
  return content;
}

std::vector<std::pair<std::string, std::uint8_t>> SynthList(const std::string& guest) {
  std::vector<std::pair<std::string, std::uint8_t>> children;
  SynthNode node;
  Classify(Split(guest), nullptr, false, &children, &node);
  return children;
}

bool FollowSynthetic(Resolved* r, SynthNode* node, int* err, std::string* original_guest) {
  for (int depth = 0; r->kind == NodeKind::kSynthetic; ++depth) {
    if (!SynthLookup(r->guest, node)) {
      *err = guest::kENOENT;
      return false;
    }
    if (node->type != SynthType::kLink) return true;
    if (depth >= 8) {
      *err = guest::kELOOP;
      return false;
    }
    std::string parent = r->guest.substr(0, r->guest.rfind('/'));
    if (parent.empty()) parent = "/";
    const bool trailing = r->trailing_slash;
    Resolved next;
    if (!ResolveFrom(parent, node->link_target, &next, err)) return false;
    next.trailing_slash = next.trailing_slash || trailing;
    if (original_guest != nullptr && depth == 0) original_guest->swap(r->guest);
    *r = std::move(next);
  }
  return true;
}

std::uint64_t SynthFsType(const std::string& guest) {
  const std::vector<std::string> c = Split(guest);
  if (!c.empty() && c[0] == "proc") return 0x9fa0;
  if (!c.empty() && c[0] == "sys") return 0x62656572;
  if (!c.empty() && c[0] == "dev") return 0x01021994;
  return 0x858458f6;
}

void RegisterMappedSegment(const MappedSegment& segment) {
  if (segment.end <= segment.start) return;
  SegmentRegistry& registry = Segments();
  std::lock_guard<std::mutex> guard(registry.lock);
  registry.segments.push_back(segment);
}

void UnregisterMappedRange(std::uintptr_t start, std::uintptr_t end) {
  SegmentRegistry& registry = Segments();
  std::lock_guard<std::mutex> guard(registry.lock);
  registry.segments.erase(std::remove_if(registry.segments.begin(), registry.segments.end(),
                                         [&](const MappedSegment& s) { return s.start < end && s.end > start; }),
                          registry.segments.end());
}

}
