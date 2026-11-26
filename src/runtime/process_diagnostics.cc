// Modified by Elixir.
#include "runtime/process_diagnostics.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <cstdio>

namespace mocktail::runtime {
namespace {

const char* g_stage_name = "startup";

double FileTimeSeconds(const FILETIME& time) {
  const ULONGLONG ticks = (static_cast<ULONGLONG>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
  return static_cast<double>(ticks) / 1e7;
}

unsigned CountProcessThreads() {
  const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return 0;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  unsigned count = 0;
  const DWORD self = GetCurrentProcessId();
  for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry)) {
    if (entry.th32ProcessID == self) {
      count = entry.cntThreads;
      break;
    }
  }
  CloseHandle(snapshot);
  return count;
}

}

void InstallCpuLimitDiagnostics() {
  std::fputs("[diagnostic] CPU-time limit diagnostics (SIGXCPU) do not apply on Windows\n", stderr);
}

void LogProcessDiagnostics(ProcessDiagnosticStage stage) {
  g_stage_name = stage == ProcessDiagnosticStage::kNativeRuntime ? "native-runtime"
                 : stage == ProcessDiagnosticStage::kShutdown     ? "shutdown"
                                                                  : "startup";
  const HANDLE process = GetCurrentProcess();
  DWORD handles = 0;
  GetProcessHandleCount(process, &handles);
  PROCESS_MEMORY_COUNTERS_EX memory{};
  memory.cb = sizeof(memory);
  GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory));
  FILETIME created{}, exited{}, kernel{}, user{};
  GetProcessTimes(process, &created, &exited, &kernel, &user);
  PROCESS_POWER_THROTTLING_STATE throttling{};
  throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  const bool have_throttling =
      GetProcessInformation(process, ProcessPowerThrottling, &throttling, sizeof(throttling)) != FALSE;
  std::fprintf(stderr,
               "[diagnostic] stage=%s pid=%lu tid=%lu priority_class=0x%lx threads=%u handles=%lu "
               "commit_mib=%llu working_set_mib=%llu cpu_user_s=%.3f cpu_kernel_s=%.3f "
               "power_throttling=%s\n",
               g_stage_name, GetCurrentProcessId(), GetCurrentThreadId(), GetPriorityClass(process),
               CountProcessThreads(), handles,
               static_cast<unsigned long long>(memory.PrivateUsage >> 20),
               static_cast<unsigned long long>(memory.WorkingSetSize >> 20), FileTimeSeconds(user),
               FileTimeSeconds(kernel),
               !have_throttling ? "unknown"
               : (throttling.ControlMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) == 0
                   ? "system-managed"
                   : (throttling.StateMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) != 0 ? "on"
                                                                                            : "off");
}

}

#else

#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace mocktail::runtime {
namespace {

volatile sig_atomic_t current_stage = 0;

const char* StageName() {
  switch (current_stage) {
    case 1: return "native-runtime";
    case 2: return "shutdown";
    default: return "startup";
  }
}

void WriteBytes(const char* bytes, std::size_t size) {
  while (size > 0) {
    const ssize_t count = write(STDERR_FILENO, bytes, size);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return;
    bytes += count;
    size -= static_cast<std::size_t>(count);
  }
}

void Text(const char* text) {
  std::size_t length = 0;
  while (text[length] != '\0') ++length;
  WriteBytes(text, length);
}

void Number(std::uint64_t value, unsigned base = 10) {
  char buffer[32];
  std::size_t start = sizeof(buffer);
  do {
    buffer[--start] = "0123456789abcdef"[value % base];
    value /= base;
  } while (value != 0);
  WriteBytes(buffer + start, sizeof(buffer) - start);
}

void SignedNumber(long value) {
  if (value < 0) {
    Text("-");
    Number(static_cast<std::uint64_t>(-(value + 1)) + 1);
  } else {
    Number(static_cast<std::uint64_t>(value));
  }
}

void CpuTime(const char* label, clockid_t clock) {
  Text(label);
  timespec time{};
  if (clock_gettime(clock, &time) == 0) {
    Number(static_cast<std::uint64_t>(time.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(time.tv_nsec));
  } else {
    Text("unavailable");
  }
}

void DumpFile(const char* path, std::size_t maximum_bytes) {
  Text("[diagnostic] ");
  Text(path);
  Text("\n");
  const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
  if (descriptor < 0) {
    Text("unavailable errno=");
    SignedNumber(errno);
    Text("\n");
    return;
  }
  char buffer[2048];
  while (maximum_bytes > 0) {
    const std::size_t capacity = maximum_bytes < sizeof(buffer)
                                     ? maximum_bytes : sizeof(buffer);
    const ssize_t count = read(descriptor, buffer, capacity);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    WriteBytes(buffer, static_cast<std::size_t>(count));
    maximum_bytes -= static_cast<std::size_t>(count);
  }
  if (maximum_bytes == 0) Text("\n[diagnostic] file output capped\n");
  close(descriptor);
}

void Snapshot() {
  Text("[diagnostic] stage=");
  Text(StageName());
  Text(" pid=");
  Number(getpid());
  Text(" ppid=");
  Number(getppid());
  Text(" tid=");
  SignedNumber(syscall(SYS_gettid));
  Text(" scheduler=");
  SignedNumber(syscall(SYS_sched_getscheduler, 0));
  Text(" (OTHER=0 FIFO=1 RR=2 BATCH=3 IDLE=5; RESET_ON_FORK=1073741824)");
  CpuTime(" process_cpu_ns=", CLOCK_PROCESS_CPUTIME_ID);
  CpuTime(" thread_cpu_ns=", CLOCK_THREAD_CPUTIME_ID);
  CpuTime(" monotonic_ns=", CLOCK_MONOTONIC);
  Text("\n");
  DumpFile("/proc/self/limits", 8192);
  DumpFile("/proc/self/cgroup", 4096);
}

void CpuLimitSignal(int signal_number, siginfo_t* info, void* context) {
  Text("\n[crash] SIGXCPU signal=");
  SignedNumber(signal_number);
  Text(" si_code=");
  SignedNumber(info != nullptr ? info->si_code : 0);
  Text(" (SI_KERNEL=128 SI_USER=0 SI_TKILL=-6) ip=0x");
  std::uintptr_t instruction = 0;
  if (context != nullptr) {
#if defined(__x86_64__)
    instruction = static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP];
#elif defined(__aarch64__)
    instruction = static_cast<ucontext_t*>(context)->uc_mcontext.pc;
#endif
  }
  Number(instruction, 16);
  Text("\n");
  Snapshot();
  DumpFile("/proc/thread-self/comm", 256);
  DumpFile("/proc/thread-self/sched", 8192);
  DumpFile("/proc/self/maps", 65536);
  Text("[crash] SIGXCPU diagnostics complete; preserving fatal signal. "
       "For SI_KERNEL, compare CPU time with Max cpu time and the failing "
       "thread's policy with Max realtime timeout. Collect coredumpctl info "
       "for this PID.\n");
  if (syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), signal_number) < 0) {
    _exit(128 + signal_number);
  }
}

}

void InstallCpuLimitDiagnostics() {
  struct sigaction previous{};
  if (sigaction(SIGXCPU, nullptr, &previous) != 0) {
    Text("[diagnostic] cannot inspect SIGXCPU handler\n");
    return;
  }
  if (previous.sa_handler != SIG_DFL) {
    Text("[diagnostic] existing SIGXCPU handler retained\n");
    return;
  }
  struct sigaction action{};
  action.sa_sigaction = CpuLimitSignal;
  action.sa_flags = SA_SIGINFO | SA_RESETHAND;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGXCPU, &action, nullptr) != 0) {
    Text("[diagnostic] cannot install SIGXCPU diagnostics\n");
    return;
  }
  Text("[diagnostic] SIGXCPU diagnostics enabled; output goes to console and "
       "the active session log\n");
}

void LogProcessDiagnostics(ProcessDiagnosticStage stage) {
  current_stage = static_cast<sig_atomic_t>(stage);
  Snapshot();
}

}

#endif
