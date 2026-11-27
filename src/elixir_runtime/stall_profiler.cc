// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/stall_profiler.h"

#include <dbghelp.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <map>
#include <mutex>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "bionic/wake_trace.h"
#include "elixir_runtime/guest_symbolizer.h"

namespace elixir::runtime {
namespace {

constexpr std::int64_t kStallUs = 30000;
constexpr std::int64_t kSnapshotEveryUs = 25000;
constexpr std::int64_t kContinuousUs = 20000;
constexpr std::int64_t kBusySnapshotEveryUs = 100000;
constexpr std::int64_t kTraceIdleUs = 60000000;
constexpr ULONGLONG kRefreshEveryMs = 1000;
constexpr std::size_t kStackWords = 512;
constexpr std::size_t kPaddingWords = 512;

std::string Utf8(const wchar_t* text, int length) {
  if (text == nullptr || length == 0) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
  std::string result(size > 0 ? static_cast<std::size_t>(size) : 0, '\0');
  if (size > 0) WideCharToMultiByte(CP_UTF8, 0, text, length, result.data(), size, nullptr, nullptr);
  return result;
}

std::string Hex(std::uintptr_t value) {
  char text[24];
  std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
  return text;
}

std::string ShortName(std::string name) {
  const std::size_t paren = name.find('(');
  if (paren != std::string::npos && paren > 0) name.resize(paren);
  if (name.size() > 100) name = name.substr(0, 97) + "...";
  std::size_t cut = name.rfind("::");
  if (cut != std::string::npos && cut > 0) {
    const std::size_t before = name.rfind("::", cut - 1);
    if (before != std::string::npos) name = name.substr(before + 2);
  }
  return name;
}

struct Module {
  std::uintptr_t base = 0;
  std::uintptr_t end = 0;
  std::string name;
  bool windows = false;
  bool driver = false;
};

class Modules {
 public:
  bool Refresh() {
    std::vector<HMODULE> handles(1024);
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), handles.data(), static_cast<DWORD>(handles.size() * sizeof(HMODULE)),
                            &needed)) {
      return false;
    }
    handles.resize(std::min<std::size_t>(handles.size(), needed / sizeof(HMODULE)));
    if (handles == handles_) return false;
    handles_ = handles;
    wchar_t windows_dir[MAX_PATH] = {};
    const UINT windows_length = GetWindowsDirectoryW(windows_dir, MAX_PATH);
    std::vector<Module> modules;
    for (HMODULE handle : handles) {
      MODULEINFO info{};
      wchar_t path[MAX_PATH] = {};
      if (!GetModuleInformation(GetCurrentProcess(), handle, &info, sizeof(info))) continue;
      const DWORD length = GetModuleFileNameW(handle, path, MAX_PATH);
      std::wstring full(path, length);
      std::wstring lower = full;
      std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return std::towlower(c); });
      std::wstring folder(windows_dir, windows_length);
      std::transform(folder.begin(), folder.end(), folder.begin(), [](wchar_t c) { return std::towlower(c); });
      Module module;
      module.base = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
      module.end = module.base + info.SizeOfImage;
      const std::wstring base_name = lower.substr(lower.find_last_of(L"\\/") + 1);
      module.name = Utf8(base_name.c_str(), static_cast<int>(base_name.size()));
      module.driver = lower.find(L"\\driverstore\\") != std::wstring::npos;
      module.windows = !folder.empty() && lower.rfind(folder, 0) == 0 && !module.driver;
      modules.push_back(std::move(module));
    }
    std::sort(modules.begin(), modules.end(), [](const Module& a, const Module& b) { return a.base < b.base; });
    modules_ = std::move(modules);
    return true;
  }

  const Module* Find(std::uintptr_t address) const {
    auto it = std::upper_bound(modules_.begin(), modules_.end(), address,
                               [](std::uintptr_t value, const Module& module) { return value < module.base; });
    if (it == modules_.begin()) return nullptr;
    --it;
    return address < it->end ? &*it : nullptr;
  }

 private:
  std::vector<HMODULE> handles_;
  std::vector<Module> modules_;
};

class Symbols {
 public:
  ~Symbols() {
    if (ready_) SymCleanup(process_);
    if (process_ != nullptr) CloseHandle(process_);
  }

  std::string Name(std::uintptr_t address) {
    if (!Ready()) return {};
    const auto found = cache_.find(address);
    if (found != cache_.end()) return found->second;
    if (cache_.size() > 50000) cache_.clear();
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512] = {};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;
    DWORD64 displacement = 0;
    std::string name;
    if (SymFromAddr(process_, address, &displacement, symbol)) name = ShortName(symbol->Name);
    return cache_.emplace(address, std::move(name)).first->second;
  }

  void Refresh() {
    if (ready_) SymRefreshModuleList(process_);
  }

 private:
  bool Ready() {
    if (ready_ || failed_) return ready_;
    failed_ = true;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &process_, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      process_ = nullptr;
      return false;
    }
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    std::wstring search;
    for (HMODULE module : {static_cast<HMODULE>(nullptr), ThisModule()}) {
      wchar_t path[MAX_PATH] = {};
      const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
      std::wstring folder(path, length);
      folder.erase(folder.find_last_of(L"\\/") + 1);
      if (!search.empty()) search += L";";
      search += folder;
    }
    ready_ = SymInitializeW(process_, search.c_str(), TRUE) != FALSE;
    failed_ = !ready_;
    return ready_;
  }

  static HMODULE ThisModule() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ThisModule), &module);
    return module;
  }

  HANDLE process_ = nullptr;
  bool ready_ = false;
  bool failed_ = false;
  std::unordered_map<std::uintptr_t, std::string> cache_;
};

struct ThreadEntry {
  DWORD id = 0;
  HANDLE handle = nullptr;
  std::string name;
  std::string label;
};

std::string PrioritySuffix(HANDLE thread) {
  switch (GetThreadPriority(thread)) {
    case THREAD_PRIORITY_NORMAL:
    case THREAD_PRIORITY_ERROR_RETURN:
      return {};
    case THREAD_PRIORITY_IDLE:
      return " [idle]";
    case THREAD_PRIORITY_LOWEST:
      return " [lowest]";
    case THREAD_PRIORITY_BELOW_NORMAL:
      return " [below normal]";
    case THREAD_PRIORITY_ABOVE_NORMAL:
      return " [above normal]";
    case THREAD_PRIORITY_HIGHEST:
      return " [highest]";
    case THREAD_PRIORITY_TIME_CRITICAL:
      return " [time critical]";
    default:
      return " [priority " + std::to_string(GetThreadPriority(thread)) + "]";
  }
}

struct Sample {
  CONTEXT context;
  std::uintptr_t stack[kStackWords + kPaddingWords] = {};
  SIZE_T copied = 0;
  bool ok = false;
};

struct FrameLookup {
  PRUNTIME_FUNCTION function = nullptr;
  DWORD64 image_base = 0;
  std::uint8_t frame_register = 0;
  bool ok = false;
};

std::uint32_t LoadDword(const void* address) {
  std::uint32_t value = 0;
  std::memcpy(&value, address, sizeof(value));
  return value;
}

std::uint32_t (*volatile g_load_dword)(const void*) = &LoadDword;

__declspec(noinline) FrameLookup LookupFrame(DWORD64 rip) noexcept {
  FrameLookup lookup;
  __try {
    lookup.function = RtlLookupFunctionEntry(rip, &lookup.image_base, nullptr);
    if (lookup.function != nullptr) {
      const std::uint32_t unwind_data = g_load_dword(&lookup.function->UnwindData);
      const std::uint32_t header = g_load_dword(reinterpret_cast<const void*>(lookup.image_base + unwind_data));
      lookup.frame_register = static_cast<std::uint8_t>((header >> 24) & 0x0F);
    }
    lookup.ok = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    lookup.function = nullptr;
    lookup.ok = false;
  }
  return lookup;
}

__declspec(noinline) bool UnwindFrame(const FrameLookup& lookup, CONTEXT* context) noexcept {
  __try {
    PVOID handler_data = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, lookup.image_base, context->Rip, lookup.function, context, &handler_data,
                     &establisher, nullptr);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

template <typename FindModule>
std::uintptr_t UnwindTo(const Sample& sample, FindModule find, bool stop_outside_windows);

template <typename FindModule>
std::uintptr_t FirstCallerOutsideWindows(const Sample& sample, FindModule find) {
  return UnwindTo(sample, find, true);
}

template <typename FindModule>
std::uintptr_t GuestCaller(const Sample& sample, FindModule find) {
  return UnwindTo(sample, find, false);
}

template <typename FindModule>
std::uintptr_t UnwindTo(const Sample& sample, FindModule find, bool stop_outside_windows) {
  CONTEXT context = sample.context;
  const std::uintptr_t original = context.Rsp;
  const auto copy = reinterpret_cast<std::uintptr_t>(sample.stack);
  const std::uintptr_t copied = sample.copied;
  const auto in_copy = [&](std::uintptr_t address) { return address >= copy && address + 8 <= copy + copied; };
  const auto rebase = [&](DWORD64* reg) {
    if (*reg >= original && *reg < original + copied) *reg = copy + (*reg - original);
  };
  rebase(&context.Rsp);
  rebase(&context.Rbp);
  for (int frame = 0; frame < 24; ++frame) {
    const auto* module = find(static_cast<std::uintptr_t>(context.Rip));
    if (module == nullptr) return stop_outside_windows ? 0 : static_cast<std::uintptr_t>(context.Rip);
    if (stop_outside_windows && !module->windows) return static_cast<std::uintptr_t>(context.Rip);
    if (!in_copy(static_cast<std::uintptr_t>(context.Rsp))) return 0;
    const FrameLookup lookup = LookupFrame(context.Rip);
    if (!lookup.ok) return 0;
    if (lookup.function == nullptr) {
      context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
      context.Rsp += 8;
    } else {
      if (lookup.frame_register != 0 &&
          (lookup.frame_register != 5 || !in_copy(static_cast<std::uintptr_t>(context.Rbp)))) {
        return 0;
      }
      if (!UnwindFrame(lookup, &context)) return 0;
    }
    rebase(&context.Rbp);
    if (context.Rip == 0) return 0;
  }
  return 0;
}

struct Place {
  bool waiting = false;
  bool driver = false;
  std::string text;
};

std::string CodeKind(std::uintptr_t address) {
  MEMORY_BASIC_INFORMATION region{};
  if (VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) != 0 &&
      region.Type == MEM_PRIVATE &&
      (region.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0) {
    return "generated code";
  }
  return "unknown code";
}

}

class StallProfiler::Impl {
 public:
  Impl(const std::atomic<std::int64_t>* last_present_us, const std::atomic<std::int64_t>* average_gap_us,
       const std::atomic<DWORD>* present_thread, std::int64_t (*now_us)())
      : last_present_us_(last_present_us),
        average_gap_us_(average_gap_us),
        present_thread_(present_thread),
        now_us_(now_us) {}

  ~Impl() { Stop(); }

  void Start() {
    if (thread_.joinable()) return;
    stop_ = false;
    thread_ = std::thread([this] { Run(); });
  }

  void Stop() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    bionic::wake_trace::Enable(false);
  }

  std::vector<std::string> Report() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> lines;
    if (busy_snapshots_ > 0 && busy_running_ > 0) {
      std::vector<std::pair<unsigned, std::string>> modules;
      for (const auto& [name, count] : busy_modules_) modules.emplace_back(count, name);
      std::sort(modules.begin(), modules.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
      std::string line = "busy profile: " + std::to_string(busy_snapshots_) +
                         " snapshots while frames came smoothly, " + std::to_string(busy_running_) +
                         " running threads seen; where they ran:";
      for (std::size_t i = 0; i < modules.size() && i < 8; ++i) {
        line += (i == 0 ? " " : ", ") + modules[i].second + " " +
                std::to_string(modules[i].first * 100 / busy_running_) + "%";
      }
      lines.push_back(line);
      std::vector<std::pair<unsigned, std::string>> places;
      for (const auto& [key, count] : busy_places_) places.emplace_back(count, key);
      std::sort(places.begin(), places.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
      std::string busiest = "  busiest (share of snapshots):";
      for (std::size_t i = 0; i < places.size() && i < 10; ++i) {
        busiest += (i == 0 ? " " : "; ") + places[i].second + " " +
                   std::to_string(places[i].first * 100 / busy_snapshots_) + "%";
      }
      lines.push_back(busiest);
      if (!busy_own_.empty()) {
        std::vector<std::pair<unsigned, std::string>> own;
        for (const auto& [name, count] : busy_own_) own.emplace_back(count, name);
        std::sort(own.begin(), own.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        std::string inside = "  in Elixir's own code (share of the running threads seen):";
        for (std::size_t i = 0; i < own.size() && i < 8; ++i) {
          const unsigned tenths = own[i].first * 1000 / busy_running_;
          inside += (i == 0 ? " " : "; ") + own[i].second + " " + std::to_string(tenths / 10) + "." +
                    std::to_string(tenths % 10) + "%";
        }
        lines.push_back(inside);
      }
    }
    if (presenter_seen_ > 0) {
      const auto share = [this](unsigned count) { return std::to_string(count * 100 / presenter_seen_) + "%"; };
      unsigned elsewhere = 0;
      const std::pair<const std::string, unsigned>* most = nullptr;
      for (const auto& wait : presenter_waits_) {
        elsewhere += wait.second;
        if (most == nullptr || wait.second > most->second) most = &wait;
      }
      std::string line = "  presenting thread: running in " + share(presenter_ran_) +
                         " of the snapshots, waiting in the graphics driver in " + share(presenter_in_driver_) +
                         ", waiting elsewhere in " + share(elsewhere);
      if (most != nullptr) line += " (most often " + most->first + ", " + share(most->second) + ")";
      lines.push_back(line);
    }
    presenter_waits_.clear();
    presenter_seen_ = 0;
    presenter_ran_ = 0;
    presenter_in_driver_ = 0;
    busy_modules_.clear();
    busy_places_.clear();
    busy_own_.clear();
    busy_snapshots_ = 0;
    busy_running_ = 0;
    if (traced_ > 0) {
      lines.push_back("stall wake chains: " + std::to_string(traced_) + " stalls traced (" +
                      "presenting thread < the thread that woke it < what woke that one ...)");
      std::vector<std::pair<std::string, ChainStats>> sorted(chains_.begin(), chains_.end());
      std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.second.count > b.second.count; });
      for (std::size_t i = 0; i < sorted.size() && i < 6; ++i) {
        const ChainStats& stats = sorted[i].second;
        lines.push_back("  " + std::to_string(stats.count) + "x, " +
                        std::to_string(stats.total_us / stats.count / 1000) + " ms on average: " + stats.example);
      }
    }
    chains_.clear();
    traced_ = 0;
    if (snapshots_ > 0) {
      lines.push_back("stall profile: " + std::to_string(snapshots_) + " snapshots of up to " +
                      std::to_string(max_threads_) + " threads while frames were stalled");
      const auto percent = [this](unsigned count) { return std::to_string(count * 100 / snapshots_) + "%"; };
      std::vector<std::tuple<unsigned, std::string, std::string, bool, bool, bool>> entries;
      for (const auto& [key, count] : counts_) {
        entries.emplace_back(count, std::get<1>(key), std::get<2>(key), std::get<3>(key), std::get<0>(key),
                             std::get<4>(key));
      }
      std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
      const auto list = [&](const char* title, int kind, std::size_t limit) {
        const bool presents = kind == 0;
        std::string line = std::string("  ") + title + ":";
        std::size_t shown = 0;
        for (const auto& entry : entries) {
          if (std::get<4>(entry) != presents) continue;
          if (kind == 1 && !std::get<5>(entry)) continue;
          if (kind >= 2 && (std::get<5>(entry) || std::get<3>(entry) != (kind == 3))) continue;
          line += (shown == 0 ? " " : "; ") + (presents ? std::string() : std::get<1>(entry) + ": ") + std::get<2>(entry) +
                  " " + percent(std::get<0>(entry));
          if (++shown == limit) break;
        }
        if (shown > 0) lines.push_back(line);
      };
      list(("presenting thread " + presenter_name_).c_str(), 0, 4);
      list("in the GPU driver", 1, 6);
      list("running", 2, 8);
      list("waiting", 3, 8);
    }
    counts_.clear();
    snapshots_ = 0;
    max_threads_ = 0;
    return lines;
  }

 private:
  void Run() {
    std::int64_t last_snapshot = 0;
    std::int64_t last_busy_snapshot = 0;
    std::int64_t busy_gap = kBusySnapshotEveryUs;
    std::int64_t stall_from = 0;
    std::uint32_t random = (static_cast<std::uint32_t>(GetTickCount64()) ^ (GetCurrentThreadId() << 8)) | 1u;
    const auto next_random = [&random](std::uint32_t below) {
      random ^= random << 13;
      random ^= random >> 17;
      random ^= random << 5;
      return random % below;
    };
    while (!stop_.load()) {
      Sleep(5);
      const std::int64_t last = last_present_us_->load(std::memory_order_relaxed);
      const std::int64_t now = now_us_();
      if (stall_from != 0 && last != stall_from) {
        TraceStallEnd(stall_from, last);
        stall_from = 0;
      }
      if (trace_on_ && now - last_stall_us_ > kTraceIdleUs) {
        bionic::wake_trace::Enable(false);
        trace_on_ = false;
      }
      const bool continuous = average_gap_us_->load(std::memory_order_relaxed) < kContinuousUs;
      if (last == 0 || now - last < kStallUs) {
        if (last != 0 && continuous && now - last_busy_snapshot >= busy_gap) {
          const std::int64_t off_tick = now + next_random(900);
          while (now_us_() < off_tick) YieldProcessor();
          last_busy_snapshot = now;
          busy_gap = kBusySnapshotEveryUs * (80 + static_cast<std::int64_t>(next_random(41))) / 100;
          BusySnapshot();
        }
        continue;
      }
      if (stall_from == 0 && !continuous) continue;
      if (stall_from == 0) {
        stall_from = last;
        last_stall_us_ = now;
        if (!trace_on_) {
          bionic::wake_trace::Enable(true);
          trace_on_ = true;
          trace_since_us_ = now;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        stall_places_.clear();
        stall_snapshots_ = 0;
      }
      if (now - last_snapshot < kSnapshotEveryUs) continue;
      last_snapshot = now;
      Snapshot();
    }
    for (ThreadEntry& entry : threads_) {
      if (entry.handle != nullptr) CloseHandle(entry.handle);
    }
    threads_.clear();
  }

  std::vector<std::pair<DWORD, HANDLE>> OpenThreads() {
    using NextThreadFn = LONG(NTAPI*)(HANDLE, HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE);
    static const auto next_thread = reinterpret_cast<NextThreadFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtGetNextThread")));
    constexpr ACCESS_MASK kAccess = THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION;
    std::vector<std::pair<DWORD, HANDLE>> threads;
    const DWORD self = GetCurrentThreadId();
    if (next_thread != nullptr) {
      HANDLE current = nullptr;
      for (;;) {
        HANDLE following = nullptr;
        const LONG status = next_thread(GetCurrentProcess(), current, kAccess, 0, 0, &following);
        if (current != nullptr && (threads.empty() || threads.back().second != current)) CloseHandle(current);
        if (status < 0 || following == nullptr) break;
        current = following;
        const DWORD id = GetThreadId(following);
        if (id != 0 && id != self) threads.emplace_back(id, following);
        if (threads.size() > 4096) break;
      }
      return threads;
    }
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return threads;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    const DWORD pid = GetCurrentProcessId();
    for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
      if (entry.th32OwnerProcessID != pid || entry.th32ThreadID == self) continue;
      if (HANDLE handle = OpenThread(kAccess, FALSE, entry.th32ThreadID)) threads.emplace_back(entry.th32ThreadID, handle);
    }
    CloseHandle(snapshot);
    return threads;
  }

  void RefreshThreads() {
    const ULONGLONG now = GetTickCount64();
    if (!threads_.empty() && now - refreshed_ms_ < kRefreshEveryMs) return;
    refreshed_ms_ = now;
    if (modules_.Refresh()) symbols_.Refresh();
    std::vector<ThreadEntry> kept;
    for (const auto& [id, handle] : OpenThreads()) {
      auto it = std::find_if(threads_.begin(), threads_.end(), [id = id](const ThreadEntry& t) { return t.id == id; });
      if (it != threads_.end() && it->handle != nullptr) {
        CloseHandle(handle);
        kept.push_back(std::move(*it));
        it->handle = nullptr;
        continue;
      }
      ThreadEntry added;
      added.id = id;
      added.handle = handle;
      PWSTR description = nullptr;
      if (SUCCEEDED(GetThreadDescription(added.handle, &description)) && description != nullptr) {
        added.name = Utf8(description, static_cast<int>(wcslen(description)));
        LocalFree(description);
      }
      if (added.name.empty()) added.name = "tid " + std::to_string(id);
      kept.push_back(std::move(added));
    }
    for (ThreadEntry& old : threads_) {
      if (old.handle != nullptr) CloseHandle(old.handle);
    }
    threads_ = std::move(kept);
    for (ThreadEntry& thread : threads_) thread.label = thread.name + PrioritySuffix(thread.handle);
    samples_.resize(threads_.size());
  }

  void BusySnapshot() {
    Capture();
    const DWORD presenter = present_thread_->load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(mutex_);
    ++busy_snapshots_;
    for (std::size_t i = 0; i < threads_.size(); ++i) {
      if (!samples_[i].ok) continue;
      const Place place = Classify(samples_[i]);
      if (threads_[i].id == presenter) {
        ++presenter_seen_;
        if (!place.waiting) {
          ++presenter_ran_;
        } else if (place.driver) {
          ++presenter_in_driver_;
        } else {
          ++presenter_waits_[place.text];
        }
      }
      if (place.waiting) continue;
      ++busy_running_;
      std::string module = place.text;
      const std::size_t cut = module.find_first_of("+!");
      if (cut != std::string::npos) module.resize(cut);
      ++busy_modules_[module];
      ++busy_places_[threads_[i].label + ": " + place.text];
      if (cut != std::string::npos && place.text[cut] == '!' && module == OwnModule()) {
        ++busy_own_[place.text.substr(cut + 1)];
      }
    }
  }

  static const std::string& OwnModule() {
    static const std::string* const name = [] {
      auto* result = new std::string();
      HMODULE module = nullptr;
      wchar_t path[MAX_PATH] = {};
      if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(&OwnModule), &module) &&
          GetModuleFileNameW(module, path, MAX_PATH) != 0) {
        const wchar_t* base = path;
        for (const wchar_t* p = path; *p != L'\0'; ++p) {
          if (*p == L'\\' || *p == L'/') base = p + 1;
        }
        *result = Utf8(base, static_cast<int>(std::wcslen(base)));
      }
      return result;
    }();
    return *name;
  }

  void Capture() {
    RefreshThreads();
    for (std::size_t i = 0; i < threads_.size(); ++i) {
      Sample& sample = samples_[i];
      sample.ok = false;
      sample.copied = 0;
      sample.context = CONTEXT{};
      sample.context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
      if (SuspendThread(threads_[i].handle) == static_cast<DWORD>(-1)) continue;
      if (GetThreadContext(threads_[i].handle, &sample.context)) {
        sample.ok = true;
        for (const SIZE_T size : {kStackWords * sizeof(std::uintptr_t), SIZE_T{1024}, SIZE_T{256}}) {
          if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(sample.context.Rsp), sample.stack,
                                size, &sample.copied)) {
            break;
          }
          sample.copied = 0;
        }
      }
      ResumeThread(threads_[i].handle);
    }
  }

  void Snapshot() {
    Capture();
    const DWORD presenter = present_thread_->load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(mutex_);
    ++snapshots_;
    ++stall_snapshots_;
    max_threads_ = std::max<std::size_t>(max_threads_, threads_.size());
    for (std::size_t i = 0; i < threads_.size(); ++i) {
      if (!samples_[i].ok) continue;
      const Place place = Classify(samples_[i]);
      const bool presents = threads_[i].id == presenter;
      if (presents) presenter_name_ = threads_[i].label;
      ++counts_[std::make_tuple(presents, threads_[i].label, place.text, place.waiting, place.driver)];
      ++stall_places_[threads_[i].id][place.text];
    }
  }

  std::string MeanwhileLocked(DWORD id) const {
    const auto thread = stall_places_.find(id);
    if (thread == stall_places_.end() || stall_snapshots_ == 0) return {};
    const auto top = std::max_element(thread->second.begin(), thread->second.end(),
                                      [](const auto& a, const auto& b) { return a.second < b.second; });
    if (top == thread->second.end()) return {};
    return " [meanwhile: " + top->first + " " + std::to_string(top->second * 100 / stall_snapshots_) + "%]";
  }

  std::string GuestName(std::uintptr_t address, bool bucket) {
    const std::uintptr_t page = address & ~static_cast<std::uintptr_t>(0xFFF);
    auto found = guest_pages_.find(page);
    if (found == guest_pages_.end()) {
      if (guest_pages_.size() > 20000) guest_pages_.clear();
      GuestPage entry;
      platform::SymbolInfo info;
      if (SymbolizeGuestAddress(address, &info) && !info.module.empty()) {
        entry.module = info.module;
        entry.base = address - info.module_offset;
      } else {
        entry.kind = CodeKind(address);
      }
      found = guest_pages_.emplace(page, std::move(entry)).first;
    }
    const GuestPage& entry = found->second;
    if (entry.module.empty()) return entry.kind;
    const std::uintptr_t offset = address - entry.base;
    return entry.module + "+" + Hex(bucket ? offset & ~static_cast<std::uintptr_t>(0xFFF) : offset);
  }

  std::string SiteName(std::uintptr_t site) {
    if (site == 0) return "?";
    if (const Module* module = modules_.Find(site)) {
      const std::string function = symbols_.Name(site);
      return module->name + "!" + (function.empty() ? Hex(site - module->base) : function);
    }
    return GuestName(site, false);
  }

  std::string LabelOf(DWORD id) const {
    for (const ThreadEntry& thread : threads_) {
      if (thread.id == id) return thread.label;
    }
    return "tid " + std::to_string(id);
  }

  static std::string Milliseconds(std::int64_t ns) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f ms", static_cast<double>(ns) / 1e6);
    return text;
  }

  void TraceStallEnd(std::int64_t from_us, std::int64_t to_us) {
    const DWORD presenter = present_thread_->load(std::memory_order_relaxed);
    if (presenter == 0) return;
    if (!trace_on_ || from_us < trace_since_us_) return;
    RefreshThreads();
    const std::vector<bionic::wake_trace::Step> chain =
        bionic::wake_trace::Chain(presenter, from_us * 1000, to_us * 1000, 6);
    std::string key, text;
    const auto add = [&](const std::string& what, const std::string& numbers) {
      key += what;
      text += what + numbers;
    };
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::size_t i = 0; i < chain.size(); ++i) {
      const bionic::wake_trace::Step& step = chain[i];
      const bool last = i + 1 == chain.size();
      if (i == 0) {
        add("presenting thread", "");
      } else {
        add(" < woken by " + LabelOf(step.thread) + " from " + SiteName(step.wake_site), "");
      }
      if (!step.found) {
        add(i == 0 ? ": no wait of it recorded (running, or waiting in the GPU driver)"
                   : ", which had not waited 1 ms or more in the 2 s before (busy, or waiting outside bionic)",
            "");
        text += MeanwhileLocked(step.thread);
        break;
      }
      if (i > 0) add(",", " " + Milliseconds(step.ran_ns) + " after" + (step.ran_ns >= 5000000 ? MeanwhileLocked(step.thread) : ""));
      add(" its wait at " + SiteName(step.wait_site), " (" + Milliseconds(step.waited_ns) + ")");
      using bionic::wake_trace::Event;
      switch (step.ended_by) {
        case Event::kWoken:
          if (last) add(" ended by a wake nobody recorded (a host thread, or spurious)", "");
          break;
        case Event::kTimedOut: add(" ended by its timeout", ""); break;
        case Event::kIoReady: add(" was poll/epoll_wait (I/O or its timeout)", ""); break;
        case Event::kSlept: add(" was a sleep", ""); break;
        case Event::kWake: break;
      }
    }
    ++traced_;
    ChainStats& stats = chains_[key];
    ++stats.count;
    stats.total_us += to_us - from_us;
    stats.example = text;
  }

  Place Classify(const Sample& sample) {
    const std::uintptr_t rip = sample.context.Rip;
    const Module* module = modules_.Find(rip);
    if (module == nullptr) return {false, false, GuestName(rip, true)};
    const std::string function = symbols_.Name(rip);
    if (!module->windows) {
      return {false, module->driver, module->name + "!" + (function.empty() ? Hex(rip - module->base) : function)};
    }
    Place place{true, false, function.empty() ? module->name : function};
    std::uintptr_t caller_address =
        FirstCallerOutsideWindows(sample, [this](std::uintptr_t address) { return modules_.Find(address); });
    bool scanned = false;
    if (caller_address == 0) {
      const std::size_t words = std::min<std::size_t>(sample.copied / sizeof(std::uintptr_t), kStackWords);
      for (std::size_t i = 0; i < words && caller_address == 0; ++i) {
        const Module* caller = sample.stack[i] >= 0x10000 ? modules_.Find(sample.stack[i]) : nullptr;
        if (caller != nullptr && !caller->windows) caller_address = sample.stack[i];
      }
      scanned = true;
    }
    if (const Module* caller = caller_address != 0 ? modules_.Find(caller_address) : nullptr) {
      const std::string caller_function = symbols_.Name(caller_address);
      place.driver = caller->driver;
      place.text += std::string(scanned ? " < ~" : " < ") + caller->name + "!" +
                    (caller_function.empty() ? Hex(caller_address - caller->base) : caller_function);
      if (!scanned) {
        const std::uintptr_t guest =
            GuestCaller(sample, [this](std::uintptr_t address) { return modules_.Find(address); });
        if (guest != 0) place.text += " < " + GuestName(guest, false);
      }
    }
    return place;
  }

  const std::atomic<std::int64_t>* last_present_us_;
  const std::atomic<std::int64_t>* average_gap_us_;
  const std::atomic<DWORD>* present_thread_;
  std::int64_t (*now_us_)();
  std::thread thread_;
  std::atomic<bool> stop_{false};

  Modules modules_;
  Symbols symbols_;
  std::vector<ThreadEntry> threads_;
  std::vector<Sample> samples_;
  ULONGLONG refreshed_ms_ = 0;

  std::mutex mutex_;
  std::map<std::tuple<bool, std::string, std::string, bool, bool>, unsigned> counts_;
  struct GuestPage {
    std::string module;
    std::uintptr_t base = 0;
    std::string kind;
  };
  std::unordered_map<std::uintptr_t, GuestPage> guest_pages_;
  struct ChainStats {
    unsigned count = 0;
    std::int64_t total_us = 0;
    std::string example;
  };
  std::map<std::string, ChainStats> chains_;
  unsigned traced_ = 0;
  std::unordered_map<DWORD, std::map<std::string, unsigned>> stall_places_;
  unsigned stall_snapshots_ = 0;
  std::map<std::string, unsigned> busy_modules_;
  std::map<std::string, unsigned> busy_places_;
  std::map<std::string, unsigned> busy_own_;
  unsigned busy_snapshots_ = 0;
  unsigned busy_running_ = 0;
  std::map<std::string, unsigned> presenter_waits_;
  unsigned presenter_seen_ = 0;
  unsigned presenter_ran_ = 0;
  unsigned presenter_in_driver_ = 0;
  bool trace_on_ = false;
  std::int64_t trace_since_us_ = 0;
  std::int64_t last_stall_us_ = 0;
  unsigned snapshots_ = 0;
  std::size_t max_threads_ = 0;
  std::string presenter_name_;
};

StallProfiler::StallProfiler(const std::atomic<std::int64_t>* last_present_us,
                             const std::atomic<std::int64_t>* average_gap_us,
                             const std::atomic<DWORD>* present_thread, std::int64_t (*now_us)())
    : impl_(std::make_unique<Impl>(last_present_us, average_gap_us, present_thread, now_us)) {}

StallProfiler::~StallProfiler() = default;

bool StallProfilerCanReadUnwindData(std::uintptr_t address) noexcept { return LookupFrame(address).ok; }

void StallProfiler::Start() { impl_->Start(); }

void StallProfiler::Stop() { impl_->Stop(); }

std::vector<std::string> StallProfiler::Report() { return impl_->Report(); }

}
