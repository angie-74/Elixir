// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <dwmapi.h>
#include <dxgi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "android/android_internal.h"
#include "bionic/runtime.h"

struct GuestFrameCallbackData;

namespace {

using FrameCallbackFn = elixir::abi::SysvFn<void(std::int64_t, void*)>;
using VsyncCallbackFn = elixir::abi::SysvFn<void(const GuestFrameCallbackData*, void*)>;
using RefreshRateCallbackFn = elixir::abi::SysvFn<void(std::int64_t, void*)>;

constexpr std::int64_t kDefaultPeriodNs = 16666667;

enum class CallbackKind { kFrame, kVsync };

struct PendingCallback {
  CallbackKind kind;
  void* function;
  void* data;
  std::int64_t due_ns;
};

struct RateCallback {
  RefreshRateCallbackFn function;
  void* data;
  bool needs_call;
};

}

struct GuestFrameCallbackData {
  std::int64_t frame_time_ns;
  std::int64_t vsync_id;
  std::int64_t expected_present_ns;
  std::int64_t deadline_ns;
};

struct AChoreographer {
  ALooper* looper = nullptr;
  elixir::android::looper::HostSourceId source = 0;
  std::mutex mutex;
  std::vector<PendingCallback> callbacks;
  bool frame_ready = false;
  std::int64_t frame_time_ns = 0;
  std::int64_t frame_vsync_id = 0;
  std::int64_t frame_period_ns = kDefaultPeriodNs;
  std::vector<RateCallback> rate_callbacks;
  std::int64_t rate_period_ns = 0;
};

namespace elixir::android {
namespace {

HMONITOR TargetMonitor() noexcept {
  HWND hwnd = nullptr;
  if (PrimaryWindowGeometry(&hwnd, nullptr, nullptr) && hwnd != nullptr) {
    if (HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY); monitor != nullptr) return monitor;
  }
  return MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

std::int64_t RefreshPeriodNs(HMONITOR monitor) noexcept {
  MONITORINFOEXW info{};
  info.cbSize = sizeof(info);
  const bool named = monitor != nullptr && GetMonitorInfoW(monitor, &info);
  if (named) {
    UINT32 path_count = 0;
    UINT32 mode_count = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) == ERROR_SUCCESS) {
      try {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(),
                               nullptr) == ERROR_SUCCESS) {
          for (UINT32 i = 0; i < path_count; ++i) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size = sizeof(source);
            source.header.adapterId = paths[i].sourceInfo.adapterId;
            source.header.id = paths[i].sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
                wcscmp(source.viewGdiDeviceName, info.szDevice) != 0) {
              continue;
            }
            const DISPLAYCONFIG_RATIONAL rate = paths[i].targetInfo.refreshRate;
            if (rate.Numerator != 0 && rate.Denominator != 0) {
              const std::int64_t ns = static_cast<std::int64_t>(rate.Denominator) * 1000000000 / rate.Numerator;
              if (ns > 1000000 && ns < 1000000000) return ns;
            }
          }
        }
      } catch (...) {
      }
    }
  }
  DWM_TIMING_INFO timing{};
  timing.cbSize = sizeof(timing);
  if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) && timing.qpcRefreshPeriod > 0) {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    const std::int64_t ns =
        static_cast<std::int64_t>(timing.qpcRefreshPeriod) * 1000000000 / static_cast<std::int64_t>(frequency.QuadPart);
    if (ns > 1000000 && ns < 1000000000) return ns;
  }
  DEVMODEW mode{};
  mode.dmSize = sizeof(mode);
  if (EnumDisplaySettingsW(named ? info.szDevice : nullptr, ENUM_CURRENT_SETTINGS, &mode) &&
      mode.dmDisplayFrequency > 1) {
    return 1000000000LL / mode.dmDisplayFrequency;
  }
  return kDefaultPeriodNs;
}

class VsyncHub {
 public:
  static VsyncHub& Get() {
    static VsyncHub* hub = new VsyncHub();
    return *hub;
  }

  void Register(AChoreographer* choreographer) {
    std::lock_guard<std::mutex> lock(mutex_);
    choreographers_.push_back(choreographer);
  }

  void Unregister(AChoreographer* choreographer) {
    std::lock_guard<std::mutex> lock(mutex_);
    choreographers_.erase(std::remove(choreographers_.begin(), choreographers_.end(), choreographer),
                          choreographers_.end());
  }

  void RequestVsync() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!thread_started_) {
      thread_started_ = true;
      period_ns_.store(RefreshPeriodNs(TargetMonitor()), std::memory_order_relaxed);
      try {
        std::thread(&VsyncHub::Run, this).detach();
      } catch (...) {
        thread_started_ = false;
        Log(kLogError, "cannot start the vsync thread");
        return;
      }
    }
    requested_ = true;
    wake_.notify_one();
  }

  std::int64_t PeriodNs() {
    std::int64_t period = period_ns_.load(std::memory_order_relaxed);
    if (period == 0) {
      period = RefreshPeriodNs(TargetMonitor());
      period_ns_.store(period, std::memory_order_relaxed);
    }
    return period;
  }

  VsyncInfo Info() {
    VsyncInfo info;
    info.period_ns = PeriodNs();
    info.source = source_.load(std::memory_order_acquire);
    info.vsync_count = count_.load(std::memory_order_relaxed);
    return info;
  }

 private:
  VsyncHub() = default;

  std::int64_t EarliestDueLocked() {
    std::int64_t earliest = INT64_MAX;
    for (AChoreographer* choreographer : choreographers_) {
      std::lock_guard<std::mutex> lock(choreographer->mutex);
      for (const PendingCallback& callback : choreographer->callbacks) earliest = std::min(earliest, callback.due_ns);
    }
    return earliest;
  }

  bool WaitUntilDueLocked(std::unique_lock<std::mutex>& lock) {
    bool waited = false;
    for (;;) {
      requested_ = false;
      const std::int64_t due = EarliestDueLocked();
      if (due == INT64_MAX) {
        waited = true;
        wake_.wait(lock, [this] { return requested_; });
        continue;
      }
      const std::int64_t lead = due - MonotonicNs() - PeriodNs();
      if (lead <= 0) return waited;
      waited = true;
      const std::int64_t wait_ns = std::min<std::int64_t>(lead, 1000000000);
      wake_.wait_for(lock, std::chrono::nanoseconds(wait_ns), [this] { return requested_; });
    }
  }

  static IDXGIOutput* OpenOutput(HMONITOR target) {
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return nullptr;
    IDXGIOutput* found = nullptr;
    IDXGIOutput* first = nullptr;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT a = 0; found == nullptr && factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
      IDXGIOutput* output = nullptr;
      for (UINT o = 0; found == nullptr && adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o) {
        DXGI_OUTPUT_DESC desc{};
        if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == target) {
          found = output;
        } else if (first == nullptr) {
          first = output;
        } else {
          output->Release();
        }
      }
      adapter->Release();
    }
    factory->Release();
    if (found != nullptr) {
      if (first != nullptr) first->Release();
      return found;
    }
    return first;
  }

  void Run() {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer == nullptr) timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    HMONITOR monitor = TargetMonitor();
    IDXGIOutput* output = OpenOutput(monitor);
    source_.store(output != nullptr ? "dxgi-waitforvblank" : "waitable-timer", std::memory_order_release);
    period_ns_.store(RefreshPeriodNs(monitor), std::memory_order_relaxed);
    std::int64_t last_ns = MonotonicNs();
    std::int64_t vsync_id = 0;
    std::uint64_t ticks_since_check = 0;
    for (;;) {
      bool was_idle = false;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        was_idle = WaitUntilDueLocked(lock);
      }
      if (was_idle) last_ns = MonotonicNs() - period_ns_.load(std::memory_order_relaxed);
      if (was_idle || ++ticks_since_check >= 120) {
        ticks_since_check = 0;
        const HMONITOR target = TargetMonitor();
        if (target != monitor) {
          if (output != nullptr) output->Release();
          monitor = target;
          output = OpenOutput(monitor);
          source_.store(output != nullptr ? "dxgi-waitforvblank" : "waitable-timer", std::memory_order_release);
        }
        period_ns_.store(RefreshPeriodNs(monitor), std::memory_order_relaxed);
      }
      const std::int64_t period = PeriodNs();
      bool waited = false;
      if (output != nullptr) {
        waited = SUCCEEDED(output->WaitForVBlank());
        if (waited && MonotonicNs() - last_ns < period / 2) waited = false;
      }
      if (!waited) {
        const std::int64_t next = last_ns + period;
        const std::int64_t wait_ns = next - MonotonicNs();
        if (wait_ns > 0 && timer != nullptr) {
          LARGE_INTEGER due;
          due.QuadPart = -(wait_ns / 100);
          if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, INFINITE);
        } else if (wait_ns > 0) {
          Sleep(static_cast<DWORD>(wait_ns / 1000000));
        }
      }
      const std::int64_t now = MonotonicNs();
      last_ns = now;
      ++vsync_id;
      count_.fetch_add(1, std::memory_order_relaxed);
      std::lock_guard<std::mutex> lock(mutex_);
      for (AChoreographer* choreographer : choreographers_) Deliver(choreographer, now, vsync_id, period);
    }
  }

  static void Deliver(AChoreographer* choreographer, std::int64_t now, std::int64_t vsync_id, std::int64_t period) {
    bool signal = false;
    {
      std::lock_guard<std::mutex> lock(choreographer->mutex);
      for (const PendingCallback& callback : choreographer->callbacks) {
        if (callback.due_ns <= now) {
          signal = true;
          break;
        }
      }
      if (signal) {
        choreographer->frame_ready = true;
        choreographer->frame_time_ns = now;
        choreographer->frame_vsync_id = vsync_id;
        choreographer->frame_period_ns = period;
      }
      if (choreographer->rate_period_ns != 0 && choreographer->rate_period_ns != period) {
        for (RateCallback& rate : choreographer->rate_callbacks) rate.needs_call = true;
        signal = signal || !choreographer->rate_callbacks.empty();
      }
      choreographer->rate_period_ns = period;
    }
    if (signal) looper::SignalLooper(choreographer->looper);
  }

  std::mutex mutex_;
  std::condition_variable wake_;
  std::vector<AChoreographer*> choreographers_;
  bool thread_started_ = false;
  bool requested_ = false;
  std::atomic<std::int64_t> period_ns_{0};
  std::atomic<const char*> source_{""};
  std::atomic<std::uint64_t> count_{0};
};

bool ChoreographerReady(void* context) noexcept {
  auto* choreographer = static_cast<AChoreographer*>(context);
  std::lock_guard<std::mutex> lock(choreographer->mutex);
  if (choreographer->frame_ready) return true;
  for (const RateCallback& rate : choreographer->rate_callbacks) {
    if (rate.needs_call) return true;
  }
  return false;
}

void ChoreographerDispatch(void* context) noexcept {
  auto* choreographer = static_cast<AChoreographer*>(context);
  std::vector<PendingCallback> due;
  std::vector<RateCallback> rates;
  GuestFrameCallbackData data{};
  std::int64_t period = kDefaultPeriodNs;
  try {
    std::lock_guard<std::mutex> lock(choreographer->mutex);
    if (choreographer->frame_ready) {
      choreographer->frame_ready = false;
      const std::int64_t frame_time = choreographer->frame_time_ns;
      auto& callbacks = choreographer->callbacks;
      for (auto it = callbacks.begin(); it != callbacks.end();) {
        if (it->due_ns <= frame_time) {
          due.push_back(*it);
          it = callbacks.erase(it);
        } else {
          ++it;
        }
      }
      data.frame_time_ns = frame_time;
      data.vsync_id = choreographer->frame_vsync_id;
      data.expected_present_ns = frame_time + choreographer->frame_period_ns;
      data.deadline_ns = frame_time + choreographer->frame_period_ns;
    }
    period = choreographer->rate_period_ns != 0 ? choreographer->rate_period_ns : VsyncHub::Get().PeriodNs();
    for (RateCallback& rate : choreographer->rate_callbacks) {
      if (rate.needs_call) {
        rates.push_back(rate);
        rate.needs_call = false;
      }
    }
  } catch (...) {
    return;
  }
  for (const PendingCallback& callback : due) {
    if (callback.kind == CallbackKind::kVsync) {
      reinterpret_cast<VsyncCallbackFn>(callback.function)(&data, callback.data);
    } else {
      reinterpret_cast<FrameCallbackFn>(callback.function)(data.frame_time_ns, callback.data);
    }
  }
  for (const RateCallback& rate : rates) rate.function(period, rate.data);
}

void DestroyChoreographer(AChoreographer* choreographer) noexcept {
  VsyncHub::Get().Unregister(choreographer);
  looper::RemoveHostSource(choreographer->looper, choreographer->source);
  looper::ReleaseLooper(choreographer->looper);
  delete choreographer;
}

struct ThreadChoreographer {
  ~ThreadChoreographer() { ReleaseThreadChoreographer(); }
  AChoreographer* choreographer = nullptr;
};
thread_local ThreadChoreographer t_choreographer;

std::int64_t DueTime(std::int64_t now_ns, std::int64_t delay_ms) {
  constexpr std::int64_t kNanosPerMilli = 1000000;
  if (delay_ms <= 0) return now_ns;
  if (delay_ms >= (INT64_MAX - now_ns) / kNanosPerMilli) return INT64_MAX;
  return now_ns + delay_ms * kNanosPerMilli;
}

void Post(AChoreographer* choreographer, CallbackKind kind, void* function, void* data, std::int64_t delay_ms) {
  if (choreographer == nullptr || function == nullptr) return;
  GuardedVoid([&] {
    {
      std::lock_guard<std::mutex> lock(choreographer->mutex);
      choreographer->callbacks.push_back(PendingCallback{kind, function, data, DueTime(MonotonicNs(), delay_ms)});
    }
    VsyncHub::Get().RequestVsync();
  });
}

}

void ReleaseThreadChoreographer() noexcept {
  if (ProcessShuttingDown()) return;
  AChoreographer* choreographer = t_choreographer.choreographer;
  if (choreographer == nullptr) return;
  t_choreographer.choreographer = nullptr;
  DestroyChoreographer(choreographer);
}

VsyncInfo GetVsyncInfo() { return VsyncHub::Get().Info(); }

}

ELIXIR_ANDROID_FN(AChoreographer*, AChoreographer_getInstance)() noexcept {
  using namespace elixir::android;
  return Guarded<AChoreographer*>(nullptr, [&]() -> AChoreographer* {
    if (t_choreographer.choreographer != nullptr) return t_choreographer.choreographer;
    ALooper* looper = looper::CurrentThreadLooper();
    if (looper == nullptr) return nullptr;
    auto* choreographer = new AChoreographer();
    choreographer->looper = looper;
    looper::AcquireLooper(looper);
    looper::HostSource source;
    source.ready = &ChoreographerReady;
    source.dispatch = &ChoreographerDispatch;
    source.context = choreographer;
    choreographer->source = looper::AddHostSource(looper, source);
    if (choreographer->source == 0) {
      looper::ReleaseLooper(looper);
      delete choreographer;
      return nullptr;
    }
    VsyncHub::Get().Register(choreographer);
    t_choreographer.choreographer = choreographer;
    return choreographer;
  });
}

ELIXIR_ANDROID_FN(void, AChoreographer_postFrameCallback)(AChoreographer* choreographer, void* callback,
                                                          void* data) noexcept {
  elixir::android::Post(choreographer, CallbackKind::kFrame, callback, data, 0);
}

ELIXIR_ANDROID_FN(void, AChoreographer_postFrameCallbackDelayed)(AChoreographer* choreographer, void* callback,
                                                                 void* data, std::int64_t delay_ms) noexcept {
  elixir::android::Post(choreographer, CallbackKind::kFrame, callback, data, delay_ms);
}

ELIXIR_ANDROID_FN(void, AChoreographer_postFrameCallback64)(AChoreographer* choreographer, void* callback,
                                                            void* data) noexcept {
  elixir::android::Post(choreographer, CallbackKind::kFrame, callback, data, 0);
}

ELIXIR_ANDROID_FN(void, AChoreographer_postFrameCallbackDelayed64)(AChoreographer* choreographer, void* callback,
                                                                   void* data, std::uint32_t delay_ms) noexcept {
  elixir::android::Post(choreographer, CallbackKind::kFrame, callback, data, delay_ms);
}

ELIXIR_ANDROID_FN(void, AChoreographer_postVsyncCallback)(AChoreographer* choreographer, void* callback,
                                                          void* data) noexcept {
  elixir::android::Post(choreographer, CallbackKind::kVsync, callback, data, 0);
}

ELIXIR_ANDROID_FN(void, AChoreographer_registerRefreshRateCallback)(AChoreographer* choreographer,
                                                                    RefreshRateCallbackFn callback,
                                                                    void* data) noexcept {
  if (choreographer == nullptr || callback == nullptr) return;
  elixir::android::GuardedVoid([&] {
    {
      std::lock_guard<std::mutex> lock(choreographer->mutex);
      for (const RateCallback& rate : choreographer->rate_callbacks) {
        if (rate.function == callback && rate.data == data) return;
      }
      choreographer->rate_callbacks.push_back(RateCallback{callback, data, true});
    }
    elixir::android::looper::SignalLooper(choreographer->looper);
  });
}

ELIXIR_ANDROID_FN(void, AChoreographer_unregisterRefreshRateCallback)(AChoreographer* choreographer,
                                                                      RefreshRateCallbackFn callback,
                                                                      void* data) noexcept {
  if (choreographer == nullptr) return;
  std::lock_guard<std::mutex> lock(choreographer->mutex);
  auto& rates = choreographer->rate_callbacks;
  rates.erase(std::remove_if(rates.begin(), rates.end(),
                             [&](const RateCallback& rate) { return rate.function == callback && rate.data == data; }),
              rates.end());
}

ELIXIR_ANDROID_FN(std::int64_t, AChoreographerFrameCallbackData_getFrameTimeNanos)(
    const GuestFrameCallbackData* data) noexcept {
  return data != nullptr ? data->frame_time_ns : 0;
}

ELIXIR_ANDROID_FN(std::uint64_t, AChoreographerFrameCallbackData_getFrameTimelinesLength)(
    const GuestFrameCallbackData* data) noexcept {
  return data != nullptr ? 1 : 0;
}

ELIXIR_ANDROID_FN(std::uint64_t, AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex)(
    const GuestFrameCallbackData* data) noexcept {
  return 0;
}

ELIXIR_ANDROID_FN(std::int64_t, AChoreographerFrameCallbackData_getFrameTimelineVsyncId)(
    const GuestFrameCallbackData* data, std::uint64_t index) noexcept {
  return data != nullptr && index == 0 ? data->vsync_id : 0;
}

ELIXIR_ANDROID_FN(std::int64_t, AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos)(
    const GuestFrameCallbackData* data, std::uint64_t index) noexcept {
  return data != nullptr && index == 0 ? data->expected_present_ns : 0;
}

ELIXIR_ANDROID_FN(std::int64_t, AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos)(
    const GuestFrameCallbackData* data, std::uint64_t index) noexcept {
  return data != nullptr && index == 0 ? data->deadline_ns : 0;
}

namespace elixir::android {

void RegisterChoreographerExports(bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##name))
  ELIXIR_ADD(AChoreographer_getInstance);
  ELIXIR_ADD(AChoreographer_postFrameCallback);
  ELIXIR_ADD(AChoreographer_postFrameCallbackDelayed);
  ELIXIR_ADD(AChoreographer_postFrameCallback64);
  ELIXIR_ADD(AChoreographer_postFrameCallbackDelayed64);
  ELIXIR_ADD(AChoreographer_postVsyncCallback);
  ELIXIR_ADD(AChoreographer_registerRefreshRateCallback);
  ELIXIR_ADD(AChoreographer_unregisterRefreshRateCallback);
  ELIXIR_ADD(AChoreographerFrameCallbackData_getFrameTimeNanos);
  ELIXIR_ADD(AChoreographerFrameCallbackData_getFrameTimelinesLength);
  ELIXIR_ADD(AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex);
  ELIXIR_ADD(AChoreographerFrameCallbackData_getFrameTimelineVsyncId);
  ELIXIR_ADD(AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos);
  ELIXIR_ADD(AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos);
#undef ELIXIR_ADD
}

}
