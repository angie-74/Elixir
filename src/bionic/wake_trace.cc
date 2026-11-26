// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/wake_trace.h"

#include <windows.h>

#include <algorithm>
#include <new>

namespace elixir::bionic::wake_trace {
namespace internal {
std::atomic<bool> g_enabled{false};
}

namespace {

constexpr std::uint64_t kCapacity = std::uint64_t{1} << 17;
constexpr std::int64_t kLookBackNs = 2000000000;
constexpr std::int64_t kSignificantWaitNs = 1000000;
constexpr std::int64_t kWakeSlackNs = 1000000;
constexpr std::int64_t kReorderNs = 50000000;

struct Entry {
  std::atomic<std::uint64_t> sequence{0};
  std::atomic<std::int64_t> time_ns{0};
  std::atomic<std::uintptr_t> address{0};
  std::atomic<std::uintptr_t> site{0};
  std::atomic<std::int64_t> waited_ns{0};
  std::atomic<std::uint32_t> thread{0};
  std::atomic<std::uint8_t> event{0};
};

std::atomic<Entry*> g_ring{nullptr};
std::atomic<std::uint64_t> g_next{0};

struct Copy {
  std::int64_t time_ns = 0;
  std::uintptr_t address = 0;
  std::uintptr_t site = 0;
  std::int64_t waited_ns = 0;
  std::uint32_t thread = 0;
  Event event = Event::kWoken;
};

bool IsWaitEnd(Event event) { return event != Event::kWake; }

std::uintptr_t ThisModuleBase() {
  static const std::uintptr_t base = [] {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ThisModuleBase), &module);
    return reinterpret_cast<std::uintptr_t>(module);
  }();
  return base;
}

std::vector<Copy> Snapshot(std::int64_t from_ns, std::int64_t to_ns) {
  std::vector<Copy> events;
  Entry* ring = g_ring.load(std::memory_order_acquire);
  if (ring == nullptr) return events;
  const std::uint64_t end = g_next.load(std::memory_order_acquire);
  const std::uint64_t begin = end > kCapacity ? end - kCapacity : 0;
  for (std::uint64_t index = end; index-- > begin;) {
    Entry& entry = ring[index & (kCapacity - 1)];
    if (entry.sequence.load(std::memory_order_acquire) != index + 1) continue;
    Copy copy;
    copy.time_ns = entry.time_ns.load(std::memory_order_relaxed);
    copy.address = entry.address.load(std::memory_order_relaxed);
    copy.site = entry.site.load(std::memory_order_relaxed);
    copy.waited_ns = entry.waited_ns.load(std::memory_order_relaxed);
    copy.thread = entry.thread.load(std::memory_order_relaxed);
    copy.event = static_cast<Event>(entry.event.load(std::memory_order_relaxed));
    std::atomic_thread_fence(std::memory_order_acquire);
    if (entry.sequence.load(std::memory_order_relaxed) != index + 1) continue;
    if (copy.time_ns < from_ns - kReorderNs) break;
    if (copy.time_ns < from_ns || copy.time_ns > to_ns) continue;
    events.push_back(copy);
  }
  std::sort(events.begin(), events.end(), [](const Copy& a, const Copy& b) { return a.time_ns < b.time_ns; });
  return events;
}

void Fill(Step* step, const Copy& wait) {
  step->found = true;
  step->ended_by = wait.event;
  step->wait_site = wait.site;
  step->waited_ns = wait.waited_ns;
  step->ended_ns = wait.time_ns;
}

}

void Enable(bool on) noexcept {
  if (on && g_ring.load(std::memory_order_acquire) == nullptr) {
    Entry* ring = new (std::nothrow) Entry[kCapacity];
    if (ring == nullptr) return;
    Entry* expected = nullptr;
    if (!g_ring.compare_exchange_strong(expected, ring, std::memory_order_acq_rel)) delete[] ring;
  }
  internal::g_enabled.store(on, std::memory_order_relaxed);
}

std::int64_t NowNs() noexcept {
  static const std::int64_t frequency = [] {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    return static_cast<std::int64_t>(value.QuadPart);
  }();
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  const std::int64_t ticks = counter.QuadPart;
  return ticks / frequency * 1000000000 + ticks % frequency * 1000000000 / frequency;
}

__declspec(noinline) std::uintptr_t GuestCallSite() noexcept {
  CONTEXT context;
  RtlCaptureContext(&context);
  const std::uintptr_t self = ThisModuleBase();
  for (int frame = 0; frame < 16; ++frame) {
    DWORD64 image_base = 0;
    PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
    if (function == nullptr || image_base != self) return static_cast<std::uintptr_t>(context.Rip);
    void* handler_data = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, function, &context, &handler_data, &establisher,
                     nullptr);
    if (context.Rip == 0) return 0;
  }
  return 0;
}

void Record(Event event, const volatile void* address, std::uintptr_t site, std::int64_t waited_ns) noexcept {
  if (!Enabled()) return;
  Entry* ring = g_ring.load(std::memory_order_acquire);
  if (ring == nullptr) return;
  const std::uint64_t index = g_next.fetch_add(1, std::memory_order_relaxed);
  Entry& entry = ring[index & (kCapacity - 1)];
  entry.sequence.store(0, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  entry.time_ns.store(NowNs(), std::memory_order_relaxed);
  entry.address.store(reinterpret_cast<std::uintptr_t>(address), std::memory_order_relaxed);
  entry.site.store(site, std::memory_order_relaxed);
  entry.waited_ns.store(waited_ns, std::memory_order_relaxed);
  entry.thread.store(GetCurrentThreadId(), std::memory_order_relaxed);
  entry.event.store(static_cast<std::uint8_t>(event), std::memory_order_relaxed);
  entry.sequence.store(index + 1, std::memory_order_release);
}

std::vector<Step> Chain(std::uint32_t thread, std::int64_t from_ns, std::int64_t to_ns, int depth) {
  std::vector<Step> chain;
  const std::vector<Copy> events = Snapshot(from_ns - kLookBackNs, to_ns);
  Step first;
  first.thread = thread;
  const Copy* current = nullptr;
  for (const Copy& event : events) {
    if (event.thread != thread || !IsWaitEnd(event.event) || event.time_ns < from_ns) continue;
    if (current == nullptr || event.waited_ns > current->waited_ns) current = &event;
  }
  if (current != nullptr) Fill(&first, *current);
  chain.push_back(first);
  while (current != nullptr && current->event == Event::kWoken && static_cast<int>(chain.size()) < depth) {
    const std::int64_t began = current->time_ns - current->waited_ns - kWakeSlackNs;
    const Copy* wake = nullptr;
    for (const Copy& event : events) {
      if (event.time_ns > current->time_ns) break;
      if (event.event == Event::kWake && event.address == current->address && event.thread != current->thread &&
          event.time_ns >= began) {
        wake = &event;
      }
    }
    if (wake == nullptr) break;
    const Copy* before = nullptr;
    for (const Copy& event : events) {
      if (event.time_ns > wake->time_ns) break;
      if (event.thread == wake->thread && IsWaitEnd(event.event) && event.waited_ns >= kSignificantWaitNs) {
        before = &event;
      }
    }
    Step next;
    next.thread = wake->thread;
    next.wake_site = wake->site;
    if (before != nullptr) {
      Fill(&next, *before);
      next.ran_ns = wake->time_ns - before->time_ns;
    }
    chain.push_back(next);
    current = before;
  }
  return chain;
}

void ResetForTest() noexcept {
  internal::g_enabled.store(false, std::memory_order_relaxed);
  Entry* ring = g_ring.load(std::memory_order_acquire);
  if (ring != nullptr) {
    for (std::uint64_t i = 0; i < kCapacity; ++i) ring[i].sequence.store(0, std::memory_order_relaxed);
  }
  g_next.store(0, std::memory_order_relaxed);
}

}
