// Modified by Elixir.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "runtime/owned_pthread.h"

#if defined(_WIN32)
#include <windows.h>

#include "platform/windows/guest_tls.h"
#else
#include <signal.h>
#endif

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <new>
#include <thread>

namespace mocktail {
namespace runtime {

OwnedPthread::~OwnedPthread() {
  if (joinable_) {
    std::_Exit(EXIT_FAILURE);
  }
}

#if defined(_WIN32)
namespace {

struct ThreadStart {
  PthreadEntryPoint entry_point;
  void* context;
};

constexpr ULONG kStackGuaranteeBytes = 64 * 1024;

DWORD WINAPI ThreadMain(void* parameter) {
  ThreadStart start = *static_cast<ThreadStart*>(parameter);
  delete static_cast<ThreadStart*>(parameter);
  static_cast<void>(elixir::platform::EnsureGuestThread());
  ULONG guarantee = kStackGuaranteeBytes;
  SetThreadStackGuarantee(&guarantee);
  start.entry_point(start.context);
  return 0;
}

}

int OwnedPthread::Start(PthreadEntryPoint entry_point, void* context, std::size_t stack_size) {
  if (entry_point == nullptr) {
    return EINVAL;
  }
  if (joinable_) {
    return EBUSY;
  }
  auto* start = new (std::nothrow) ThreadStart{entry_point, context};
  if (start == nullptr) {
    return ENOMEM;
  }
  HANDLE thread = CreateThread(nullptr, stack_size, &ThreadMain, start,
                               stack_size != 0 ? STACK_SIZE_PARAM_IS_A_RESERVATION : 0, nullptr);
  if (thread == nullptr) {
    delete start;
    return EAGAIN;
  }
  thread_ = thread;
  joinable_ = true;
  return 0;
}

OwnedPthreadWaitResult OwnedPthread::WaitFor(int timeout_ms, int poll_interval_ms, PthreadWaitPump pump,
                                             void* pump_context) {
  if (!joinable_) {
    return {OwnedPthreadWaitStatus::kPlatformError, EINVAL};
  }
  const bool wait_forever = timeout_ms < 0;
  const int effective_poll_ms = poll_interval_ms > 0 ? poll_interval_ms : 1;
  const auto started_at = std::chrono::steady_clock::now();
  while (true) {
    const DWORD result = WaitForSingleObject(thread_, 0);
    if (result == WAIT_OBJECT_0) {
      CloseHandle(thread_);
      thread_ = nullptr;
      joinable_ = false;
      return {OwnedPthreadWaitStatus::kJoined, 0};
    }
    if (result != WAIT_TIMEOUT) {
      return {OwnedPthreadWaitStatus::kPlatformError, static_cast<int>(GetLastError())};
    }
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at);
    if (!wait_forever && elapsed.count() >= timeout_ms) {
      return {OwnedPthreadWaitStatus::kTimedOut, 0};
    }
    if (pump != nullptr) {
      pump(pump_context);
    }
    int sleep_ms = effective_poll_ms;
    if (!wait_forever) {
      const int remaining_ms = timeout_ms - static_cast<int>(elapsed.count());
      if (remaining_ms < sleep_ms) {
        sleep_ms = remaining_ms;
      }
    }
    if (sleep_ms > 0) {
      WaitForSingleObject(thread_, static_cast<DWORD>(sleep_ms));
    }
  }
}

OwnedPthreadCancelResult OwnedPthread::CancelAndJoinFor(int timeout_ms, int poll_interval_ms, PthreadWaitPump pump,
                                                        void* pump_context) {
  OwnedPthreadCancelResult result;
  if (!joinable_) {
    result.cancel_error = EINVAL;
    result.wait = {OwnedPthreadWaitStatus::kPlatformError, EINVAL};
    return result;
  }
  result.cancel_error = ENOSYS;
  result.wait = WaitFor(timeout_ms, poll_interval_ms, pump, pump_context);
  return result;
}

int OwnedPthread::Signal(int ) const { return joinable_ ? ENOSYS : ESRCH; }

#else

int OwnedPthread::Start(PthreadEntryPoint entry_point, void* context,
                        std::size_t stack_size) {
  if (entry_point == nullptr) {
    return EINVAL;
  }
  if (joinable_) {
    return EBUSY;
  }

  pthread_attr_t attributes;
  int result = pthread_attr_init(&attributes);
  if (result != 0) {
    return result;
  }
  if (stack_size != 0) {
    result = pthread_attr_setstacksize(&attributes, stack_size);
  }
  if (result == 0) {
    result = pthread_create(&thread_, &attributes, entry_point, context);
  }
  pthread_attr_destroy(&attributes);
  if (result == 0) {
    joinable_ = true;
  }
  return result;
}

OwnedPthreadWaitResult OwnedPthread::WaitFor(int timeout_ms,
                                             int poll_interval_ms,
                                             PthreadWaitPump pump,
                                             void* pump_context) {
  if (!joinable_) {
    return {OwnedPthreadWaitStatus::kPlatformError, EINVAL};
  }

  const bool wait_forever = timeout_ms < 0;
  const int effective_poll_ms = poll_interval_ms > 0 ? poll_interval_ms : 1;
  const auto started_at = std::chrono::steady_clock::now();
  while (true) {
    const int join_result = pthread_tryjoin_np(thread_, nullptr);
    if (join_result == 0) {
      joinable_ = false;
      return {OwnedPthreadWaitStatus::kJoined, 0};
    }
    if (join_result != EBUSY) {
      return {OwnedPthreadWaitStatus::kPlatformError, join_result};
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    if (!wait_forever && elapsed.count() >= timeout_ms) {
      return {OwnedPthreadWaitStatus::kTimedOut, 0};
    }
    if (pump != nullptr) {
      pump(pump_context);
    }

    int sleep_ms = effective_poll_ms;
    if (!wait_forever) {
      const int remaining_ms = timeout_ms - static_cast<int>(elapsed.count());
      if (remaining_ms < sleep_ms) {
        sleep_ms = remaining_ms;
      }
    }
    if (sleep_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
  }
}

OwnedPthreadCancelResult OwnedPthread::CancelAndJoinFor(int timeout_ms,
                                                        int poll_interval_ms,
                                                        PthreadWaitPump pump,
                                                        void* pump_context) {
  OwnedPthreadCancelResult result;
  if (!joinable_) {
    result.cancel_error = EINVAL;
    result.wait = {OwnedPthreadWaitStatus::kPlatformError, EINVAL};
    return result;
  }
  result.cancel_error = pthread_cancel(thread_);
  result.wait = WaitFor(timeout_ms, poll_interval_ms, pump, pump_context);
  return result;
}

int OwnedPthread::Signal(int signal_number) const {
  return joinable_ ? pthread_kill(thread_, signal_number) : ESRCH;
}

#endif

const char* OwnedPthreadWaitStatusName(OwnedPthreadWaitStatus status) {
  switch (status) {
    case OwnedPthreadWaitStatus::kJoined:
      return "Joined";
    case OwnedPthreadWaitStatus::kTimedOut:
      return "TimedOut";
    case OwnedPthreadWaitStatus::kPlatformError:
      return "PlatformError";
  }
  return "Unknown";
}

}
}
