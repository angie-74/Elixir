// Modified by Elixir.
#include "runtime/memory_limit.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace mocktail {
namespace runtime {
namespace {

constexpr std::uint64_t kBytesPerKibibyte = 1024U;
constexpr std::uint64_t kBytesPerMebibyte = 1024U * 1024U;
constexpr int kWatchdogPollMilliseconds = 100;

#if !defined(_WIN32)
void WriteAllBestEffort(int descriptor, std::string_view bytes) {
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const ssize_t written =
        write(descriptor, bytes.data() + offset, bytes.size() - offset);
    if (written > 0) {
      offset += static_cast<std::size_t>(written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    return;
  }
}
#endif

bool ParseKibibyteField(std::string_view status, std::string_view field,
                        std::uint64_t* bytes) {
  const std::size_t field_position = status.find(field);
  if (field_position == std::string_view::npos ||
      (field_position != 0 && status[field_position - 1] != '\n')) {
    return false;
  }
  std::size_t value_begin = field_position + field.size();
  while (value_begin < status.size() &&
         (status[value_begin] == ' ' || status[value_begin] == '\t')) {
    ++value_begin;
  }
  std::uint64_t kibibytes = 0;
  const char* begin = status.data() + value_begin;
  const char* end = status.data() + status.size();
  const std::from_chars_result parsed =
      std::from_chars(begin, end, kibibytes);
  if (parsed.ec != std::errc() || parsed.ptr == begin ||
      kibibytes > UINT64_MAX / kBytesPerKibibyte) {
    return false;
  }
  const char* suffix = parsed.ptr;
  while (suffix < end && (*suffix == ' ' || *suffix == '\t')) {
    ++suffix;
  }
  if (end - suffix < 2 || suffix[0] != 'k' || suffix[1] != 'B') {
    return false;
  }
  *bytes = kibibytes * kBytesPerKibibyte;
  return true;
}

#if defined(_WIN32)
ProcessMemoryUsage ReadCurrentProcessMemory() { return ReadCurrentProcessMemoryUsage(); }
#else
ProcessMemoryUsage ReadCurrentProcessMemory() {
  std::array<char, 32768> buffer{};
  const int status_file = open("/proc/self/status", O_RDONLY | O_CLOEXEC);
  if (status_file < 0) {
    return {};
  }
  ssize_t total = 0;
  while (static_cast<std::size_t>(total) < buffer.size()) {
    const ssize_t read_result =
        read(status_file, buffer.data() + total, buffer.size() - total);
    if (read_result > 0) {
      total += read_result;
      continue;
    }
    if (read_result < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
  (void)close(status_file);
  if (total <= 0) {
    return {};
  }
  return ParseProcessStatusMemory(
      std::string_view(buffer.data(), static_cast<std::size_t>(total)));
}
#endif

}

#if defined(_WIN32)
ProcessMemoryUsage ReadCurrentProcessMemoryUsage() {
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  ProcessMemoryUsage usage;
  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                            sizeof(counters))) {
    return usage;
  }
  usage.resident_bytes = static_cast<std::uint64_t>(counters.PrivateUsage);
  usage.swap_bytes = 0;
  usage.valid = true;
  return usage;
}
#endif

ProcessMemoryUsage ParseProcessStatusMemory(std::string_view status) {
  ProcessMemoryUsage usage;
  if (!ParseKibibyteField(status, "VmRSS:", &usage.resident_bytes)) {
    return usage;
  }
  std::uint64_t swap_bytes = 0;
  if (status.find("VmSwap:") != std::string_view::npos &&
      !ParseKibibyteField(status, "VmSwap:", &swap_bytes)) {
    return {};
  }
  usage.swap_bytes = swap_bytes;
  usage.valid = usage.resident_bytes <=
                UINT64_MAX - usage.swap_bytes;
  return usage;
}

CgroupMemoryLimitResult MaybeReexecWithCgroupMemoryLimit(
    int argc, char* const argv[], std::uint64_t limit_bytes,
    const std::vector<std::string>* reexec_arguments) {
  CgroupMemoryLimitResult result;
  (void)argc;
  (void)argv;
  (void)reexec_arguments;
  if (limit_bytes == 0) {
    result.detail = "memory limit is disabled";
    return result;
  }
  result.detail = "automatic cgroup scopes are disabled";
  return result;
}

MemoryLimitWatchdog::~MemoryLimitWatchdog() { Stop(); }

#if defined(_WIN32)
bool MemoryLimitWatchdog::Start(std::uint64_t limit_bytes,
                                std::string* error) {
  if (running_ || limit_bytes == 0) {
    if (error != nullptr) {
      *error = running_ ? "memory watchdog is already running"
                        : "memory watchdog requires a non-zero limit";
    }
    return false;
  }
  limit_bytes_ = limit_bytes;
  stop_requested_.store(false, std::memory_order_release);
  stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  thread_ = stop_event_ != nullptr
                ? CreateThread(nullptr, 0, &MemoryLimitWatchdog::ThreadEntry, this, 0, nullptr)
                : nullptr;
  if (thread_ == nullptr) {
    const DWORD code = GetLastError();
    if (stop_event_ != nullptr) CloseHandle(stop_event_);
    stop_event_ = nullptr;
    limit_bytes_ = 0;
    if (error != nullptr) {
      *error = "cannot start memory watchdog (Windows error " + std::to_string(code) + ")";
    }
    return false;
  }
  running_ = true;
  return true;
}

void MemoryLimitWatchdog::Stop() {
  if (!running_) {
    return;
  }
  stop_requested_.store(true, std::memory_order_release);
  SetEvent(stop_event_);
  if (WaitForSingleObject(thread_, INFINITE) != WAIT_OBJECT_0) {
    std::_Exit(EXIT_FAILURE);
  }
  CloseHandle(thread_);
  CloseHandle(stop_event_);
  thread_ = nullptr;
  stop_event_ = nullptr;
  running_ = false;
  limit_bytes_ = 0;
}

unsigned long __stdcall MemoryLimitWatchdog::ThreadEntry(void* context) {
  static_cast<MemoryLimitWatchdog*>(context)->Run();
  return 0;
}

void MemoryLimitWatchdog::Run() {
  while (!stop_requested_.load(std::memory_order_acquire)) {
    const ProcessMemoryUsage usage = ReadCurrentProcessMemory();
    if (usage.valid && usage.committed_bytes() >= limit_bytes_) {
      std::fprintf(stderr,
                   "[memory] limit exceeded: commit=%llu MiB, limit=%llu MiB; "
                   "terminating Elixir with exit 137\n",
                   static_cast<unsigned long long>(usage.committed_bytes() / kBytesPerMebibyte),
                   static_cast<unsigned long long>(limit_bytes_ / kBytesPerMebibyte));
      std::fflush(stderr);
      std::_Exit(137);
    }
    WaitForSingleObject(stop_event_, kWatchdogPollMilliseconds);
  }
}
#else
bool MemoryLimitWatchdog::Start(std::uint64_t limit_bytes,
                                std::string* error) {
  if (running_ || limit_bytes == 0) {
    if (error != nullptr) {
      *error = running_ ? "memory watchdog is already running"
                        : "memory watchdog requires a non-zero limit";
    }
    return false;
  }
  limit_bytes_ = limit_bytes;
  stop_requested_.store(false, std::memory_order_release);
  const int start_error =
      worker_.Start(&MemoryLimitWatchdog::ThreadEntry, this, 0);
  if (start_error != 0) {
    limit_bytes_ = 0;
    if (error != nullptr) {
      *error = std::string("cannot start memory watchdog: ") +
               std::strerror(start_error);
    }
    return false;
  }
  running_ = true;
  return true;
}

void MemoryLimitWatchdog::Stop() {
  if (!running_) {
    return;
  }
  stop_requested_.store(true, std::memory_order_release);
  const OwnedPthreadWaitResult wait = worker_.WaitFor(-1, 10);
  if (!wait.joined()) {
    std::_Exit(EXIT_FAILURE);
  }
  running_ = false;
  limit_bytes_ = 0;
}

void* MemoryLimitWatchdog::ThreadEntry(void* context) {
  static_cast<MemoryLimitWatchdog*>(context)->Run();
  return nullptr;
}

void MemoryLimitWatchdog::Run() {
  const timespec interval = {
      0, static_cast<long>(kWatchdogPollMilliseconds) * 1000L * 1000L};
  while (!stop_requested_.load(std::memory_order_acquire)) {
    const ProcessMemoryUsage usage = ReadCurrentProcessMemory();
    if (usage.valid && usage.committed_bytes() >= limit_bytes_) {
      const std::uint64_t used_mb =
          usage.committed_bytes() / kBytesPerMebibyte;
      const std::uint64_t limit_mb = limit_bytes_ / kBytesPerMebibyte;
      std::array<char, 256> message{};
      const int length = std::snprintf(
          message.data(), message.size(),
          "[memory] limit exceeded: RSS+swap=%llu MiB, limit=%llu MiB; "
          "terminating Mocktail with exit 137\n",
          static_cast<unsigned long long>(used_mb),
          static_cast<unsigned long long>(limit_mb));
      if (length > 0) {
        const std::size_t write_size =
            std::min(static_cast<std::size_t>(length), message.size() - 1U);
        WriteAllBestEffort(
            STDERR_FILENO,
            std::string_view(message.data(), write_size));
      }
      std::_Exit(137);
    }
    timespec remaining = interval;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR &&
           !stop_requested_.load(std::memory_order_acquire)) {
    }
  }
}
#endif

}
}
