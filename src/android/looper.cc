// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail stubs/libandroid_stub.cc (Apache-2.0, Copyright 2026
// Mocktail Project Authors). Modified by Elixir.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "android/android_internal.h"
#include "bionic/runtime.h"

namespace guest = elixir::bionic::guest;
using elixir::android::GuestPollfd;
using elixir::android::looper::GuestLooperCallback;
using elixir::android::looper::HostSource;
using elixir::android::looper::HostSourceId;

namespace {

constexpr int kPrepareAllowNonCallbacks = 1 << 0;

struct LooperRegistration {
  int fd = -1;
  int ident = 0;
  int events = 0;
  GuestLooperCallback callback = nullptr;
  void* data = nullptr;
  std::uint64_t token = 0;
  bool invalid = false;
};

struct LooperResponse {
  LooperRegistration registration;
  int events = 0;
  HostSourceId source = 0;
};

std::atomic<elixir::android::FdWaitFn> g_fd_wait{nullptr};

std::atomic<bool> g_process_exiting{false};
void NoteProcessExit() { g_process_exiting.store(true, std::memory_order_release); }

}

struct ALooper {
  explicit ALooper(int requested_opts)
      : opts(requested_opts), allow_non_callbacks((requested_opts & kPrepareAllowNonCallbacks) != 0) {
    static const bool exit_hook = std::atexit(&NoteProcessExit) == 0;
    (void)exit_hook;
    wake_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer == nullptr) timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
  }

  ~ALooper() {
    const bool exiting =
        g_process_exiting.load(std::memory_order_acquire) || elixir::android::ProcessShuttingDown();
    if (!exiting && wake_read_fd >= 0) elixir::android::guest_libc::Close(wake_read_fd);
    if (!exiting && wake_write_fd >= 0 && wake_write_fd != wake_read_fd) {
      elixir::android::guest_libc::Close(wake_write_fd);
    }
    if (wake_event != nullptr) CloseHandle(wake_event);
    if (timer != nullptr) CloseHandle(timer);
  }

  ALooper(const ALooper&) = delete;
  ALooper& operator=(const ALooper&) = delete;

  bool valid() const { return wake_event != nullptr; }

  const int opts;
  const bool allow_non_callbacks;
  std::atomic<std::uint32_t> refs{1};
  std::atomic<bool> polling{false};
  std::atomic<bool> wake_pending{false};
  HANDLE wake_event = nullptr;
  HANDLE timer = nullptr;

  int wake_read_fd = -1;
  int wake_write_fd = -1;
  bool wake_is_eventfd = false;
  std::atomic<int> published_wake_fd{-1};

  std::mutex mutex;
  bool snapshot_taken = false;
  std::unordered_map<int, std::uint64_t> token_by_fd;
  std::unordered_map<std::uint64_t, LooperRegistration> registrations_by_token;
  std::uint64_t next_token = 1;
  std::vector<std::pair<HostSourceId, HostSource>> host_sources;
  HostSourceId next_source_id = 1;

  std::deque<LooperResponse> pending_responses;
  std::vector<GuestPollfd> poll_fds;
  std::vector<std::uint64_t> poll_tokens;
};

namespace elixir::android {
namespace {

void DeleteLooper(ALooper* looper) noexcept { delete looper; }

void ReleaseLooperReference(ALooper* looper) noexcept {
  if (looper != nullptr && looper->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) DeleteLooper(looper);
}

struct ThreadLooperState {
  ~ThreadLooperState();
  ALooper* looper = nullptr;
};
thread_local ThreadLooperState t_thread_looper;

void ReleaseThreadLooper() noexcept {
  if (ProcessShuttingDown()) return;
  ReleaseThreadChoreographer();
  ALooper* looper = t_thread_looper.looper;
  t_thread_looper.looper = nullptr;
  ReleaseLooperReference(looper);
}

ThreadLooperState::~ThreadLooperState() { ReleaseThreadLooper(); }

ELIXIR_SYSV void ThreadLooperKeyDestructor(void*) noexcept { ReleaseThreadLooper(); }

bool ThreadLooperKey(int* out) noexcept {
  static std::atomic<bool> created{false};
  static int key = 0;
  static SRWLOCK lock = SRWLOCK_INIT;
  if (!created.load(std::memory_order_acquire)) {
    AcquireSRWLockExclusive(&lock);
    if (!created.load(std::memory_order_relaxed)) {
      int fresh = 0;
      if (guest_libc::PthreadKeyCreate(&fresh, &ThreadLooperKeyDestructor) == 0) {
        key = fresh;
        created.store(true, std::memory_order_release);
      }
    }
    ReleaseSRWLockExclusive(&lock);
    if (!created.load(std::memory_order_acquire)) return false;
  }
  *out = key;
  return true;
}

void WriteWakeFd(ALooper* looper) noexcept {
  const int fd = looper->published_wake_fd.load(std::memory_order_acquire);
  if (fd < 0 || g_process_exiting.load(std::memory_order_acquire)) return;
  int error = 0;
  if (looper->wake_is_eventfd) {
    const std::uint64_t one = 1;
    guest_libc::Write(fd, &one, sizeof(one), &error);
  } else {
    const std::uint8_t one = 1;
    guest_libc::Write(fd, &one, sizeof(one), &error);
  }
}

void Signal(ALooper* looper) noexcept {
  SetEvent(looper->wake_event);
  if (g_fd_wait.load(std::memory_order_acquire) == nullptr) WriteWakeFd(looper);
}

void DrainWakeFd(ALooper* looper) noexcept {
  if (looper->wake_read_fd < 0) return;
  int error = 0;
  if (looper->wake_is_eventfd) {
    std::uint64_t value = 0;
    guest_libc::Read(looper->wake_read_fd, &value, sizeof(value), &error);
    return;
  }
  std::uint8_t buffer[256];
  while (guest_libc::Read(looper->wake_read_fd, buffer, sizeof(buffer), &error) ==
         static_cast<std::int64_t>(sizeof(buffer))) {
  }
}

int EnsureWakeFd(ALooper* looper) noexcept {
  if (g_fd_wait.load(std::memory_order_acquire) != nullptr || looper->wake_read_fd >= 0) return 0;
  if (!guest_libc::HasPoll()) return guest::kENOSYS;
  int error = 0;
  if (guest_libc::HasEventfd()) {
    const int fd = guest_libc::Eventfd(0, guest::kEFD_NONBLOCK | guest::kEFD_CLOEXEC, &error);
    if (fd >= 0) {
      looper->wake_read_fd = looper->wake_write_fd = fd;
      looper->wake_is_eventfd = true;
    }
  }
  if (looper->wake_read_fd < 0) {
    int fds[2] = {-1, -1};
    if (guest_libc::Pipe2(fds, guest::kO_NONBLOCK | guest::kO_CLOEXEC, &error) != 0) {
      return error != 0 ? error : guest::kEMFILE;
    }
    looper->wake_read_fd = fds[0];
    looper->wake_write_fd = fds[1];
    looper->wake_is_eventfd = false;
  }
  looper->published_wake_fd.store(looper->wake_write_fd, std::memory_order_release);
  if (looper->wake_pending.load(std::memory_order_acquire)) WriteWakeFd(looper);
  return 0;
}

std::int16_t PollEventsFromAndroid(int events) {
  std::int16_t result = 0;
  if ((events & looper::kEventInput) != 0) result |= guest::kPOLLIN;
  if ((events & looper::kEventOutput) != 0) result |= guest::kPOLLOUT;
  return result;
}

int AndroidEventsFromPoll(std::int16_t revents) {
  int result = 0;
  if ((revents & (guest::kPOLLIN | guest::kPOLLRDNORM | guest::kPOLLPRI)) != 0) result |= looper::kEventInput;
  if ((revents & (guest::kPOLLOUT | guest::kPOLLWRNORM)) != 0) result |= looper::kEventOutput;
  if ((revents & guest::kPOLLERR) != 0) result |= looper::kEventError;
  if ((revents & guest::kPOLLHUP) != 0) result |= looper::kEventHangup;
  return result;
}

std::uint64_t NextRegistrationTokenLocked(ALooper* looper) {
  for (;;) {
    const std::uint64_t token = looper->next_token++;
    if (looper->next_token == 0) looper->next_token = 1;
    if (token != 0 && looper->registrations_by_token.find(token) == looper->registrations_by_token.end()) {
      return token;
    }
  }
}

bool ResponseIsCurrent(ALooper* looper, const LooperResponse& response) {
  std::scoped_lock lock(looper->mutex);
  if (response.source != 0) {
    return std::any_of(looper->host_sources.begin(), looper->host_sources.end(),
                       [&](const auto& entry) { return entry.first == response.source; });
  }
  const auto fd_it = looper->token_by_fd.find(response.registration.fd);
  if (fd_it == looper->token_by_fd.end() || fd_it->second != response.registration.token) return false;
  return looper->registrations_by_token.find(response.registration.token) != looper->registrations_by_token.end();
}

int RemoveFdLocked(ALooper* looper, int fd, std::uint64_t expected_token) {
  const auto fd_it = looper->token_by_fd.find(fd);
  if (fd_it == looper->token_by_fd.end() || (expected_token != 0 && fd_it->second != expected_token)) return 0;
  looper->registrations_by_token.erase(fd_it->second);
  looper->token_by_fd.erase(fd_it);
  return 1;
}

void ClearPollOutputs(int* out_fd, int* out_events, void** out_data) {
  if (out_fd != nullptr) *out_fd = 0;
  if (out_events != nullptr) *out_events = 0;
  if (out_data != nullptr) *out_data = nullptr;
}

bool TakePendingIdent(ALooper* looper, int* out_fd, int* out_events, void** out_data, int* result) {
  while (!looper->pending_responses.empty()) {
    const LooperResponse response = looper->pending_responses.front();
    looper->pending_responses.pop_front();
    if (response.registration.callback != nullptr || !ResponseIsCurrent(looper, response)) continue;
    if (out_fd != nullptr) *out_fd = response.registration.fd;
    if (out_events != nullptr) *out_events = response.events;
    if (out_data != nullptr) *out_data = response.registration.data;
    *result = response.registration.ident;
    return true;
  }
  return false;
}

int RemainingPollTimeout(int original_timeout, const std::chrono::steady_clock::time_point& deadline) {
  if (original_timeout < 0) return -1;
  if (original_timeout == 0) return 0;
  const auto remaining = deadline - std::chrono::steady_clock::now();
  if (remaining <= std::chrono::steady_clock::duration::zero()) return 0;
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
  if (millis < remaining) millis += std::chrono::milliseconds(1);
  return static_cast<int>(std::min<std::int64_t>(millis.count(), std::numeric_limits<int>::max()));
}

void WaitOnWin32(ALooper* looper, int timeout_ms) {
  if (timeout_ms == 0) return;
  if (timeout_ms < 0 || looper->timer == nullptr) {
    WaitForSingleObject(looper->wake_event, timeout_ms < 0 ? INFINITE : static_cast<DWORD>(timeout_ms));
    return;
  }
  LARGE_INTEGER due;
  due.QuadPart = -static_cast<LONGLONG>(timeout_ms) * 10000;
  if (!SetWaitableTimer(looper->timer, &due, 0, nullptr, nullptr, FALSE)) {
    WaitForSingleObject(looper->wake_event, static_cast<DWORD>(timeout_ms));
    return;
  }
  const HANDLE handles[2] = {looper->wake_event, looper->timer};
  if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0) CancelWaitableTimer(looper->timer);
}

bool AnyHostSourceReady(ALooper* looper) {
  for (const auto& [id, source] : looper->host_sources) {
    if (source.ready != nullptr && source.ready(source.context)) return true;
  }
  return false;
}

bool DispatchHostSources(ALooper* looper) {
  std::vector<std::pair<HostSourceId, HostSource>> ready;
  {
    std::scoped_lock lock(looper->mutex);
    for (const auto& entry : looper->host_sources) {
      if (entry.second.ready != nullptr && entry.second.ready(entry.second.context)) ready.push_back(entry);
    }
  }
  bool invoked = false;
  for (const auto& [id, source] : ready) {
    if (source.dispatch != nullptr) {
      source.dispatch(source.context);
      invoked = true;
    } else if (source.guest_callback != nullptr) {
      const int keep = source.guest_callback(source.fake_fd, looper::kEventInput, source.data);
      invoked = true;
      if (keep == 0) looper::RemoveHostSource(looper, id);
    } else {
      LooperResponse response;
      response.registration.fd = source.fake_fd;
      response.registration.ident = source.ident;
      response.registration.data = source.data;
      response.events = looper::kEventInput;
      response.source = id;
      looper->pending_responses.push_back(response);
    }
  }
  return invoked;
}

int PollOnceImpl(int timeout_ms, int* out_fd, int* out_events, void** out_data) {
  ClearPollOutputs(out_fd, out_events, out_data);
  ALooper* looper = t_thread_looper.looper;
  if (looper == nullptr || !looper->valid()) return looper::kPollError;

  int pending_result = 0;
  if (TakePendingIdent(looper, out_fd, out_events, out_data, &pending_result)) return pending_result;

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 0);
  int wait_timeout = timeout_ms;

  for (;;) {
    const FdWaitFn custom_wait = g_fd_wait.load(std::memory_order_acquire);
    bool fd_mode = false;
    bool sources_ready = false;
    {
      std::scoped_lock lock(looper->mutex);
      looper->poll_fds.clear();
      looper->poll_tokens.clear();
      if (custom_wait == nullptr && looper->wake_read_fd < 0 && !looper->registrations_by_token.empty()) {
        EnsureWakeFd(looper);
      }
      if (custom_wait == nullptr && looper->wake_read_fd >= 0) {
        looper->poll_fds.push_back({looper->wake_read_fd, guest::kPOLLIN, 0});
        looper->poll_tokens.push_back(0);
      }
      for (const auto& [token, registration] : looper->registrations_by_token) {
        if (registration.invalid) continue;
        looper->poll_fds.push_back({registration.fd, PollEventsFromAndroid(registration.events), 0});
        looper->poll_tokens.push_back(token);
        fd_mode = true;
      }
      sources_ready = AnyHostSourceReady(looper);
      looper->snapshot_taken = true;
    }
    const int effective_timeout =
        sources_ready || looper->wake_pending.load(std::memory_order_acquire) ? 0 : wait_timeout;

    looper->polling.store(true, std::memory_order_release);
    int ready_count = 0;
    int wait_error = 0;
    if (fd_mode) {
      if (custom_wait != nullptr) {
        ready_count = custom_wait(looper->poll_fds.data(), looper->poll_fds.size(), looper->wake_event,
                                  effective_timeout, &wait_error);
      } else {
        ready_count = guest_libc::Poll(looper->poll_fds.data(), looper->poll_fds.size(), effective_timeout,
                                       &wait_error);
      }
    } else {
      WaitOnWin32(looper, effective_timeout);
    }
    looper->polling.store(false, std::memory_order_release);
    {
      std::scoped_lock lock(looper->mutex);
      looper->snapshot_taken = false;
    }

    if (ready_count < 0) {
      if (wait_error == guest::kEINTR) {
        wait_timeout = RemainingPollTimeout(timeout_ms, deadline);
        continue;
      }
      bionic::SetGuestErrno(wait_error);
      return looper::kPollError;
    }

    ResetEvent(looper->wake_event);
    std::vector<LooperResponse> responses;
    if (fd_mode) {
      for (std::size_t i = 0; i < looper->poll_fds.size(); ++i) {
        const GuestPollfd& entry = looper->poll_fds[i];
        if (entry.revents == 0) continue;
        if (looper->poll_tokens[i] == 0) {
          DrainWakeFd(looper);
          continue;
        }
        std::scoped_lock lock(looper->mutex);
        const auto it = looper->registrations_by_token.find(looper->poll_tokens[i]);
        if (it == looper->registrations_by_token.end()) continue;
        if ((entry.revents & guest::kPOLLNVAL) != 0) {
          it->second.invalid = true;
          continue;
        }
        responses.push_back({it->second, AndroidEventsFromPoll(entry.revents), 0});
      }
    }
    const bool had_wake = looper->wake_pending.exchange(false, std::memory_order_acq_rel);

    bool invoked_callback = DispatchHostSources(looper);
    for (const LooperResponse& response : responses) {
      if (response.registration.callback == nullptr) continue;
      const int keep =
          response.registration.callback(response.registration.fd, response.events, response.registration.data);
      invoked_callback = true;
      if (keep == 0) {
        std::scoped_lock lock(looper->mutex);
        RemoveFdLocked(looper, response.registration.fd, response.registration.token);
      }
    }
    for (const LooperResponse& response : responses) {
      if (response.registration.callback == nullptr) looper->pending_responses.push_back(response);
    }

    if (TakePendingIdent(looper, out_fd, out_events, out_data, &pending_result)) return pending_result;
    if (invoked_callback) return looper::kPollCallback;
    if (had_wake) return looper::kPollWake;
    if (timeout_ms == 0) return looper::kPollTimeout;
    wait_timeout = RemainingPollTimeout(timeout_ms, deadline);
    if (timeout_ms > 0 && wait_timeout == 0) return looper::kPollTimeout;
  }
}

void SignalIfPollingLocked(ALooper* looper) noexcept {
  if (looper->snapshot_taken) Signal(looper);
}

}

namespace looper {

HostSourceId AddHostSource(ALooper* looper, const HostSource& source) noexcept {
  if (looper == nullptr || !looper->valid()) return 0;
  return Guarded<HostSourceId>(0, [&] {
    std::scoped_lock lock(looper->mutex);
    const HostSourceId id = looper->next_source_id++;
    looper->host_sources.emplace_back(id, source);
    SignalIfPollingLocked(looper);
    return id;
  });
}

void RemoveHostSource(ALooper* looper, HostSourceId id) noexcept {
  if (looper == nullptr || id == 0) return;
  std::scoped_lock lock(looper->mutex);
  auto& sources = looper->host_sources;
  sources.erase(std::remove_if(sources.begin(), sources.end(), [id](const auto& entry) { return entry.first == id; }),
                sources.end());
}

void SignalLooper(ALooper* looper) noexcept {
  if (looper != nullptr && looper->valid()) Signal(looper);
}

void AcquireLooper(ALooper* looper) noexcept {
  if (looper != nullptr) looper->refs.fetch_add(1, std::memory_order_relaxed);
}

void ReleaseLooper(ALooper* looper) noexcept { ReleaseLooperReference(looper); }

ALooper* CurrentThreadLooper() noexcept { return t_thread_looper.looper; }

bool AllowsNonCallbacks(ALooper* looper) noexcept { return looper != nullptr && looper->allow_non_callbacks; }

}

void SetLooperFdWait(FdWaitFn wait) { g_fd_wait.store(wait, std::memory_order_release); }

}

using elixir::android::Guarded;

ELIXIR_ANDROID_FN(ALooper*, ALooper_forThread)() noexcept { return elixir::android::t_thread_looper.looper; }

ELIXIR_ANDROID_FN(ALooper*, ALooper_prepare)(int opts) noexcept {
  using elixir::android::t_thread_looper;
  if (t_thread_looper.looper != nullptr) return t_thread_looper.looper;
  auto* looper = new (std::nothrow) ALooper(opts);
  if (looper == nullptr) return elixir::bionic::FailWith<ALooper*>(guest::kENOMEM, nullptr);
  if (!looper->valid()) {
    delete looper;
    return elixir::bionic::FailWith<ALooper*>(guest::kENOMEM, nullptr);
  }
  t_thread_looper.looper = looper;
  const int saved_errno = elixir::bionic::GuestErrno();
  int key = 0;
  if (elixir::android::ThreadLooperKey(&key)) elixir::android::guest_libc::PthreadSetspecific(key, looper);
  elixir::bionic::SetGuestErrno(saved_errno);
  return looper;
}

ELIXIR_ANDROID_FN(void, ALooper_acquire)(ALooper* looper) noexcept { elixir::android::looper::AcquireLooper(looper); }

ELIXIR_ANDROID_FN(void, ALooper_release)(ALooper* looper) noexcept {
  elixir::android::ReleaseLooperReference(looper);
}

ELIXIR_ANDROID_FN(int, ALooper_addFd)(ALooper* looper, int fd, int ident, int events, GuestLooperCallback callback,
                                      void* data) noexcept {
  using namespace elixir::android;
  if (looper == nullptr || !looper->valid() || fd < 0 ||
      (callback == nullptr && (!looper->allow_non_callbacks || ident < 0))) {
    return elixir::bionic::FailWith(guest::kEINVAL, -1);
  }
  if (g_fd_wait.load(std::memory_order_acquire) == nullptr && guest_libc::HasPoll()) {
    GuestPollfd probe{fd, 0, 0};
    int error = 0;
    if (guest_libc::Poll(&probe, 1, 0, &error) == 1 && (probe.revents & guest::kPOLLNVAL) != 0) {
      return elixir::bionic::FailWith(guest::kEBADF, -1);
    }
  }
  return Guarded(-1, [&] {
    std::scoped_lock lock(looper->mutex);
    if (const int error = EnsureWakeFd(looper); error != 0) return elixir::bionic::FailWith(error, -1);
    auto fd_it = looper->token_by_fd.find(fd);
    const bool update = fd_it != looper->token_by_fd.end();
    const std::uint64_t old_token = update ? fd_it->second : 0;
    const std::uint64_t token = NextRegistrationTokenLocked(looper);
    LooperRegistration registration;
    registration.fd = fd;
    registration.ident = callback != nullptr ? looper::kPollCallback : ident;
    registration.events = events;
    registration.callback = callback;
    registration.data = data;
    registration.token = token;
    looper->registrations_by_token.emplace(token, registration);
    if (update) {
      fd_it->second = token;
      looper->registrations_by_token.erase(old_token);
    } else {
      looper->token_by_fd.emplace(fd, token);
    }
    SignalIfPollingLocked(looper);
    return 1;
  });
}

ELIXIR_ANDROID_FN(int, ALooper_removeFd)(ALooper* looper, int fd) noexcept {
  using namespace elixir::android;
  if (looper == nullptr || !looper->valid() || fd < 0) return elixir::bionic::FailWith(guest::kEINVAL, -1);
  std::scoped_lock lock(looper->mutex);
  const int removed = RemoveFdLocked(looper, fd, 0);
  if (removed != 0) SignalIfPollingLocked(looper);
  return removed;
}

ELIXIR_ANDROID_FN(void, ALooper_wake)(ALooper* looper) noexcept {
  if (looper == nullptr || !looper->valid()) return;
  looper->wake_pending.store(true, std::memory_order_release);
  elixir::android::Signal(looper);
}

ELIXIR_ANDROID_FN(bool, ALooper_isPolling)(ALooper* looper) noexcept {
  return looper != nullptr && looper->polling.load(std::memory_order_acquire);
}

ELIXIR_ANDROID_FN(int, ALooper_pollOnce)(int timeout_ms, int* out_fd, int* out_events, void** out_data) noexcept {
  return Guarded(elixir::android::looper::kPollError,
                 [&] { return elixir::android::PollOnceImpl(timeout_ms, out_fd, out_events, out_data); });
}

ELIXIR_ANDROID_FN(int, ALooper_pollAll)(int timeout_ms, int* out_fd, int* out_events, void** out_data) noexcept {
  using namespace elixir::android;
  return Guarded(looper::kPollError, [&] {
    if (timeout_ms <= 0) {
      int result;
      do {
        result = PollOnceImpl(timeout_ms, out_fd, out_events, out_data);
      } while (result == looper::kPollCallback);
      return result;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    int remaining = timeout_ms;
    for (;;) {
      const int result = PollOnceImpl(remaining, out_fd, out_events, out_data);
      if (result != looper::kPollCallback) return result;
      remaining = RemainingPollTimeout(timeout_ms, deadline);
      if (remaining == 0) return looper::kPollTimeout;
    }
  });
}

namespace elixir::android {

void RegisterLooperExports(elixir::bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##name))
  ELIXIR_ADD(ALooper_forThread);
  ELIXIR_ADD(ALooper_prepare);
  ELIXIR_ADD(ALooper_acquire);
  ELIXIR_ADD(ALooper_release);
  ELIXIR_ADD(ALooper_addFd);
  ELIXIR_ADD(ALooper_removeFd);
  ELIXIR_ADD(ALooper_wake);
  ELIXIR_ADD(ALooper_isPolling);
  ELIXIR_ADD(ALooper_pollOnce);
  ELIXIR_ADD(ALooper_pollAll);
#undef ELIXIR_ADD
}

}
