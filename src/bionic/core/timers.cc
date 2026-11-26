// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>

#include "abi/sysv_abi.h"
#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"
#include "platform/windows/guest_tls.h"

namespace elixir::bionic {
namespace core {
namespace {

constexpr int kSigevSignal = 0, kSigevNone = 1, kSigevThread = 2, kSigevThreadId = 4;
constexpr int kItimerReal = 0, kItimerVirtual = 1, kItimerProf = 2;

struct GuestSigevent {
  std::uint64_t sigev_value;
  std::int32_t sigev_signo;
  std::int32_t sigev_notify;
  union {
    std::int32_t tid;
    struct {
      void* function;
      void* attribute;
    } thread;
    std::int32_t pad[12];
  } un;
};
static_assert(sizeof(GuestSigevent) == 64, "struct sigevent");

struct GuestItimerspec {
  guest::Timespec it_interval;
  guest::Timespec it_value;
};

struct Timer {
  std::int32_t clock_id = 0;
  int notify = kSigevSignal;
  int signo = 0;
  std::uint64_t value = 0;
  std::int32_t tid = 0;
  abi::SysvFn<void(std::uint64_t)> function = nullptr;
  bool armed = false;
  std::int64_t deadline_ns = 0;
  std::int64_t interval_ns = 0;
  int overrun = 0;
  int pending_overrun = 0;
  bool busy = false;
  bool deleted = false;
};

struct Service {
  SRWLOCK lock = SRWLOCK_INIT;
  HANDLE wake = nullptr;
  HANDLE timer = nullptr;
  HANDLE thread = nullptr;
  std::map<std::uintptr_t, std::shared_ptr<Timer>> timers;
  std::uintptr_t next_id = 1;
  std::shared_ptr<Timer> itimer_real;
};

Service& GetService() {
  static Service* service = new Service();
  return *service;
}

class Lock {
 public:
  explicit Lock(Service& service) noexcept : service_(service) { AcquireSRWLockExclusive(&service_.lock); }
  ~Lock() { ReleaseSRWLockExclusive(&service_.lock); }
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

 private:
  Service& service_;
};

struct ThreadCallback {
  std::shared_ptr<Timer> timer;
  std::uint64_t value;
  abi::SysvFn<void(std::uint64_t)> function;
};

void CALLBACK RunThreadCallback(PTP_CALLBACK_INSTANCE, void* context) {
  std::unique_ptr<ThreadCallback> work(static_cast<ThreadCallback*>(context));
  platform::EnsureGuestThread();
  work->function(work->value);
  Service& service = GetService();
  Lock lock(service);
  work->timer->busy = false;
}

void Fire(const std::shared_ptr<Timer>& timer, bool is_itimer) {
  if (is_itimer) {
    GenerateProcessSignal(guest::kSIGALRM, guest::kSI_KERNEL, 0);
    return;
  }
  switch (timer->notify) {
    case kSigevSignal: GenerateProcessSignal(timer->signo, guest::kSI_TIMER, timer->value); break;
    case kSigevThreadId: SendGuestSignalToThread(timer->tid, timer->signo); break;
    case kSigevThread: {
      auto* work = new ThreadCallback{timer, timer->value, timer->function};
      if (!TrySubmitThreadpoolCallback(RunThreadCallback, work, nullptr)) {
        Service& service = GetService();
        Lock lock(service);
        timer->busy = false;
        delete work;
      }
      break;
    }
    default: break;
  }
}

DWORD WINAPI ServiceMain(void*) {
  platform::EnsureGuestThread();
  Service& service = GetService();
  for (;;) {
    std::int64_t earliest = INT64_MAX;
    struct Expired {
      std::shared_ptr<Timer> timer;
      bool is_itimer;
    };
    Expired expired[32];
    int expired_count = 0;
    {
      Lock lock(service);
      const std::int64_t now = MonotonicNanoseconds();
      auto consider = [&](const std::shared_ptr<Timer>& timer, bool is_itimer) {
        if (!timer || !timer->armed || timer->deleted) return;
        if (timer->deadline_ns <= now && expired_count < 32) {
          int overruns = 0;
          if (timer->interval_ns > 0) {
            timer->deadline_ns += timer->interval_ns;
            while (timer->deadline_ns <= now) {
              timer->deadline_ns += timer->interval_ns;
              ++overruns;
            }
          } else {
            timer->armed = false;
          }
          if (!is_itimer && timer->notify == kSigevThread && timer->busy) {
            timer->pending_overrun += 1 + overruns;
            if (timer->armed && timer->deadline_ns < earliest) earliest = timer->deadline_ns;
            return;
          }
          timer->overrun = timer->pending_overrun + overruns;
          timer->pending_overrun = 0;
          if (!is_itimer && timer->notify == kSigevThread) timer->busy = true;
          expired[expired_count++] = Expired{timer, is_itimer};
        }
        if (timer->armed && timer->deadline_ns < earliest) earliest = timer->deadline_ns;
      };
      consider(service.itimer_real, true);
      for (auto& entry : service.timers) consider(entry.second, false);
    }
    for (int i = 0; i < expired_count; ++i) Fire(expired[i].timer, expired[i].is_itimer);
    if (expired_count > 0) continue;
    if (earliest == INT64_MAX) {
      WaitForSingleObject(service.wake, INFINITE);
      continue;
    }
    const std::int64_t remaining = earliest - MonotonicNanoseconds();
    if (remaining <= 0) continue;
    LARGE_INTEGER due;
    due.QuadPart = -(remaining / 100 > 0 ? remaining / 100 : 1);
    if (service.timer != nullptr && SetWaitableTimerEx(service.timer, &due, 0, nullptr, nullptr, nullptr, 0)) {
      HANDLE handles[2] = {service.wake, service.timer};
      WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    } else {
      const std::int64_t ms = (remaining + 999999) / 1000000;
      WaitForSingleObject(service.wake, ms > 0x7ffffffe ? 0x7ffffffe : static_cast<DWORD>(ms));
    }
  }
}

bool EnsureServiceLocked(Service& service) noexcept {
  if (service.thread != nullptr) return true;
  if (service.wake == nullptr) service.wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (service.timer == nullptr) {
    service.timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (service.timer == nullptr) service.timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
  }
  if (service.wake == nullptr) return false;
  service.thread = CreateThread(nullptr, 256 * 1024, ServiceMain, nullptr, 0, nullptr);
  if (service.thread != nullptr) SetThreadDescription(service.thread, L"elixir-timers");
  return service.thread != nullptr;
}

std::int64_t RemainingNanoseconds(const Timer& timer) noexcept {
  if (!timer.armed) return 0;
  const std::int64_t remaining = timer.deadline_ns - MonotonicNanoseconds();
  return remaining > 0 ? remaining : 1;
}

guest::Timeval ToTimeval(std::int64_t ns) noexcept {
  guest::Timeval tv;
  tv.tv_sec = ns / 1000000000;
  tv.tv_usec = (ns % 1000000000 + 999) / 1000;
  if (tv.tv_usec >= 1000000) {
    tv.tv_usec -= 1000000;
    ++tv.tv_sec;
  }
  return tv;
}

bool ValidTimeval(const guest::Timeval& tv) noexcept { return tv.tv_sec >= 0 && tv.tv_usec >= 0 && tv.tv_usec < 1000000; }

std::int64_t TimevalNanoseconds(const guest::Timeval& tv) noexcept {
  return TimespecToNanoseconds(guest::Timespec{tv.tv_sec, tv.tv_usec * 1000});
}

std::shared_ptr<Timer> FindTimer(Service& service, void* handle) {
  auto found = service.timers.find(reinterpret_cast<std::uintptr_t>(handle));
  return found == service.timers.end() ? nullptr : found->second;
}

}
}

ELIXIR_BIONIC_FN(int, getitimer)(int which, guest::Itimerval* value) noexcept {
  if (which == core::kItimerVirtual || which == core::kItimerProf) return FailWith(guest::kENOSYS, -1);
  if (which != core::kItimerReal) return FailWith(guest::kEINVAL, -1);
  if (value == nullptr) return FailWith(guest::kEFAULT, -1);
  core::Service& service = core::GetService();
  guest::Itimerval result{};
  {
    core::Lock lock(service);
    if (service.itimer_real && service.itimer_real->armed) {
      result.it_value = core::ToTimeval(core::RemainingNanoseconds(*service.itimer_real));
      result.it_interval = core::ToTimeval(service.itimer_real->interval_ns);
    }
  }
  *value = result;
  return 0;
}

ELIXIR_BIONIC_FN(int, setitimer)(int which, const guest::Itimerval* value, guest::Itimerval* old) noexcept {
  if (which == core::kItimerVirtual || which == core::kItimerProf) return FailWith(guest::kENOSYS, -1);
  if (which != core::kItimerReal) return FailWith(guest::kEINVAL, -1);
  guest::Itimerval incoming{};
  if (value != nullptr) {
    incoming = *value;
    if (!core::ValidTimeval(incoming.it_value) || !core::ValidTimeval(incoming.it_interval)) {
      return FailWith(guest::kEINVAL, -1);
    }
  }
  core::Service& service = core::GetService();
  guest::Itimerval previous{};
  try {
    core::Lock lock(service);
    if (!service.itimer_real) service.itimer_real = std::make_shared<core::Timer>();
    core::Timer& timer = *service.itimer_real;
    if (timer.armed) {
      previous.it_value = core::ToTimeval(core::RemainingNanoseconds(timer));
      previous.it_interval = core::ToTimeval(timer.interval_ns);
    }
    if (value != nullptr) {
      const std::int64_t first = core::TimevalNanoseconds(incoming.it_value);
      timer.interval_ns = core::TimevalNanoseconds(incoming.it_interval);
      timer.armed = first > 0;
      timer.deadline_ns = core::MonotonicNanoseconds() + first;
      if (timer.armed && !core::EnsureServiceLocked(service)) return FailWith(guest::kEAGAIN, -1);
      if (service.wake != nullptr) SetEvent(service.wake);
    }
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
  if (old != nullptr) *old = previous;
  return 0;
}

ELIXIR_BIONIC_FN(unsigned, alarm)(unsigned seconds) noexcept {
  guest::Itimerval value{};
  value.it_value.tv_sec = seconds;
  guest::Itimerval old{};
  if (elixir_bionic_setitimer(core::kItimerReal, &value, &old) != 0) return 0;
  unsigned remaining = static_cast<unsigned>(old.it_value.tv_sec);
  if (old.it_value.tv_usec >= 500000) ++remaining;
  if (remaining == 0 && old.it_value.tv_usec > 0) remaining = 1;
  return remaining;
}

ELIXIR_BIONIC_FN(int, timer_create)(std::int32_t clock_id, const core::GuestSigevent* event, void** timer_id) noexcept {
  using namespace guest;
  switch (clock_id) {
    case kCLOCK_REALTIME:
    case kCLOCK_MONOTONIC:
    case kCLOCK_BOOTTIME:
    case kCLOCK_TAI: break;
    case kCLOCK_REALTIME_ALARM:
    case kCLOCK_BOOTTIME_ALARM: return FailWith(kEPERM, -1);
    case kCLOCK_PROCESS_CPUTIME_ID:
    case kCLOCK_THREAD_CPUTIME_ID: return FailWith(kENOTSUP, -1);
    default: return FailWith(kEINVAL, -1);
  }
  if (timer_id == nullptr) return FailWith(kEFAULT, -1);
  auto timer = std::make_shared<core::Timer>();
  timer->clock_id = clock_id;
  if (event == nullptr) {
    timer->notify = core::kSigevSignal;
    timer->signo = kSIGALRM;
  } else {
    core::GuestSigevent copy;
    std::memcpy(&copy, event, sizeof(copy));
    timer->notify = copy.sigev_notify;
    timer->signo = copy.sigev_signo;
    timer->value = copy.sigev_value;
    switch (copy.sigev_notify) {
      case core::kSigevNone: break;
      case core::kSigevSignal:
        if (copy.sigev_signo < 1 || copy.sigev_signo > core::kMaxSignal) return FailWith(kEINVAL, -1);
        break;
      case core::kSigevThreadId:
        if (copy.sigev_signo < 1 || copy.sigev_signo > core::kMaxSignal || copy.un.tid <= 0) {
          return FailWith(kEINVAL, -1);
        }
        timer->tid = copy.un.tid;
        break;
      case core::kSigevThread:
        if (copy.un.thread.function == nullptr) return FailWith(kEINVAL, -1);
        timer->function = reinterpret_cast<abi::SysvFn<void(std::uint64_t)>>(copy.un.thread.function);
        break;
      default: return FailWith(kEINVAL, -1);
    }
  }
  core::Service& service = core::GetService();
  try {
    core::Lock lock(service);
    const std::uintptr_t id = service.next_id++;
    if (event == nullptr) timer->value = id;
    service.timers.emplace(id, timer);
    *timer_id = reinterpret_cast<void*>(id);
  } catch (...) {
    return FailWith(kEAGAIN, -1);
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, timer_settime)(void* timer_id, int flags, const core::GuestItimerspec* value,
                                     core::GuestItimerspec* old) noexcept {
  if (value == nullptr) return FailWith(guest::kEINVAL, -1);
  const core::GuestItimerspec incoming = *value;
  if (!core::ValidTimespec(incoming.it_value) || !core::ValidTimespec(incoming.it_interval)) {
    return FailWith(guest::kEINVAL, -1);
  }
  core::Service& service = core::GetService();
  core::GuestItimerspec previous{};
  {
    core::Lock lock(service);
    std::shared_ptr<core::Timer> timer = core::FindTimer(service, timer_id);
    if (!timer) return FailWith(guest::kEINVAL, -1);
    if (timer->armed) {
      previous.it_value = core::NanosecondsToTimespec(core::RemainingNanoseconds(*timer));
      previous.it_interval = core::NanosecondsToTimespec(timer->interval_ns);
    }
    std::int64_t first = core::TimespecToNanoseconds(incoming.it_value);
    timer->interval_ns = core::TimespecToNanoseconds(incoming.it_interval);
    timer->overrun = 0;
    timer->pending_overrun = 0;
    if (first == 0) {
      timer->armed = false;
    } else {
      if ((flags & guest::kTIMER_ABSTIME) != 0) {
        const bool realtime = timer->clock_id == guest::kCLOCK_REALTIME || timer->clock_id == guest::kCLOCK_TAI;
        first -= realtime ? core::RealtimeNanoseconds() : core::MonotonicNanoseconds();
        if (first < 0) first = 0;
      }
      timer->deadline_ns = core::MonotonicNanoseconds() + first;
      timer->armed = true;
      if (!core::EnsureServiceLocked(service)) {
        timer->armed = false;
        return FailWith(guest::kEAGAIN, -1);
      }
    }
    if (service.wake != nullptr) SetEvent(service.wake);
  }
  if (old != nullptr) *old = previous;
  return 0;
}

ELIXIR_BIONIC_FN(int, timer_gettime)(void* timer_id, core::GuestItimerspec* value) noexcept {
  if (value == nullptr) return FailWith(guest::kEFAULT, -1);
  core::Service& service = core::GetService();
  core::GuestItimerspec result{};
  {
    core::Lock lock(service);
    std::shared_ptr<core::Timer> timer = core::FindTimer(service, timer_id);
    if (!timer) return FailWith(guest::kEINVAL, -1);
    if (timer->armed) {
      result.it_value = core::NanosecondsToTimespec(core::RemainingNanoseconds(*timer));
      result.it_interval = core::NanosecondsToTimespec(timer->interval_ns);
    }
  }
  *value = result;
  return 0;
}

ELIXIR_BIONIC_FN(int, timer_getoverrun)(void* timer_id) noexcept {
  core::Service& service = core::GetService();
  core::Lock lock(service);
  std::shared_ptr<core::Timer> timer = core::FindTimer(service, timer_id);
  if (!timer) return FailWith(guest::kEINVAL, -1);
  return timer->overrun;
}

ELIXIR_BIONIC_FN(int, timer_delete)(void* timer_id) noexcept {
  core::Service& service = core::GetService();
  core::Lock lock(service);
  auto found = service.timers.find(reinterpret_cast<std::uintptr_t>(timer_id));
  if (found == service.timers.end()) return FailWith(guest::kEINVAL, -1);
  found->second->deleted = true;
  found->second->armed = false;
  service.timers.erase(found);
  if (service.wake != nullptr) SetEvent(service.wake);
  return 0;
}

namespace core {

void RegisterTimerExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(getitimer);
  ELIXIR_ADD(setitimer);
  ELIXIR_ADD(alarm);
  ELIXIR_ADD(timer_create);
  ELIXIR_ADD(timer_settime);
  ELIXIR_ADD(timer_gettime);
  ELIXIR_ADD(timer_getoverrun);
  ELIXIR_ADD(timer_delete);
#undef ELIXIR_ADD
}

}
}
