// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "platform/windows/raw_syscall_trap.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>

namespace elixir::platform {
namespace {

constexpr std::size_t kMaxWatched = 4;
constexpr int kRegisters = 4;
constexpr int kMaxActivations = 8;
constexpr DWORD kTrapFlag = 0x100;
constexpr DWORD kResumeFlag = 0x10000;

struct Zone {
  RawSyscallFunction function;
  std::uintptr_t image_base = 0;
  std::string image;
  std::uintptr_t idle_stop = 0;
};

struct Snapshot {
  std::uint64_t generation = 0;
  std::size_t count = 0;
  const Zone* zones[kMaxWatched] = {};
};

const Snapshot kNothing{};
std::atomic<const Snapshot*> g_snapshot{&kNothing};
std::atomic<RawSyscallHandler> g_handler{nullptr};
std::atomic<int> g_enabled{-1};
std::atomic<std::uint64_t> g_handled{0}, g_stops{0}, g_traced{0}, g_unwatched{0};
std::atomic<std::size_t> g_known{0};

std::mutex g_writers;
std::vector<Zone*>* g_zones = nullptr;
std::uint64_t g_generation = 0;
PVOID g_handle = nullptr;

struct Activation {
  const Zone* zone = nullptr;
  std::uintptr_t rsp = 0;
  std::uint64_t next = 0;
  bool leaves = false;
  bool at_entry = false;
  bool tracing = false;
  std::uintptr_t over_return = 0;
  std::uintptr_t over_rsp = 0;
};

struct ThreadTrap {
  Activation activations[kMaxActivations];
  int depth = 0;
  bool jump_step = false;
  bool stepping = false;
  bool step_is_call = false;
  std::uintptr_t step_return = 0;
  std::uint64_t generation = 0;
  std::uintptr_t registers[kRegisters] = {};
};
thread_local ThreadTrap t_trap;

constexpr std::uint64_t kNeverArmed = ~std::uint64_t{0};

int Count(std::uint64_t bits) noexcept { return __builtin_popcountll(bits); }
unsigned Lowest(std::uint64_t bits) noexcept { return static_cast<unsigned>(__builtin_ctzll(bits)); }

bool IsStop(X86Flow flow) noexcept { return flow == X86Flow::kSyscall || flow == X86Flow::kIndirectJump; }

bool Fits(const Activation& a) noexcept { return Count(a.next) + (a.leaves ? 1 : 0) <= kRegisters; }

std::uintptr_t IdleStop(const RawSyscallFunction& f) noexcept {
  const std::uint64_t first = f.next[0];
  if (first != 0 && (first & (first - 1)) == 0) return f.AddressOf(f.stops[Lowest(first)]);
  return f.start;
}

struct Wanted {
  std::uintptr_t address[kRegisters] = {};
  int count = 0;
  int dropped = 0;

  void Add(std::uintptr_t value, bool required) noexcept {
    if (value == 0) return;
    for (int i = 0; i < count; ++i) {
      if (address[i] == value) return;
    }
    if (count < kRegisters) {
      address[count++] = value;
    } else if (required) {
      ++dropped;
    }
  }
};

Wanted Compute(const ThreadTrap& t, const Snapshot& s) noexcept {
  Wanted wanted;
  for (int i = t.depth - 1; i >= 0; --i) {
    const Activation& a = t.activations[i];
    if (a.tracing) {
      wanted.Add(a.over_return, true);
    } else {
      const RawSyscallFunction& f = a.zone->function;
      for (std::uint64_t bits = a.next; bits != 0; bits &= bits - 1) {
        wanted.Add(f.AddressOf(f.stops[Lowest(bits)]), true);
      }
    }
    wanted.Add(a.zone->idle_stop, false);
  }
  for (std::size_t z = 0; z < s.count; ++z) wanted.Add(s.zones[z]->idle_stop, false);
  return wanted;
}

void WriteRegisters(ThreadTrap& t, const Snapshot& s, CONTEXT* c) noexcept {
  const Wanted wanted = Compute(t, s);
  DWORD64* const registers[kRegisters] = {&c->Dr0, &c->Dr1, &c->Dr2, &c->Dr3};
  DWORD64 enable = 0;
  for (int i = 0; i < kRegisters; ++i) {
    *registers[i] = i < wanted.count ? wanted.address[i] : 0;
    t.registers[i] = static_cast<std::uintptr_t>(*registers[i]);
    if (i < wanted.count) enable |= DWORD64{1} << (2 * i);
  }
  c->Dr6 = 0;
  c->Dr7 = enable;
  c->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
  t.generation = s.generation;
  if (wanted.dropped != 0) g_unwatched.fetch_add(static_cast<std::uint64_t>(wanted.dropped), std::memory_order_relaxed);
}

Activation* Push(ThreadTrap& t, const Zone* zone) noexcept {
  if (t.depth == kMaxActivations) {
    for (int i = 1; i < kMaxActivations; ++i) t.activations[i - 1] = t.activations[i];
    --t.depth;
  }
  Activation& a = t.activations[t.depth++];
  a = Activation{};
  a.zone = zone;
  return &a;
}

void Emulate(CONTEXT* c, const Zone* zone, std::ptrdiff_t index) noexcept {
  RawSyscallRequest request;
  request.number = c->Rax;
  request.args[0] = c->Rdi;
  request.args[1] = c->Rsi;
  request.args[2] = c->Rdx;
  request.args[3] = c->R10;
  request.args[4] = c->R8;
  request.args[5] = c->R9;
  request.site = static_cast<std::uintptr_t>(c->Rip);
  request.image_base = zone->image_base;
  request.image = zone->image.c_str();
  const RawSyscallHandler handler = g_handler.load(std::memory_order_acquire);
  constexpr std::int64_t kNoSuchCall = -38;
  const std::int64_t result = handler != nullptr ? handler(request) : kNoSuchCall;
  const DWORD64 after = c->Rip + zone->function.lengths[static_cast<std::size_t>(index)];
  c->Rax = static_cast<DWORD64>(result);
  c->Rcx = after;
  c->R11 = c->EFlags & ~(kTrapFlag | kResumeFlag);
  c->Rip = after;
  c->EFlags &= ~kResumeFlag;
  g_handled.fetch_add(1, std::memory_order_relaxed);
}

bool ReadWord(std::uintptr_t address, std::uintptr_t* out) noexcept {
  SIZE_T copied = 0;
  return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, sizeof(*out), &copied) &&
         copied == sizeof(*out);
}

void Trace(ThreadTrap& t, const Snapshot& s, CONTEXT* c) noexcept {
  t.stepping = false;
  while (t.depth > 0) {
    Activation& a = t.activations[t.depth - 1];
    const RawSyscallFunction& f = a.zone->function;
    const std::ptrdiff_t index = f.IndexOf(static_cast<std::uintptr_t>(c->Rip));
    if (index < 0) {
      std::uintptr_t pushed = 0;
      if (t.step_is_call && ReadWord(static_cast<std::uintptr_t>(c->Rsp), &pushed) && pushed == t.step_return) {
        a.over_return = t.step_return;
        a.over_rsp = static_cast<std::uintptr_t>(c->Rsp) + sizeof(std::uintptr_t);
      } else {
        --t.depth;
      }
      break;
    }
    const auto at = static_cast<std::size_t>(index);
    if (f.flows[at] == X86Flow::kSyscall) {
      Emulate(c, a.zone, index);
      continue;
    }
    a.rsp = static_cast<std::uintptr_t>(c->Rsp);
    a.at_entry = false;
    a.next = f.next[at];
    a.leaves = f.leaves[at] != 0;
    if (a.next == 0) {
      --t.depth;
      break;
    }
    if (Fits(a)) {
      a.tracing = false;
      break;
    }
    a.tracing = true;
    t.stepping = true;
    t.step_is_call = f.flows[at] == X86Flow::kCall || f.flows[at] == X86Flow::kIndirectCall;
    t.step_return = static_cast<std::uintptr_t>(c->Rip) + f.lengths[at];
    c->EFlags |= kTrapFlag | kResumeFlag;
    g_traced.fetch_add(1, std::memory_order_relaxed);
    WriteRegisters(t, s, c);
    return;
  }
  t.step_is_call = false;
  c->EFlags &= ~kTrapFlag;
  WriteRegisters(t, s, c);
}

void Settle(ThreadTrap& t, const Snapshot& s, CONTEXT* c) noexcept {
  if (t.depth > 0) {
    Activation& a = t.activations[t.depth - 1];
    if (a.next == 0) {
      --t.depth;
    } else if (!Fits(a)) {
      a.tracing = true;
      Trace(t, s, c);
      return;
    }
  }
  WriteRegisters(t, s, c);
}

const Zone* FindStop(const ThreadTrap& t, const Snapshot& s, std::uintptr_t address, std::ptrdiff_t* index) noexcept {
  const auto has_stop = [address, index](const Zone* zone) {
    const RawSyscallFunction& f = zone->function;
    const std::ptrdiff_t at = f.IndexOf(address);
    if (at < 0) return false;
    if (at != 0 && !IsStop(f.flows[static_cast<std::size_t>(at)])) return false;
    *index = at;
    return true;
  };
  for (int i = t.depth - 1; i >= 0; --i) {
    if (has_stop(t.activations[i].zone)) return t.activations[i].zone;
  }
  for (std::size_t z = 0; z < s.count; ++z) {
    if (has_stop(s.zones[z])) return s.zones[z];
  }
  return nullptr;
}

void Stop(ThreadTrap& t, const Snapshot& s, CONTEXT* c, const Zone* zone, std::ptrdiff_t index) noexcept {
  const RawSyscallFunction& f = zone->function;
  const auto at = static_cast<std::size_t>(index);
  const X86Flow flow = f.flows[at];
  const bool entry = !IsStop(flow);
  if (flow == X86Flow::kSyscall) Emulate(c, zone, index);
  const auto rsp = static_cast<std::uintptr_t>(c->Rsp);
  while (t.depth > 0) {
    const std::uintptr_t recorded = t.activations[t.depth - 1].rsp;
    if (recorded < rsp || (entry && recorded == rsp)) {
      --t.depth;
    } else {
      break;
    }
  }
  Activation* a = nullptr;
  if (!entry && t.depth > 0) {
    Activation& top = t.activations[t.depth - 1];
    if (top.zone == zone && (top.rsp == rsp || top.at_entry)) {
      a = &top;
    } else if (top.rsp == rsp) {
      --t.depth;
    }
  }
  if (a == nullptr) a = Push(t, zone);
  a->rsp = rsp;
  a->at_entry = entry;
  a->tracing = false;
  a->over_return = 0;
  a->over_rsp = 0;
  if (flow == X86Flow::kSyscall) {
    const std::size_t after = at + 1;
    if (after < f.offsets.size()) {
      a->next = f.next[after];
      a->leaves = f.leaves[after] != 0;
    } else {
      a->next = 0;
      a->leaves = true;
    }
    Settle(t, s, c);
  } else if (flow == X86Flow::kIndirectJump) {
    a->next = f.next[at];
    a->leaves = false;
    t.jump_step = true;
    c->EFlags |= kTrapFlag | kResumeFlag;
  } else {
    a->next = f.next[0];
    a->leaves = f.leaves[0] != 0;
    c->EFlags |= kResumeFlag;
    Settle(t, s, c);
  }
}

LONG CALLBACK TrapHandler(EXCEPTION_POINTERS* info) {
  if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
  CONTEXT* const c = info->ContextRecord;
  ThreadTrap& t = t_trap;
  const Snapshot& s = *g_snapshot.load(std::memory_order_acquire);
  const auto rip = static_cast<std::uintptr_t>(c->Rip);

  if (t.jump_step) {
    t.jump_step = false;
    c->EFlags &= ~(kTrapFlag | kResumeFlag);
    if (t.depth > 0) {
      Activation& a = t.activations[t.depth - 1];
      const RawSyscallFunction& f = a.zone->function;
      const std::ptrdiff_t index = f.IndexOf(rip);
      if (index >= 0) {
        a.next = f.next[static_cast<std::size_t>(index)];
        a.leaves = f.leaves[static_cast<std::size_t>(index)] != 0;
        a.rsp = static_cast<std::uintptr_t>(c->Rsp);
      } else {
        --t.depth;
      }
    }
    Settle(t, s, c);
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  if (t.stepping) {
    c->EFlags &= ~(kTrapFlag | kResumeFlag);
    Trace(t, s, c);
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  for (int i = t.depth - 1; i >= 0; --i) {
    Activation& a = t.activations[i];
    if (a.tracing && a.over_return == rip && a.over_rsp == static_cast<std::uintptr_t>(c->Rsp)) {
      t.depth = i + 1;
      a.over_return = 0;
      a.over_rsp = 0;
      g_stops.fetch_add(1, std::memory_order_relaxed);
      Trace(t, s, c);
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  }
  std::ptrdiff_t index = -1;
  if (const Zone* zone = FindStop(t, s, rip, &index)) {
    g_stops.fetch_add(1, std::memory_order_relaxed);
    Stop(t, s, c, zone, index);
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  for (const std::uintptr_t watched : t.registers) {
    if (watched != 0 && watched == rip) {
      c->EFlags |= kResumeFlag;
      WriteRegisters(t, s, c);
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

void PublishLocked() {
  std::vector<Zone*> order(*g_zones);
  std::stable_sort(order.begin(), order.end(),
                   [](const Zone* a, const Zone* b) { return a->function.syscalls > b->function.syscalls; });
  auto* snapshot = new Snapshot();
  snapshot->generation = ++g_generation;
  for (const Zone* zone : order) {
    if (snapshot->count == kMaxWatched) break;
    snapshot->zones[snapshot->count++] = zone;
  }
  g_snapshot.store(snapshot, std::memory_order_release);
}

}

void SetRawSyscallHandler(RawSyscallHandler handler) noexcept {
  g_handler.store(handler, std::memory_order_release);
}

bool RawSyscallTrapEnabled() noexcept {
  int enabled = g_enabled.load(std::memory_order_acquire);
  if (enabled < 0) {
    char value[8] = {};
    const DWORD length = GetEnvironmentVariableA("ELIXIR_RAW_SYSCALLS", value, sizeof(value));
    enabled = (length == 1 && value[0] == '0') ? 0 : 1;
    g_enabled.store(enabled, std::memory_order_release);
  }
  return enabled != 0;
}

void SetRawSyscallTrapEnabled(bool enabled) noexcept { g_enabled.store(enabled ? 1 : 0, std::memory_order_release); }

RawSyscallWatch AddRawSyscallFunctions(std::vector<RawSyscallFunction> functions, std::uintptr_t image_base,
                                       const char* image_name) {
  RawSyscallWatch watch;
  watch.functions = functions.size();
  for (const RawSyscallFunction& function : functions) watch.syscalls += function.syscalls;
  if (functions.empty()) return watch;
  const bool enabled = RawSyscallTrapEnabled();
  {
    std::lock_guard<std::mutex> lock(g_writers);
    if (g_zones == nullptr) g_zones = new std::vector<Zone*>();
    std::vector<const Zone*> added;
    for (RawSyscallFunction& function : functions) {
      auto* zone = new Zone();
      zone->function = std::move(function);
      zone->image_base = image_base;
      zone->image = image_name != nullptr ? image_name : "";
      zone->idle_stop = IdleStop(zone->function);
      g_zones->push_back(zone);
      added.push_back(zone);
    }
    g_known.store(g_zones->size(), std::memory_order_relaxed);
    if (enabled) {
      if (g_handle == nullptr) g_handle = AddVectoredExceptionHandler(1, TrapHandler);
      if (g_handle != nullptr) PublishLocked();
    }
    const Snapshot* snapshot = g_snapshot.load(std::memory_order_acquire);
    for (std::size_t z = 0; z < snapshot->count; ++z) {
      if (std::find(added.begin(), added.end(), snapshot->zones[z]) == added.end()) continue;
      ++watch.watched_functions;
      watch.watched_syscalls += snapshot->zones[z]->function.syscalls;
    }
  }
  ArmRawSyscallsOnThisThread();
  return watch;
}

void RemoveRawSyscallFunctions(std::uintptr_t start, std::uintptr_t end) noexcept {
  std::lock_guard<std::mutex> lock(g_writers);
  if (g_zones == nullptr) return;
  const auto gone = std::remove_if(g_zones->begin(), g_zones->end(), [start, end](const Zone* zone) {
    return zone->function.start < end && start < zone->function.end;
  });
  if (gone == g_zones->end()) return;
  g_zones->erase(gone, g_zones->end());
  g_known.store(g_zones->size(), std::memory_order_relaxed);
  if (g_snapshot.load(std::memory_order_acquire) != &kNothing) PublishLocked();
}

void ArmRawSyscallsOnThisThread() noexcept {
  const Snapshot* snapshot = g_snapshot.load(std::memory_order_acquire);
  ThreadTrap& t = t_trap;
  if (t.generation == snapshot->generation) return;
  CONTEXT context;
  std::memset(&context, 0, sizeof(context));
  WriteRegisters(t, *snapshot, &context);
  context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  if (!SetThreadContext(GetCurrentThread(), &context)) t.generation = kNeverArmed;
}

void ApplyRawSyscallBreakpoints(void* context, bool always) noexcept {
  const Snapshot* snapshot = g_snapshot.load(std::memory_order_acquire);
  ThreadTrap& t = t_trap;
  if (snapshot->generation == 0) return;
  if (!always && t.generation == snapshot->generation) return;
  WriteRegisters(t, *snapshot, static_cast<CONTEXT*>(context));
}

RawSyscallStats GetRawSyscallStats() noexcept {
  RawSyscallStats stats;
  stats.handled = g_handled.load(std::memory_order_relaxed);
  stats.stops = g_stops.load(std::memory_order_relaxed);
  stats.traced = g_traced.load(std::memory_order_relaxed);
  stats.unwatched = g_unwatched.load(std::memory_order_relaxed);
  stats.watched_functions = g_snapshot.load(std::memory_order_acquire)->count;
  stats.known_functions = g_known.load(std::memory_order_relaxed);
  return stats;
}

}
