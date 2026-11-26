// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_MEMORY_LIMIT_H_
#define MOCKTAIL_RUNTIME_MEMORY_LIMIT_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
#include "runtime/owned_pthread.h"
#endif

namespace mocktail {
namespace runtime {

struct ProcessMemoryUsage {
  std::uint64_t resident_bytes = 0;
  std::uint64_t swap_bytes = 0;
  bool valid = false;

  std::uint64_t committed_bytes() const {
    return resident_bytes + swap_bytes;
  }
};

ProcessMemoryUsage ParseProcessStatusMemory(std::string_view status);

enum class CgroupMemoryLimitStatus {
  kActive,
  kUnavailable,
  kExecFailed,
};

struct CgroupMemoryLimitResult {
  CgroupMemoryLimitStatus status = CgroupMemoryLimitStatus::kUnavailable;
  std::string detail;

  bool active() const { return status == CgroupMemoryLimitStatus::kActive; }
};

CgroupMemoryLimitResult MaybeReexecWithCgroupMemoryLimit(
    int argc, char* const argv[], std::uint64_t limit_bytes,
    const std::vector<std::string>* reexec_arguments = nullptr);

class MemoryLimitWatchdog final {
 public:
  MemoryLimitWatchdog() = default;
  ~MemoryLimitWatchdog();

  MemoryLimitWatchdog(const MemoryLimitWatchdog&) = delete;
  MemoryLimitWatchdog& operator=(const MemoryLimitWatchdog&) = delete;

  bool Start(std::uint64_t limit_bytes, std::string* error);
  void Stop();

  bool running() const { return running_; }

 private:
#if defined(_WIN32)
  static unsigned long __stdcall ThreadEntry(void* context);
#else
  static void* ThreadEntry(void* context);
#endif
  void Run();

  std::atomic<bool> stop_requested_{false};
  std::uint64_t limit_bytes_ = 0;
#if defined(_WIN32)
  void* thread_ = nullptr;
  void* stop_event_ = nullptr;
#else
  OwnedPthread worker_;
#endif
  bool running_ = false;
};

#if defined(_WIN32)
ProcessMemoryUsage ReadCurrentProcessMemoryUsage();
#endif

}
}

#endif
