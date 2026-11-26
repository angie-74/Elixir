// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_WAKE_TRACE_H_
#define ELIXIR_BIONIC_WAKE_TRACE_H_

#include <atomic>
#include <cstdint>
#include <vector>

namespace elixir::bionic::wake_trace {

enum class Event : std::uint8_t {
  kWoken = 1,
  kTimedOut = 2,
  kWake = 3,
  kIoReady = 4,
  kSlept = 5,
};

inline constexpr std::int64_t kRecordWaitsFromNs = 500000;

namespace internal {
extern std::atomic<bool> g_enabled;
}

inline bool Enabled() noexcept { return internal::g_enabled.load(std::memory_order_relaxed); }

void Enable(bool on) noexcept;

std::int64_t NowNs() noexcept;

std::uintptr_t GuestCallSite() noexcept;

void Record(Event event, const volatile void* address, std::uintptr_t site, std::int64_t waited_ns) noexcept;

inline void RecordWaitEnd(Event event, const volatile void* address, std::int64_t started_ns) noexcept {
  if (!Enabled()) return;
  const std::int64_t waited = NowNs() - started_ns;
  if (waited >= kRecordWaitsFromNs) Record(event, address, GuestCallSite(), waited);
}

inline void RecordWake(const volatile void* address) noexcept {
  if (Enabled()) Record(Event::kWake, address, GuestCallSite(), 0);
}

struct Step {
  std::uint32_t thread = 0;
  bool found = false;
  Event ended_by = Event::kWoken;
  std::uintptr_t wait_site = 0;
  std::int64_t waited_ns = 0;
  std::int64_t ended_ns = 0;
  std::int64_t ran_ns = 0;
  std::uintptr_t wake_site = 0;
};

std::vector<Step> Chain(std::uint32_t thread, std::int64_t from_ns, std::int64_t to_ns, int depth);

void ResetForTest() noexcept;

}

#endif
