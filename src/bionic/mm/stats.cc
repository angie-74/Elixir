// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <atomic>
#include <cstdio>
#include <string>

#include "bionic/mm/mm_internal.h"
#include "bionic/wake_trace.h"

namespace elixir::bionic::mm {
namespace {

struct OpCounters {
  std::atomic<std::uint64_t> calls{0};
  std::atomic<std::uint64_t> total_ns{0};
  std::atomic<std::uint64_t> longest_ns{0};
};

OpCounters g_ops[static_cast<int>(MmOp::kCount)];
std::atomic<std::uint64_t> g_dontneed{0};
std::atomic<std::uint64_t> g_dontneed_fast{0};

constexpr const char* kNames[] = {"mmap", "munmap", "mprotect", "madvise"};

}

void NoteMmCall(MmOp op, std::int64_t ns) noexcept {
  OpCounters& counters = g_ops[static_cast<int>(op)];
  const std::uint64_t value = ns > 0 ? static_cast<std::uint64_t>(ns) : 0;
  counters.calls.fetch_add(1, std::memory_order_relaxed);
  counters.total_ns.fetch_add(value, std::memory_order_relaxed);
  std::uint64_t longest = counters.longest_ns.load(std::memory_order_relaxed);
  while (value > longest &&
         !counters.longest_ns.compare_exchange_weak(longest, value, std::memory_order_relaxed)) {
  }
}

void NoteDontNeed(bool fast) noexcept {
  g_dontneed.fetch_add(1, std::memory_order_relaxed);
  if (fast) g_dontneed_fast.fetch_add(1, std::memory_order_relaxed);
}

MmTimer::MmTimer(MmOp op) noexcept : op_(op), start_ns_(wake_trace::NowNs()) {}
MmTimer::~MmTimer() { NoteMmCall(op_, wake_trace::NowNs() - start_ns_); }

}

namespace elixir::bionic {

std::string MmStatisticsReport() {
  using mm::g_ops;
  std::string report;
  for (int i = 0; i < static_cast<int>(mm::MmOp::kCount); ++i) {
    const std::uint64_t calls = g_ops[i].calls.exchange(0, std::memory_order_relaxed);
    const std::uint64_t total = g_ops[i].total_ns.exchange(0, std::memory_order_relaxed);
    const std::uint64_t longest = g_ops[i].longest_ns.exchange(0, std::memory_order_relaxed);
    if (calls == 0) continue;
    char text[160];
    std::snprintf(text, sizeof(text), "%s%s %llu calls, %.1f ms in all, longest %.2f ms", report.empty() ? "" : "; ",
                  mm::kNames[i], static_cast<unsigned long long>(calls), static_cast<double>(total) / 1e6,
                  static_cast<double>(longest) / 1e6);
    report += text;
    if (static_cast<mm::MmOp>(i) == mm::MmOp::kMadvise) {
      const std::uint64_t dontneed = mm::g_dontneed.exchange(0, std::memory_order_relaxed);
      const std::uint64_t fast = mm::g_dontneed_fast.exchange(0, std::memory_order_relaxed);
      if (dontneed > 0) {
        std::snprintf(text, sizeof(text), " (MADV_DONTNEED %llu, %llu of them in one pass)",
                      static_cast<unsigned long long>(dontneed), static_cast<unsigned long long>(fast));
        report += text;
      }
    }
  }
  return report.empty() ? report : "mm: " + report;
}

}
