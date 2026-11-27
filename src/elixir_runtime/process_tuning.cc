// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/process_tuning.h"

#include <windows.h>
#include <avrt.h>
#include <timeapi.h>

#include <atomic>
#include <utility>

namespace elixir::runtime {
namespace {

std::atomic<int> g_timer_requests{0};
std::atomic<bool> g_mmcss_enabled{true};

constexpr ULONG kIgnoreTimerResolution = 0x4;
constexpr unsigned long kOneMillisecond = 10000;

using SetTimerResolutionFn = LONG(NTAPI*)(ULONG, BOOLEAN, PULONG);
using QueryTimerResolutionFn = LONG(NTAPI*)(PULONG, PULONG, PULONG);

template <typename Function>
Function NtdllFunction(const char* name) {
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll == nullptr) return nullptr;
  return reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(ntdll, name)));
}

std::string WindowsError(DWORD code) { return "Windows error " + std::to_string(code); }

}

TuningOutcome ApplyPriorityClass(PriorityRequest request) {
  TuningOutcome outcome;
  DWORD priority = 0;
  const char* name = nullptr;
  switch (request) {
    case PriorityRequest::kUnchanged:
      outcome.detail = "priority class left as started (" + CurrentPriorityClassName() + ")";
      return outcome;
    case PriorityRequest::kNormal:
      priority = NORMAL_PRIORITY_CLASS;
      name = "normal";
      break;
    case PriorityRequest::kAboveNormal:
      priority = ABOVE_NORMAL_PRIORITY_CLASS;
      name = "above-normal";
      break;
    case PriorityRequest::kHigh:
      priority = HIGH_PRIORITY_CLASS;
      name = "high";
      break;
  }
  if (!SetPriorityClass(GetCurrentProcess(), priority)) {
    outcome.detail = std::string("SetPriorityClass(") + name + ") failed: " + WindowsError(GetLastError());
    return outcome;
  }
  outcome.applied = GetPriorityClass(GetCurrentProcess()) == priority;
  outcome.detail = std::string("priority class ") + name + (outcome.applied ? "" : " not confirmed");
  return outcome;
}

std::string CurrentPriorityClassName() {
  switch (GetPriorityClass(GetCurrentProcess())) {
    case IDLE_PRIORITY_CLASS: return "idle";
    case BELOW_NORMAL_PRIORITY_CLASS: return "below-normal";
    case NORMAL_PRIORITY_CLASS: return "normal";
    case ABOVE_NORMAL_PRIORITY_CLASS: return "above-normal";
    case HIGH_PRIORITY_CLASS: return "high";
    case REALTIME_PRIORITY_CLASS: return "realtime";
    default: return "unknown";
  }
}

TuningOutcome SetPowerThrottlingOptOut(bool opt_out) {
  TuningOutcome outcome;
  PROCESS_POWER_THROTTLING_STATE state{};
  state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  if (opt_out) {
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | kIgnoreTimerResolution;
    state.StateMask = 0;
    if (SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state))) {
      outcome.applied = true;
      outcome.detail = "opted out of power throttling (execution speed and timer resolution)";
      return outcome;
    }
    const DWORD first_error = GetLastError();
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    if (SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state))) {
      outcome.applied = true;
      outcome.detail = "opted out of power throttling (execution speed; timer-resolution control unavailable)";
      return outcome;
    }
    outcome.detail = "SetProcessInformation(ProcessPowerThrottling) failed: " + WindowsError(first_error);
    return outcome;
  }
  state.ControlMask = 0;
  state.StateMask = 0;
  outcome.applied = SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state));
  outcome.detail = outcome.applied ? "power throttling left to Windows"
                                   : "SetProcessInformation(ProcessPowerThrottling) failed: " +
                                         WindowsError(GetLastError());
  return outcome;
}

std::string CurrentPowerThrottlingState() {
  PROCESS_POWER_THROTTLING_STATE state{};
  state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  if (!GetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state))) {
    return "unknown";
  }
  if ((state.ControlMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) == 0) return "system";
  return (state.StateMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) != 0 ? "on" : "off";
}

TimerResolutionScope::TimerResolutionScope(bool enable) {
  if (!enable || timeBeginPeriod(1) != TIMERR_NOERROR) return;
  active_ = true;
  resolution_ = kOneMillisecond;
  g_timer_requests.fetch_add(1);
  const auto query = NtdllFunction<QueryTimerResolutionFn>("NtQueryTimerResolution");
  const auto set = NtdllFunction<SetTimerResolutionFn>("NtSetTimerResolution");
  ULONG coarsest = 0, finest = 0, current = 0;
  if (query == nullptr || set == nullptr || query(&coarsest, &finest, &current) < 0) return;
  if (finest == 0 || finest >= kOneMillisecond) return;
  ULONG actual = 0;
  if (set(finest, TRUE, &actual) < 0) return;
  finest_ = finest;
  resolution_ = finest;
}

TimerResolutionScope::~TimerResolutionScope() { Release(); }

void TimerResolutionScope::Release() {
  if (!active_) return;
  if (finest_ != 0) {
    if (const auto set = NtdllFunction<SetTimerResolutionFn>("NtSetTimerResolution")) {
      ULONG actual = 0;
      set(finest_, FALSE, &actual);
    }
    finest_ = 0;
  }
  timeEndPeriod(1);
  active_ = false;
  resolution_ = 0;
  g_timer_requests.fetch_sub(1);
}

int ActiveTimerResolutionRequests() { return g_timer_requests.load(); }

std::string TimerResolutionText(unsigned long resolution_100ns) {
  std::string text = std::to_string(resolution_100ns / kOneMillisecond);
  const unsigned long fraction = resolution_100ns % kOneMillisecond;
  if (fraction != 0) {
    std::string digits = std::to_string(fraction);
    digits.insert(0, 4 - digits.size(), '0');
    while (digits.back() == '0') digits.pop_back();
    text += "." + digits;
  }
  return text + " ms";
}

const wchar_t* MmcssTaskName(MmcssTask task) { return task == MmcssTask::kGames ? L"Games" : L"Pro Audio"; }

void SetMmcssEnabled(bool enabled) { g_mmcss_enabled.store(enabled); }
bool MmcssEnabled() { return g_mmcss_enabled.load(); }

MmcssThreadRegistration::MmcssThreadRegistration(MmcssThreadRegistration&& other) noexcept {
  *this = std::move(other);
}

MmcssThreadRegistration& MmcssThreadRegistration::operator=(MmcssThreadRegistration&& other) noexcept {
  if (this != &other) {
    Revert();
    handle_ = other.handle_;
    task_index_ = other.task_index_;
    thread_id_ = other.thread_id_;
    detail_ = std::move(other.detail_);
    other.handle_ = nullptr;
    other.task_index_ = 0;
  }
  return *this;
}

MmcssThreadRegistration::~MmcssThreadRegistration() { Revert(); }

void MmcssThreadRegistration::Revert() {
  if (handle_ == nullptr) return;
  if (thread_id_ == GetCurrentThreadId()) AvRevertMmThreadCharacteristics(static_cast<HANDLE>(handle_));
  handle_ = nullptr;
  task_index_ = 0;
}

MmcssThreadRegistration RegisterCurrentThreadWithMmcss(MmcssTask task) {
  MmcssThreadRegistration registration;
  if (!MmcssEnabled()) {
    registration.detail_ = "MMCSS disabled (--mmcss off)";
    return registration;
  }
  DWORD index = 0;
  const HANDLE handle = AvSetMmThreadCharacteristicsW(MmcssTaskName(task), &index);
  if (handle == nullptr) {
    registration.detail_ = "AvSetMmThreadCharacteristicsW failed: " + WindowsError(GetLastError());
    return registration;
  }
  registration.handle_ = handle;
  registration.task_index_ = index;
  registration.thread_id_ = GetCurrentThreadId();
  registration.detail_ = task == MmcssTask::kGames ? "MMCSS Games" : "MMCSS Pro Audio";
  return registration;
}

TuningOutcome EnsureThreadStackGuarantee(unsigned long bytes, unsigned long* effective) {
  TuningOutcome outcome;
  ULONG size = bytes;
  if (!SetThreadStackGuarantee(&size)) {
    outcome.detail = "SetThreadStackGuarantee failed: " + WindowsError(GetLastError());
    return outcome;
  }
  ULONG current = 0;
  SetThreadStackGuarantee(&current);
  if (effective != nullptr) *effective = current;
  outcome.applied = current >= bytes;
  outcome.detail = "stack guarantee " + std::to_string(current) + " bytes";
  return outcome;
}

}
