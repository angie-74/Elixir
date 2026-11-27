// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_STALL_PROFILER_H_
#define ELIXIR_RUNTIME_STALL_PROFILER_H_

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace elixir::runtime {

class StallProfiler final {
 public:
  StallProfiler(const std::atomic<std::int64_t>* last_present_us, const std::atomic<std::int64_t>* average_gap_us,
                const std::atomic<DWORD>* present_thread, std::int64_t (*now_us)());
  ~StallProfiler();
  StallProfiler(const StallProfiler&) = delete;
  StallProfiler& operator=(const StallProfiler&) = delete;

  void Start();
  void Stop();

  std::vector<std::string> Report();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

bool StallProfilerCanReadUnwindData(std::uintptr_t address) noexcept;

}

#endif
