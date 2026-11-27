// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_PROCESS_TUNING_H_
#define ELIXIR_RUNTIME_PROCESS_TUNING_H_

#include <cstdint>
#include <string>

#include "elixir_runtime/cli.h"

namespace elixir::runtime {

struct TuningOutcome {
  bool applied = false;
  std::string detail;
};

TuningOutcome ApplyPriorityClass(PriorityRequest request);
std::string CurrentPriorityClassName();

TuningOutcome SetPowerThrottlingOptOut(bool opt_out);
std::string CurrentPowerThrottlingState();

class TimerResolutionScope {
 public:
  explicit TimerResolutionScope(bool enable);
  ~TimerResolutionScope();
  TimerResolutionScope(const TimerResolutionScope&) = delete;
  TimerResolutionScope& operator=(const TimerResolutionScope&) = delete;

  bool active() const { return active_; }
  unsigned long resolution_100ns() const { return resolution_; }
  void Release();

 private:
  bool active_ = false;
  unsigned long resolution_ = 0;
  unsigned long finest_ = 0;
};

int ActiveTimerResolutionRequests();
std::string TimerResolutionText(unsigned long resolution_100ns);

enum class MmcssTask {
  kGames,
  kProAudio,
};
const wchar_t* MmcssTaskName(MmcssTask task);

void SetMmcssEnabled(bool enabled);
bool MmcssEnabled();

class MmcssThreadRegistration {
 public:
  MmcssThreadRegistration() = default;
  MmcssThreadRegistration(MmcssThreadRegistration&& other) noexcept;
  MmcssThreadRegistration& operator=(MmcssThreadRegistration&& other) noexcept;
  ~MmcssThreadRegistration();
  MmcssThreadRegistration(const MmcssThreadRegistration&) = delete;
  MmcssThreadRegistration& operator=(const MmcssThreadRegistration&) = delete;

  bool registered() const { return handle_ != nullptr; }
  unsigned long task_index() const { return task_index_; }
  const std::string& detail() const { return detail_; }
  void Revert();

 private:
  friend MmcssThreadRegistration RegisterCurrentThreadWithMmcss(MmcssTask task);
  void* handle_ = nullptr;
  unsigned long task_index_ = 0;
  std::uint32_t thread_id_ = 0;
  std::string detail_;
};

MmcssThreadRegistration RegisterCurrentThreadWithMmcss(MmcssTask task);

TuningOutcome EnsureThreadStackGuarantee(unsigned long bytes, unsigned long* effective = nullptr);

}

#endif
