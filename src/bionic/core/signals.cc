// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <intrin.h>
#include <psapi.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"
#include "bionic/threads.h"
#include "platform/windows/guest_tls.h"

namespace elixir::bionic::core {
namespace {

using guest::Sigset;

constexpr Sigset kUnblockable = SignalBit(guest::kSIGKILL) | SignalBit(guest::kSIGSTOP);
constexpr Sigset kAllSignals = ~Sigset{0};
constexpr std::int32_t kSsAutodisarm = static_cast<std::int32_t>(0x80000000u);

using Handler = abi::SysvFn<void(int, guest::Siginfo*, void*)>;

struct Action {
  void* handler = nullptr;
  std::int32_t flags = 0;
  Sigset mask = 0;
  void* restorer = nullptr;
};

struct PendingInfo {
  std::int32_t code;
  std::int32_t pid;
  std::uint32_t uid;
  std::uint64_t value;
};

struct ThreadSignals {
  Sigset mask;
  Sigset pending;
  PendingInfo pending_info[kMaxSignal + 1];
  void* alt_sp;
  std::uint64_t alt_size;
  std::int32_t alt_flags;
  bool alt_enabled;
  bool registered;
  bool holds_lock;
  bool delivering;
  std::uint32_t staged_size;
  std::uint32_t tid;
  HANDLE wake_event;
  Sigset wait_set;
  bool wait_is_sigwait;
};

thread_local ThreadSignals t_signals;
constexpr std::size_t kStagingBytes = 8192;
alignas(64) thread_local unsigned char t_staging[kStagingBytes];

SRWLOCK g_state_lock = SRWLOCK_INIT;
Action g_actions[kMaxSignal + 1];
std::vector<ThreadSignals*>* g_threads = nullptr;
std::atomic<Sigset> g_process_pending{0};
PendingInfo g_process_info[kMaxSignal + 1];
std::deque<PendingInfo>* g_rt_queue = nullptr;
struct EarlyPending {
  std::uint32_t tid;
  int sig;
  PendingInfo info;
};
std::vector<EarlyPending>* g_early_pending = nullptr;

std::atomic<FaultFilter> g_fault_filter{nullptr};
std::atomic<std::uint64_t> g_delivered_faults{0};
std::uint64_t g_restorer = 0;
DWORD64 g_enabled_xstate = 0;
struct ModuleRange {
  std::uintptr_t begin, end;
};
ModuleRange g_system_modules[3] = {};
std::once_flag g_veh_once;

class StateLock {
 public:
  explicit StateLock(bool exclusive = true) noexcept : exclusive_(exclusive) {
    if (exclusive_) {
      AcquireSRWLockExclusive(&g_state_lock);
    } else {
      AcquireSRWLockShared(&g_state_lock);
    }
    t_signals.holds_lock = true;
  }
  ~StateLock() {
    t_signals.holds_lock = false;
    if (exclusive_) {
      ReleaseSRWLockExclusive(&g_state_lock);
    } else {
      ReleaseSRWLockShared(&g_state_lock);
    }
  }
  StateLock(const StateLock&) = delete;
  StateLock& operator=(const StateLock&) = delete;

 private:
  bool exclusive_;
};

bool ValidSignal(int sig) noexcept { return sig >= 1 && sig <= kMaxSignal; }
int LowestSignal(Sigset set) noexcept {
  for (int sig = 1; sig <= kMaxSignal; ++sig) {
    if ((set & SignalBit(sig)) != 0) return sig;
  }
  return 0;
}

bool DefaultIgnored(int sig) noexcept {
  using namespace guest;
  return sig == kSIGCHLD || sig == kSIGURG || sig == kSIGWINCH || sig == kSIGCONT ||
         sig == kSIGSTOP || sig == kSIGTSTP || sig == kSIGTTIN || sig == kSIGTTOU;
}

bool DefaultDumpsCore(int sig) noexcept {
  using namespace guest;
  return sig == kSIGQUIT || sig == kSIGILL || sig == kSIGTRAP || sig == kSIGABRT || sig == kSIGBUS ||
         sig == kSIGFPE || sig == kSIGSEGV || sig == kSIGXCPU || sig == kSIGXFSZ || sig == kSIGSYS;
}

const char* const kSignalNames[kMaxSignal + 1] = {
    nullptr, "HUP",  "INT",  "QUIT", "ILL",    "TRAP", "ABRT", "BUS",  "FPE",  "KILL", "USR1",
    "SEGV",  "USR2", "PIPE", "ALRM", "TERM",   "STKFLT", "CHLD", "CONT", "STOP", "TSTP", "TTIN",
    "TTOU",  "URG",  "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO",   "PWR",  "SYS"};

const char* const kSignalDescriptions[kMaxSignal + 1] = {
    nullptr,
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trap",
    "Aborted",
    "Bus error",
    "Floating point exception",
    "Killed",
    "User signal 1",
    "Segmentation fault",
    "User signal 2",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
    "Stack fault",
    "Child exited",
    "Continue",
    "Stopped (signal)",
    "Stopped",
    "Stopped (tty input)",
    "Stopped (tty output)",
    "Urgent I/O condition",
    "CPU time limit exceeded",
    "File size limit exceeded",
    "Virtual timer expired",
    "Profiling timer expired",
    "Window size changed",
    "I/O possible",
    "Power failure",
    "Bad system call"};

const char* DescribeSignal(int sig, char* buffer, std::size_t size) noexcept {
  if (sig >= 1 && sig < guest::kSIGSYS + 1 && kSignalDescriptions[sig] != nullptr) {
    return kSignalDescriptions[sig];
  }
  const char* prefix = "Unknown";
  int number = sig;
  if (sig >= kSigRtMin && sig <= kSigRtMax) {
    prefix = "Real-time";
    number = sig - kSigRtMin;
  }
  std::snprintf(buffer, size, "%s signal %d", prefix, number);
  return buffer;
}

void EnsureTablesLocked() {
  if (g_threads == nullptr) g_threads = new std::vector<ThreadSignals*>();
  if (g_rt_queue == nullptr) g_rt_queue = new std::deque<PendingInfo>[kMaxSignal + 1];
  if (g_early_pending == nullptr) g_early_pending = new std::vector<EarlyPending>();
}

struct ThreadSignalsCleanup {
  ~ThreadSignalsCleanup() {
    ThreadSignals& self = t_signals;
    if (!self.registered) return;
    {
      StateLock lock;
      auto& threads = *g_threads;
      for (std::size_t i = 0; i < threads.size(); ++i) {
        if (threads[i] == &self) {
          threads[i] = threads.back();
          threads.pop_back();
          break;
        }
      }
      self.registered = false;
    }
    if (self.wake_event != nullptr) {
      CloseHandle(self.wake_event);
      self.wake_event = nullptr;
    }
  }
};
thread_local ThreadSignalsCleanup t_cleanup;

ThreadSignals& Self() noexcept {
  ThreadSignals& self = t_signals;
  if (self.registered) return self;
  self.tid = GetCurrentThreadId();
  (void)&t_cleanup;
  try {
    StateLock lock;
    EnsureTablesLocked();
    g_threads->push_back(&self);
    self.registered = true;
    auto& early = *g_early_pending;
    for (std::size_t i = 0; i < early.size();) {
      if (early[i].tid == self.tid) {
        self.pending |= SignalBit(early[i].sig);
        self.pending_info[early[i].sig] = early[i].info;
        early[i] = early.back();
        early.pop_back();
      } else {
        ++i;
      }
    }
  } catch (...) {
  }
  return self;
}

HANDLE WakeEvent(ThreadSignals& self) noexcept {
  if (self.wake_event == nullptr) self.wake_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  return self.wake_event;
}

Action LoadAction(int sig) noexcept {
  StateLock lock(false);
  return g_actions[sig];
}

void AddProcessPendingLocked(int sig, const PendingInfo& info) {
  const Sigset bit = SignalBit(sig);
  if ((g_process_pending.load() & bit) == 0) {
    g_process_info[sig] = info;
    g_process_pending.fetch_or(bit);
  } else if (sig >= kSigRtMin) {
    g_rt_queue[sig].push_back(info);
  }
}

bool TakePendingLocked(ThreadSignals& self, int sig, PendingInfo* out) {
  const Sigset bit = SignalBit(sig);
  if ((self.pending & bit) != 0) {
    *out = self.pending_info[sig];
    self.pending &= ~bit;
    return true;
  }
  if ((g_process_pending.load() & bit) != 0) {
    *out = g_process_info[sig];
    if (g_rt_queue != nullptr && !g_rt_queue[sig].empty()) {
      g_process_info[sig] = g_rt_queue[sig].front();
      g_rt_queue[sig].pop_front();
    } else {
      g_process_pending.fetch_and(~bit);
    }
    return true;
  }
  return false;
}

void FillSiginfo(guest::Siginfo* info, int sig, const PendingInfo& pending) noexcept {
  std::memset(info, 0, sizeof(*info));
  info->si_signo = sig;
  info->si_code = pending.code;
  if (pending.code == guest::kSI_TIMER) {
    info->fields.raw[0] = 0;
    info->fields.raw[1] = 0;
  } else {
    info->fields.kill.si_pid = pending.pid;
    info->fields.kill.si_uid = pending.uid;
  }
  std::memcpy(reinterpret_cast<char*>(&info->fields) + 8, &pending.value, sizeof(pending.value));
}

void FillGregs(guest::Mcontext* mc, const CONTEXT& c) noexcept {
  using namespace guest;
  auto* g = mc->gregs;
  g[kRegR8] = static_cast<std::int64_t>(c.R8);
  g[kRegR9] = static_cast<std::int64_t>(c.R9);
  g[kRegR10] = static_cast<std::int64_t>(c.R10);
  g[kRegR11] = static_cast<std::int64_t>(c.R11);
  g[kRegR12] = static_cast<std::int64_t>(c.R12);
  g[kRegR13] = static_cast<std::int64_t>(c.R13);
  g[kRegR14] = static_cast<std::int64_t>(c.R14);
  g[kRegR15] = static_cast<std::int64_t>(c.R15);
  g[kRegRdi] = static_cast<std::int64_t>(c.Rdi);
  g[kRegRsi] = static_cast<std::int64_t>(c.Rsi);
  g[kRegRbp] = static_cast<std::int64_t>(c.Rbp);
  g[kRegRbx] = static_cast<std::int64_t>(c.Rbx);
  g[kRegRdx] = static_cast<std::int64_t>(c.Rdx);
  g[kRegRax] = static_cast<std::int64_t>(c.Rax);
  g[kRegRcx] = static_cast<std::int64_t>(c.Rcx);
  g[kRegRsp] = static_cast<std::int64_t>(c.Rsp);
  g[kRegRip] = static_cast<std::int64_t>(c.Rip);
  g[kRegEfl] = static_cast<std::int64_t>(c.EFlags);
  g[kRegCsgsfs] = static_cast<std::int64_t>(static_cast<std::uint64_t>(c.SegCs) |
                                            (static_cast<std::uint64_t>(c.SegGs) << 16) |
                                            (static_cast<std::uint64_t>(c.SegFs) << 32) |
                                            (static_cast<std::uint64_t>(c.SegSs) << 48));
}

void FillStackT(guest::StackT* stack, const ThreadSignals& self, bool on_alt) noexcept {
  stack->ss_sp = self.alt_sp;
  stack->ss_size = self.alt_size;
  stack->ss_flags = on_alt ? guest::kSS_ONSTACK : (self.alt_enabled ? self.alt_flags : guest::kSS_DISABLE);
}

bool OnAltStack(const ThreadSignals& self, std::uint64_t sp) noexcept {
  if (!self.alt_enabled) return false;
  const auto low = reinterpret_cast<std::uint64_t>(self.alt_sp);
  return sp > low && sp <= low + self.alt_size;
}

std::uint64_t CurrentStackPointer() noexcept {
  return reinterpret_cast<std::uint64_t>(_AddressOfReturnAddress());
}

}
}

extern "C" ELIXIR_SYSV ELIXIR_NAKED void elixir_core_call_on_stack(void* , int ,
                                                                  void* , void* ,
                                                                  void* ) {
  __asm__ volatile(
      "pushq %rbp\n\t"
      "movq %rsp, %rbp\n\t"
      "andq $-16, %r8\n\t"
      "movq %r8, %rsp\n\t"
      "movq %rdi, %rax\n\t"
      "movl %esi, %edi\n\t"
      "movq %rdx, %rsi\n\t"
      "movq %rcx, %rdx\n\t"
      "callq *%rax\n\t"
      "movq %rbp, %rsp\n\t"
      "popq %rbp\n\t"
      "retq\n\t");
}

namespace elixir::bionic::core {
namespace {

void DeliverOnCurrentThread(int sig, const PendingInfo& pending) noexcept {
  const Action action = LoadAction(sig);
  if (action.handler == kSigIgn) return;
  if (action.handler == kSigDfl) {
    if (!DefaultIgnored(sig)) TerminateForSignal(sig);
    return;
  }
  ThreadSignals& self = Self();
  guest::Siginfo info;
  FillSiginfo(&info, sig, pending);
  guest::Ucontext uc;
  std::memset(&uc, 0, sizeof(uc));
  CONTEXT context;
  RtlCaptureContext(&context);
  FillGregs(&uc.uc_mcontext, context);
  std::memcpy(uc.fpregs_mem, &context.FltSave, sizeof(uc.fpregs_mem));
  uc.uc_mcontext.fpregs = uc.fpregs_mem;
  const std::uint64_t sp = CurrentStackPointer();
  const bool on_alt = OnAltStack(self, sp);
  FillStackT(&uc.uc_stack, self, on_alt);
  uc.uc_sigmask = self.mask;

  Sigset blocked = action.mask;
  if ((action.flags & guest::kSA_NODEFER) == 0) blocked |= SignalBit(sig);
  self.mask = (self.mask | blocked) & ~kUnblockable;
  if ((action.flags & static_cast<std::int32_t>(guest::kSA_RESETHAND)) != 0) {
    StateLock lock;
    g_actions[sig] = Action{};
  }
  const bool use_alt = (action.flags & guest::kSA_ONSTACK) != 0 && self.alt_enabled && !on_alt;
  if (use_alt) {
    auto* top = static_cast<char*>(self.alt_sp) + self.alt_size;
    if ((self.alt_flags & kSsAutodisarm) != 0) self.alt_enabled = false;
    elixir_core_call_on_stack(action.handler, sig, &info, &uc, top);
    if ((self.alt_flags & kSsAutodisarm) != 0) self.alt_enabled = true;
  } else {
    reinterpret_cast<Handler>(action.handler)(sig, &info, &uc);
  }
  self.mask = uc.uc_sigmask & ~kUnblockable;
}

int DeliverPendingSignals() noexcept {
  ThreadSignals& self = Self();
  int delivered = 0;
  for (int round = 0; round < 2 * kMaxSignal; ++round) {
    const Sigset deliverable = (self.pending | g_process_pending.load()) & ~self.mask;
    if (deliverable == 0) break;
    const int sig = LowestSignal(deliverable);
    PendingInfo info{};
    bool taken = false;
    {
      StateLock lock;
      taken = TakePendingLocked(self, sig, &info);
    }
    if (!taken) continue;
    DeliverOnCurrentThread(sig, info);
    ++delivered;
  }
  return delivered;
}

PendingInfo MakeInfo(int code, std::uint64_t value = 0) noexcept {
  PendingInfo info{};
  info.code = code;
  info.pid = CurrentPid();
  info.uid = CurrentUid();
  info.value = value;
  return info;
}

bool IsIgnoredDisposition(int sig, const Action& action) noexcept {
  return action.handler == kSigIgn || (action.handler == kSigDfl && DefaultIgnored(sig));
}

void SendToProcess(int sig, const PendingInfo& info) noexcept {
  if (sig == 0) return;
  ThreadSignals& self = Self();
  const Sigset bit = SignalBit(sig);
  bool deliver_here = false;
  {
    StateLock lock;
    EnsureTablesLocked();
    for (ThreadSignals* thread : *g_threads) {
      if (thread->wait_is_sigwait && (thread->wait_set & bit) != 0) {
        thread->pending |= bit;
        thread->pending_info[sig] = info;
        thread->wait_set = 0;
        if (thread->wake_event != nullptr) SetEvent(thread->wake_event);
        return;
      }
    }
    const Action& action = g_actions[sig];
    if (IsIgnoredDisposition(sig, action)) {
      bool blocked_somewhere = false;
      for (ThreadSignals* thread : *g_threads) blocked_somewhere |= (thread->mask & bit) != 0;
      if (!blocked_somewhere) return;
    }
    AddProcessPendingLocked(sig, info);
    for (ThreadSignals* thread : *g_threads) {
      if (thread != &self && !thread->wait_is_sigwait && (thread->wait_set & bit) != 0) {
        thread->wait_set = 0;
        if (thread->wake_event != nullptr) SetEvent(thread->wake_event);
        return;
      }
    }
    deliver_here = (self.mask & bit) == 0;
  }
  if (deliver_here) DeliverPendingSignals();
}

int SendToThread(std::uint32_t tid, int sig, const PendingInfo& info) noexcept {
  ThreadSignals& self = Self();
  if (tid == self.tid) {
    if (sig == 0) return 0;
    {
      StateLock lock;
      self.pending |= SignalBit(sig);
      self.pending_info[sig] = info;
    }
    if ((self.mask & SignalBit(sig)) == 0) DeliverPendingSignals();
    return 0;
  }
  bool fatal = false;
  {
    StateLock lock;
    EnsureTablesLocked();
    ThreadSignals* target = nullptr;
    for (ThreadSignals* thread : *g_threads) {
      if (thread->tid == tid) target = thread;
    }
    if (target == nullptr) {
      HANDLE handle = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
      if (handle == nullptr) return guest::kESRCH;
      const DWORD owner = GetProcessIdOfThread(handle);
      DWORD exit_code = 0;
      const bool alive = GetExitCodeThread(handle, &exit_code) && exit_code == STILL_ACTIVE;
      CloseHandle(handle);
      if (owner != GetCurrentProcessId() || !alive) return guest::kESRCH;
      if (sig == 0) return 0;
      try {
        g_early_pending->push_back(EarlyPending{tid, sig, info});
      } catch (...) {
        return guest::kEAGAIN;
      }
      fatal = g_actions[sig].handler == kSigDfl && !DefaultIgnored(sig);
    } else {
      if (sig == 0) return 0;
      target->pending |= SignalBit(sig);
      target->pending_info[sig] = info;
      if ((target->wait_set & SignalBit(sig)) != 0) {
        target->wait_set = 0;
        if (target->wake_event != nullptr) SetEvent(target->wake_event);
      }
      fatal = g_actions[sig].handler == kSigDfl && !DefaultIgnored(sig) && (target->mask & SignalBit(sig)) == 0;
    }
  }
  if (fatal) TerminateForSignal(sig);
  return 0;
}

bool ProcessExists(std::int32_t pid) noexcept {
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (process == nullptr) return GetLastError() == ERROR_ACCESS_DENIED;
  DWORD code = 0;
  const bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
  CloseHandle(process);
  return alive;
}

int WaitForSignal(Sigset set, std::int64_t timeout_ns, PendingInfo* out) noexcept {
  ThreadSignals& self = Self();
  HANDLE event = WakeEvent(self);
  const std::int64_t deadline = timeout_ns >= 0 ? MonotonicNanoseconds() + timeout_ns : INT64_MAX;
  for (;;) {
    {
      StateLock lock;
      const Sigset available = (self.pending | g_process_pending.load()) & set;
      if (available != 0) {
        const int sig = LowestSignal(available);
        if (TakePendingLocked(self, sig, out)) return sig;
      }
      if (event == nullptr) return 0;
      self.wait_set = set;
      self.wait_is_sigwait = true;
    }
    DWORD wait_ms = INFINITE;
    if (timeout_ns >= 0) {
      const std::int64_t remaining = deadline - MonotonicNanoseconds();
      if (remaining <= 0) {
        StateLock lock;
        self.wait_set = 0;
        self.wait_is_sigwait = false;
        const Sigset available = (self.pending | g_process_pending.load()) & set;
        if (available != 0) {
          const int sig = LowestSignal(available);
          if (TakePendingLocked(self, sig, out)) return sig;
        }
        return 0;
      }
      const std::int64_t milliseconds = (remaining + 999999) / 1000000;
      wait_ms = milliseconds >= 0x7fffffff ? 0x7fffffff : static_cast<DWORD>(milliseconds);
    }
    WaitForSingleObject(event, wait_ms);
    StateLock lock;
    self.wait_set = 0;
    self.wait_is_sigwait = false;
  }
}

void SuspendWithMask(Sigset mask) noexcept {
  ThreadSignals& self = Self();
  HANDLE event = WakeEvent(self);
  const Sigset saved = self.mask;
  self.mask = mask & ~kUnblockable;
  for (;;) {
    if (DeliverPendingSignals() > 0) break;
    {
      StateLock lock;
      const Sigset deliverable = (self.pending | g_process_pending.load()) & ~self.mask;
      if (deliverable != 0) continue;
      self.wait_set = ~self.mask;
      self.wait_is_sigwait = false;
    }
    if (event != nullptr) {
      WaitForSingleObject(event, INFINITE);
    } else {
      Sleep(10);
    }
    StateLock lock;
    self.wait_set = 0;
  }
  self.mask = saved;
}

constexpr std::uint64_t kFrameMagic = 0x454c58534947464dull;
constexpr std::uint64_t kUserEflags = 0x50DD5;

struct XStateRecord {
  std::uint32_t feature;
  std::uint32_t length;
  std::uint32_t offset;
  std::uint32_t present;
};

struct alignas(16) SignalFrame {
  void* pretcode;
  guest::Ucontext uc;
  guest::Siginfo info;
  std::uint64_t magic;
  SignalFrame* self;
  void* handler;
  std::uint64_t saved_eflags;
  std::uint32_t xstate_count;
  std::uint32_t xstate_bytes;
  XStateRecord xstate[4];
};
static_assert(offsetof(SignalFrame, uc) == 8, "ucontext must sit at rsp+8 (libgcc/libunwind)");
static_assert(offsetof(SignalFrame, info) == 824, "trampoline hard-codes siginfo at +824");
static_assert(sizeof(SignalFrame) < kStagingBytes, "staging buffer too small");

struct Fault {
  int sig = 0;
  int code = 0;
  std::uint64_t address = 0;
  std::int64_t trapno = 0;
  std::int64_t err = 0;
  std::int64_t cr2 = 0;
  std::uint64_t guest_rip = 0;
  bool stack_overflow = false;
};

int FloatCodeFromMxcsr(std::uint32_t mxcsr) noexcept {
  const std::uint32_t raised = mxcsr & 0x3f & ~((mxcsr >> 7) & 0x3f);
  if (raised & 0x01) return guest::kFPE_FLTINV;
  if (raised & 0x04) return guest::kFPE_FLTDIV;
  if (raised & 0x08) return guest::kFPE_FLTOVF;
  if (raised & 0x10) return guest::kFPE_FLTUND;
  if (raised & 0x20) return guest::kFPE_FLTRES;
  if (raised & 0x02) return guest::kFPE_FLTINV;
  return guest::kFPE_FLTUNK;
}

bool ClassifyFault(const EXCEPTION_RECORD* record, const CONTEXT* context, Fault* fault) noexcept {
  using namespace guest;
  const std::uint64_t rip = context->Rip;
  fault->guest_rip = rip;
  switch (record->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_STACK_OVERFLOW: {
      const bool overflow = record->ExceptionCode == EXCEPTION_STACK_OVERFLOW;
      const std::uint64_t access = record->NumberParameters >= 1 ? record->ExceptionInformation[0] : 0;
      std::uint64_t address = record->NumberParameters >= 2 ? record->ExceptionInformation[1] : context->Rsp;
      fault->sig = kSIGSEGV;
      fault->stack_overflow = overflow;
      if (!overflow && address == ~std::uint64_t{0}) {
        fault->code = kSI_KERNEL;
        fault->address = 0;
        fault->trapno = 13;
        return true;
      }
      MEMORY_BASIC_INFORMATION region{};
      const bool committed = VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) != 0 &&
                             region.State == MEM_COMMIT;
      fault->code = committed && !overflow ? kSEGV_ACCERR : kSEGV_MAPERR;
      fault->address = address;
      fault->cr2 = static_cast<std::int64_t>(address);
      fault->trapno = 14;
      fault->err = 4 | (access == 1 ? 2 : 0) | (fault->code == kSEGV_ACCERR ? 1 : 0) | (access == 8 ? 16 : 0);
      return true;
    }
    case EXCEPTION_IN_PAGE_ERROR:
      fault->sig = kSIGBUS;
      fault->code = kBUS_ADRERR;
      fault->address = record->NumberParameters >= 2 ? record->ExceptionInformation[1] : 0;
      fault->cr2 = static_cast<std::int64_t>(fault->address);
      fault->trapno = 14;
      return true;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
      fault->sig = kSIGBUS;
      fault->code = kBUS_ADRALN;
      fault->trapno = 17;
      return true;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      fault->sig = kSIGILL;
      fault->code = kILL_ILLOPN;
      fault->address = rip;
      fault->trapno = 6;
      return true;
    case EXCEPTION_PRIV_INSTRUCTION:
      fault->sig = kSIGSEGV;
      fault->code = kSI_KERNEL;
      fault->trapno = 13;
      return true;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
      fault->sig = kSIGFPE;
      fault->code = kFPE_INTDIV;
      fault->address = rip;
      fault->trapno = 0;
      return true;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_STACK_CHECK:
    case 0xC00002B4:
    case 0xC00002B5:
      fault->sig = kSIGFPE;
      switch (record->ExceptionCode) {
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: fault->code = kFPE_FLTDIV; break;
        case EXCEPTION_FLT_OVERFLOW: fault->code = kFPE_FLTOVF; break;
        case EXCEPTION_FLT_UNDERFLOW: fault->code = kFPE_FLTUND; break;
        case EXCEPTION_FLT_INEXACT_RESULT: fault->code = kFPE_FLTRES; break;
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_FLT_DENORMAL_OPERAND:
        case EXCEPTION_FLT_STACK_CHECK: fault->code = kFPE_FLTINV; break;
        default: fault->code = FloatCodeFromMxcsr(context->MxCsr); break;
      }
      fault->address = rip;
      fault->trapno = 19;
      return true;
    case EXCEPTION_BREAKPOINT: {
      fault->sig = kSIGTRAP;
      fault->code = kSI_KERNEL;
      fault->trapno = 3;
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(rip);
      if (bytes[0] == 0xCC) {
        fault->guest_rip = rip + 1;
      } else if (bytes[0] == 0xCD && bytes[1] == 0x03) {
        fault->guest_rip = rip + 2;
      }
      return true;
    }
    case EXCEPTION_SINGLE_STEP:
      fault->sig = kSIGTRAP;
      fault->code = (context->Dr6 & 0xF) != 0 ? 4 : 2 ;
      fault->address = rip;
      fault->trapno = 1;
      return true;
    default: return false;
  }
}

std::uint32_t SaveXState(CONTEXT* context, SignalFrame* frame, unsigned char* data, std::size_t capacity) noexcept {
  frame->xstate_count = 0;
  frame->xstate_bytes = 0;
  if ((context->ContextFlags & CONTEXT_XSTATE) != CONTEXT_XSTATE) return 0;
  DWORD64 mask = 0;
  if (!GetXStateFeaturesMask(context, &mask)) return 0;
  static const DWORD kFeatures[] = {XSTATE_AVX, XSTATE_AVX512_KMASK, XSTATE_AVX512_ZMM_H, XSTATE_AVX512_ZMM};
  std::uint32_t used = 0;
  for (DWORD feature : kFeatures) {
    if ((g_enabled_xstate & (DWORD64{1} << feature)) == 0) continue;
    DWORD length = 0;
    void* source = LocateXStateFeature(context, feature, &length);
    if (source == nullptr || length == 0 || used + length > capacity) continue;
    XStateRecord& record = frame->xstate[frame->xstate_count++];
    record.feature = feature;
    record.length = length;
    record.offset = used;
    record.present = (mask >> feature) & 1;
    if (record.present != 0) std::memcpy(data + used, source, length);
    used += length;
  }
  frame->xstate_bytes = used;
  return used;
}

void RestoreXState(CONTEXT* context, const SignalFrame* frame) noexcept {
  if (frame->xstate_count == 0 || (context->ContextFlags & CONTEXT_XSTATE) != CONTEXT_XSTATE) return;
  DWORD64 mask = 0;
  if (!GetXStateFeaturesMask(context, &mask)) return;
  const auto* data = reinterpret_cast<const unsigned char*>(frame + 1);
  for (std::uint32_t i = 0; i < frame->xstate_count && i < 4; ++i) {
    const XStateRecord& record = frame->xstate[i];
    DWORD length = 0;
    void* target = LocateXStateFeature(context, record.feature, &length);
    if (target == nullptr) continue;
    if (record.present != 0) {
      std::memcpy(target, data + record.offset, length < record.length ? length : record.length);
      mask |= DWORD64{1} << record.feature;
    } else {
      mask &= ~(DWORD64{1} << record.feature);
    }
  }
  SetXStateFeaturesMask(context, mask);
}

}
}

extern "C" ELIXIR_SYSV ELIXIR_ASM_CALLED void* elixir_core_signal_install(void* target_address) noexcept {
  using namespace elixir::bionic::core;
  elixir::platform::ActivateGuestThreadPointer();
  ThreadSignals& self = t_signals;
  const std::size_t size = self.staged_size;
  auto base = reinterpret_cast<std::uintptr_t>(target_address);
  for (std::uintptr_t page = (base + size - 1) & ~std::uintptr_t{4095}; page > base; page -= 4096) {
    *reinterpret_cast<volatile char*>(page) = *reinterpret_cast<volatile char*>(page);
  }
  std::memcpy(target_address, t_staging, size);
  self.delivering = false;
  return static_cast<SignalFrame*>(target_address)->handler;
}

extern "C" ELIXIR_SYSV ELIXIR_NAKED void elixir_core_signal_trampoline() {
  __asm__ volatile(
      "subq $8, %rsp\n\t"
      "movq %rbx, %rdi\n\t"
      "callq elixir_core_signal_install\n\t"
      "addq $8, %rsp\n\t"
      "movq %rax, %r11\n\t"
      "leaq 8(%rbx), %rdx\n\t"
      "leaq 824(%rbx), %rsi\n\t"
      "movl 824(%rbx), %edi\n\t"
      "xorl %eax, %eax\n\t"
      "jmpq *%r11\n\t");
}

namespace elixir::bionic::core {
namespace {

bool StageFaultFrame(ThreadSignals& self, CONTEXT* context, const Fault& fault, const Action& action) noexcept {
  const std::uint64_t rsp = context->Rsp;
  const bool on_alt = OnAltStack(self, rsp);
  const bool use_alt = (action.flags & guest::kSA_ONSTACK) != 0 && self.alt_enabled && !on_alt;
  if (fault.stack_overflow && !use_alt) return false;

  auto* staged = reinterpret_cast<SignalFrame*>(t_staging);
  std::memset(staged, 0, sizeof(SignalFrame));
  unsigned char* extra = t_staging + sizeof(SignalFrame);
  const std::uint32_t extra_bytes = SaveXState(context, staged, extra, kStagingBytes - sizeof(SignalFrame));
  const std::uint64_t frame_size = sizeof(SignalFrame) + extra_bytes;

  const auto alt_low = reinterpret_cast<std::uint64_t>(self.alt_sp);
  const std::uint64_t top = use_alt ? alt_low + self.alt_size : rsp - 128;
  const std::uint64_t base = ((top - frame_size) & ~std::uint64_t{15}) - 8;
  if ((use_alt || on_alt) && base < alt_low) return false;
  auto* final_frame = reinterpret_cast<SignalFrame*>(base);

  staged->pretcode = reinterpret_cast<void*>(g_restorer);
  guest::Ucontext& uc = staged->uc;
  FillStackT(&uc.uc_stack, self, on_alt);
  FillGregs(&uc.uc_mcontext, *context);
  auto* gregs = uc.uc_mcontext.gregs;
  gregs[guest::kRegRip] = static_cast<std::int64_t>(fault.guest_rip);
  gregs[guest::kRegErr] = fault.err;
  gregs[guest::kRegTrapno] = fault.trapno;
  gregs[guest::kRegOldmask] = static_cast<std::int64_t>(self.mask);
  gregs[guest::kRegCr2] = fault.cr2;
  std::memcpy(uc.fpregs_mem, &context->FltSave, sizeof(uc.fpregs_mem));
  uc.uc_mcontext.fpregs = final_frame->uc.fpregs_mem;
  uc.uc_sigmask = self.mask;
  guest::Siginfo& info = staged->info;
  info.si_signo = fault.sig;
  info.si_code = fault.code;
  info.fields.fault.si_addr = reinterpret_cast<void*>(fault.address);
  staged->magic = kFrameMagic;
  staged->self = final_frame;
  staged->handler = action.handler;
  staged->saved_eflags = context->EFlags;

  Sigset blocked = action.mask;
  if ((action.flags & guest::kSA_NODEFER) == 0) blocked |= SignalBit(fault.sig);
  self.mask = (self.mask | blocked) & ~kUnblockable;
  if ((action.flags & static_cast<std::int32_t>(guest::kSA_RESETHAND)) != 0) {
    StateLock lock;
    g_actions[fault.sig] = Action{};
  }
  self.staged_size = static_cast<std::uint32_t>(frame_size);
  self.delivering = true;

  context->Rsp = base;
  context->Rbx = base;
  context->Rip = reinterpret_cast<DWORD64>(&elixir_core_signal_trampoline);
  context->EFlags &= ~DWORD{0x100 | 0x400};
  context->MxCsr = 0x1F80;
  context->FltSave.MxCsr = 0x1F80;
  context->FltSave.ControlWord = 0x037F;
  context->FltSave.StatusWord = 0;
  context->FltSave.TagWord = 0;
  g_delivered_faults.fetch_add(1, std::memory_order_relaxed);
  return true;
}

bool CompleteSigreturn(CONTEXT* context) noexcept {
  auto* frame = reinterpret_cast<SignalFrame*>(context->Rsp - 8);
  MEMORY_BASIC_INFORMATION region{};
  if (VirtualQuery(frame, &region, sizeof(region)) == 0 || region.State != MEM_COMMIT) return false;
  if (frame->magic != kFrameMagic || frame->self != frame) return false;
  const auto* g = frame->uc.uc_mcontext.gregs;
  using namespace guest;
  context->R8 = static_cast<DWORD64>(g[kRegR8]);
  context->R9 = static_cast<DWORD64>(g[kRegR9]);
  context->R10 = static_cast<DWORD64>(g[kRegR10]);
  context->R11 = static_cast<DWORD64>(g[kRegR11]);
  context->R12 = static_cast<DWORD64>(g[kRegR12]);
  context->R13 = static_cast<DWORD64>(g[kRegR13]);
  context->R14 = static_cast<DWORD64>(g[kRegR14]);
  context->R15 = static_cast<DWORD64>(g[kRegR15]);
  context->Rdi = static_cast<DWORD64>(g[kRegRdi]);
  context->Rsi = static_cast<DWORD64>(g[kRegRsi]);
  context->Rbp = static_cast<DWORD64>(g[kRegRbp]);
  context->Rbx = static_cast<DWORD64>(g[kRegRbx]);
  context->Rdx = static_cast<DWORD64>(g[kRegRdx]);
  context->Rax = static_cast<DWORD64>(g[kRegRax]);
  context->Rcx = static_cast<DWORD64>(g[kRegRcx]);
  context->Rsp = static_cast<DWORD64>(g[kRegRsp]);
  context->Rip = static_cast<DWORD64>(g[kRegRip]);
  context->EFlags = static_cast<DWORD>((frame->saved_eflags & ~kUserEflags) |
                                       (static_cast<std::uint64_t>(g[kRegEfl]) & kUserEflags));
  const void* fpregs = frame->uc.uc_mcontext.fpregs;
  if (fpregs != nullptr) {
    std::memcpy(&context->FltSave, fpregs, sizeof(context->FltSave));
    context->FltSave.MxCsr &= 0xFFBF;
    context->MxCsr = context->FltSave.MxCsr;
  }
  RestoreXState(context, frame);
  t_signals.mask = frame->uc.uc_sigmask & ~kUnblockable;
  return true;
}

LONG CALLBACK SignalVectoredHandler(EXCEPTION_POINTERS* pointers) {
  EXCEPTION_RECORD* record = pointers->ExceptionRecord;
  CONTEXT* context = pointers->ContextRecord;
  if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && g_restorer != 0 && context->Rip == g_restorer) {
    return CompleteSigreturn(context) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
  }
  Fault fault;
  if (!ClassifyFault(record, context, &fault)) return EXCEPTION_CONTINUE_SEARCH;
  if (platform::CurrentGuestThread() == nullptr) return EXCEPTION_CONTINUE_SEARCH;
  ThreadSignals& self = t_signals;
  if (self.delivering || self.holds_lock) return EXCEPTION_CONTINUE_SEARCH;
  const FaultFilter filter = g_fault_filter.load(std::memory_order_acquire);
  if (filter != nullptr) {
    if (!filter(static_cast<std::uintptr_t>(context->Rip))) return EXCEPTION_CONTINUE_SEARCH;
  } else {
    for (const ModuleRange& module : g_system_modules) {
      if (context->Rip >= module.begin && context->Rip < module.end) return EXCEPTION_CONTINUE_SEARCH;
    }
  }
  if ((self.mask & SignalBit(fault.sig)) != 0) return EXCEPTION_CONTINUE_SEARCH;
  Action action;
  {
    AcquireSRWLockShared(&g_state_lock);
    action = g_actions[fault.sig];
    ReleaseSRWLockShared(&g_state_lock);
  }
  if (action.handler == kSigDfl || action.handler == kSigIgn) return EXCEPTION_CONTINUE_SEARCH;
  return StageFaultFrame(self, context, fault, action) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

std::once_flag g_restorer_once;

void EnsureRestorerPage() noexcept {
  std::call_once(g_restorer_once, [] {
    void* page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (page == nullptr) return;
    static const unsigned char kRestoreRt[] = {0x48, 0xc7, 0xc0, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x05};
    std::memcpy(page, kRestoreRt, sizeof(kRestoreRt));
    DWORD old_protection = 0;
    VirtualProtect(page, 4096, PAGE_READONLY, &old_protection);
    g_restorer = reinterpret_cast<std::uint64_t>(page);
  });
}

void InstallFaultHandling() noexcept {
  EnsureRestorerPage();
  std::call_once(g_veh_once, [] {
    g_enabled_xstate = GetEnabledXStateFeatures();
    const wchar_t* const kSystemModules[] = {L"ntdll.dll", L"kernelbase.dll", L"kernel32.dll"};
    for (int i = 0; i < 3; ++i) {
      HMODULE module = GetModuleHandleW(kSystemModules[i]);
      MODULEINFO information{};
      if (module != nullptr && GetModuleInformation(GetCurrentProcess(), module, &information, sizeof(information))) {
        g_system_modules[i].begin = reinterpret_cast<std::uintptr_t>(information.lpBaseOfDll);
        g_system_modules[i].end = g_system_modules[i].begin + information.SizeOfImage;
      }
    }
    AddVectoredExceptionHandler(0, SignalVectoredHandler);
  });
}

bool IsFaultSignal(int sig) noexcept {
  using namespace guest;
  return sig == kSIGSEGV || sig == kSIGBUS || sig == kSIGILL || sig == kSIGFPE || sig == kSIGTRAP;
}

int SetAction(int sig, const guest::Sigaction* new_action, guest::Sigaction* old_action) noexcept {
  if (!ValidSignal(sig)) return guest::kEINVAL;
  guest::Sigaction incoming{};
  const bool installs_handler = new_action != nullptr && new_action->sa_handler != kSigDfl &&
                                new_action->sa_handler != kSigIgn;
  if (new_action != nullptr) {
    if (sig == guest::kSIGKILL || sig == guest::kSIGSTOP) return guest::kEINVAL;
    std::memcpy(&incoming, new_action, sizeof(incoming));
  }
  if (installs_handler) {
    EnsureRestorerPage();
    if (IsFaultSignal(sig)) InstallFaultHandling();
  }
  Action previous;
  bool now_ignored = false;
  {
    StateLock lock;
    previous = g_actions[sig];
    if (new_action != nullptr) {
      Action next;
      next.handler = incoming.sa_handler;
      next.flags = incoming.sa_flags;
      next.mask = incoming.sa_mask & ~kUnblockable;
      next.restorer = incoming.sa_restorer;
      if ((next.flags & guest::kSA_RESTORER) == 0 && next.handler != kSigDfl && next.handler != kSigIgn) {
        next.flags |= guest::kSA_RESTORER;
        next.restorer = reinterpret_cast<void*>(g_restorer);
      }
      g_actions[sig] = next;
      now_ignored = IsIgnoredDisposition(sig, next);
      if (now_ignored) {
        g_process_pending.fetch_and(~SignalBit(sig));
        if (g_rt_queue != nullptr) g_rt_queue[sig].clear();
        t_signals.pending &= ~SignalBit(sig);
      }
    }
  }
  if (old_action != nullptr) {
    guest::Sigaction out{};
    out.sa_flags = previous.flags;
    out.sa_handler = previous.handler;
    out.sa_mask = previous.mask;
    out.sa_restorer = previous.restorer;
    std::memcpy(old_action, &out, sizeof(out));
  }
  return 0;
}

Sigset FilterReservedSignals(Sigset set, int how) noexcept {
  constexpr int kKernelSigRtMin = 32;
  Sigset reserved_unblocked = 0;
  for (int sig = kKernelSigRtMin + 1; sig <= kKernelSigRtMin + 9; ++sig) reserved_unblocked |= SignalBit(sig);
  if (how == kSigUnblock) {
    set &= ~SignalBit(kKernelSigRtMin);
    set |= reserved_unblocked;
  } else {
    set |= SignalBit(kKernelSigRtMin);
    set &= ~reserved_unblocked;
  }
  return set;
}

int ChangeMask(int how, const Sigset* set, Sigset* old) noexcept {
  ThreadSignals& self = Self();
  Sigset requested = 0;
  if (set != nullptr) {
    std::memcpy(&requested, set, sizeof(requested));
    if (how != kSigBlock && how != kSigUnblock && how != kSigSetmask) return guest::kEINVAL;
    requested = FilterReservedSignals(requested, how);
  }
  if (old != nullptr) {
    const Sigset current = self.mask;
    std::memcpy(old, &current, sizeof(current));
  }
  if (set == nullptr) return 0;
  switch (how) {
    case kSigBlock: self.mask |= requested; break;
    case kSigUnblock: self.mask &= ~requested; break;
    default: self.mask = requested; break;
  }
  self.mask &= ~kUnblockable;
  DeliverPendingSignals();
  return 0;
}

void* Signal(int sig, void* handler, int flags) noexcept {
  guest::Sigaction action{};
  action.sa_handler = handler;
  action.sa_flags = flags;
  guest::Sigaction old{};
  const int error = SetAction(sig, &action, &old);
  if (error != 0) return FailWith(error, kSigErr);
  return old.sa_handler;
}

}

Sigset CurrentSignalMask() noexcept { return t_signals.mask; }

void SetSignalMask(Sigset mask) noexcept {
  ThreadSignals& self = Self();
  self.mask = mask & ~kUnblockable;
  DeliverPendingSignals();
}

int RaiseOnCurrentThread(int sig, int si_code) noexcept {
  if (sig < 0 || sig > kMaxSignal) return guest::kEINVAL;
  if (sig == 0) return 0;
  return SendToThread(Self().tid, sig, MakeInfo(si_code));
}

void GenerateProcessSignal(int sig, int si_code, std::uint64_t si_value) noexcept {
  if (!ValidSignal(sig)) return;
  SendToProcess(sig, MakeInfo(si_code, si_value));
}

void ResetSignalToDefault(int sig) noexcept {
  if (!ValidSignal(sig)) return;
  StateLock lock;
  g_actions[sig] = Action{};
}

[[noreturn]] void TerminateForSignal(int sig) noexcept {
  char buffer[64];
  char message[160];
  std::snprintf(message, sizeof(message), "Fatal signal %d (SIG%s): %s", sig,
                sig >= 1 && sig <= guest::kSIGSYS && kSignalNames[sig] != nullptr ? kSignalNames[sig] : "RT",
                DescribeSignal(sig, buffer, sizeof(buffer)));
  HostLog(7, "libc", message);
  std::fflush(nullptr);
  if (DefaultDumpsCore(sig)) RaiseFailFastException(nullptr, nullptr, 0);
  TerminateProcess(GetCurrentProcess(), static_cast<UINT>(128 + sig));
  ExitProcess(static_cast<UINT>(128 + sig));
}

void WriteHostStderr(const char* text, std::size_t length) noexcept {
  HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return;
  DWORD written = 0;
  WriteFile(handle, text, static_cast<DWORD>(length), &written, nullptr);
}

void SetFaultDeliveryFilter(FaultFilter filter) noexcept { g_fault_filter.store(filter, std::memory_order_release); }
std::uint64_t DeliveredFaultCount() noexcept { return g_delivered_faults.load(std::memory_order_relaxed); }
int RaiseGuestSignal(int sig) noexcept { return RaiseOnCurrentThread(sig, guest::kSI_TKILL); }

}

namespace elixir::bionic {
namespace {
using core::Sigset;
using core::kUnblockable;

const char* g_sys_siglist[core::kMaxSignal + 1];
const char* g_sys_signame[core::kMaxSignal + 1];

int SetBit(Sigset* set, int sig, bool value) noexcept {
  if (set == nullptr || sig < 1 || sig > core::kMaxSignal) return FailWith(guest::kEINVAL, -1);
  Sigset bits;
  std::memcpy(&bits, set, sizeof(bits));
  if (value) {
    bits |= core::SignalBit(sig);
  } else {
    bits &= ~core::SignalBit(sig);
  }
  std::memcpy(set, &bits, sizeof(bits));
  return 0;
}

}

ELIXIR_BIONIC_FN(int, sigemptyset)(Sigset* set) noexcept {
  if (set == nullptr) return FailWith(guest::kEINVAL, -1);
  *set = 0;
  return 0;
}
ELIXIR_BIONIC_FN(int, sigfillset)(Sigset* set) noexcept {
  if (set == nullptr) return FailWith(guest::kEINVAL, -1);
  *set = core::kAllSignals;
  return 0;
}
ELIXIR_BIONIC_FN(int, sigaddset)(Sigset* set, int sig) noexcept { return SetBit(set, sig, true); }
ELIXIR_BIONIC_FN(int, sigdelset)(Sigset* set, int sig) noexcept { return SetBit(set, sig, false); }
ELIXIR_BIONIC_FN(int, sigismember)(const Sigset* set, int sig) noexcept {
  if (set == nullptr || sig < 1 || sig > core::kMaxSignal) return FailWith(guest::kEINVAL, -1);
  return (*set & core::SignalBit(sig)) != 0 ? 1 : 0;
}

ELIXIR_BIONIC_FN(int, sigaction)(int sig, const guest::Sigaction* action, guest::Sigaction* old) noexcept {
  const int error = core::SetAction(sig, action, old);
  return error == 0 ? 0 : FailWith(error, -1);
}

ELIXIR_BIONIC_FN(void*, signal)(int sig, void* handler) noexcept {
  return core::Signal(sig, handler, guest::kSA_RESTART);
}
ELIXIR_BIONIC_FN(void*, bsd_signal)(int sig, void* handler) noexcept {
  return core::Signal(sig, handler, guest::kSA_RESTART);
}

ELIXIR_BIONIC_FN(int, siginterrupt)(int sig, int flag) noexcept {
  guest::Sigaction action{};
  if (core::SetAction(sig, nullptr, &action) != 0) return FailWith(guest::kEINVAL, -1);
  if (flag != 0) {
    action.sa_flags &= ~guest::kSA_RESTART;
  } else {
    action.sa_flags |= guest::kSA_RESTART;
  }
  const int error = core::SetAction(sig, &action, nullptr);
  return error == 0 ? 0 : FailWith(error, -1);
}

ELIXIR_BIONIC_FN(void*, sigset)(int sig, void* disposition) noexcept {
  if (sig < 1 || sig > core::kMaxSignal || sig == guest::kSIGKILL || sig == guest::kSIGSTOP) {
    return FailWith(guest::kEINVAL, core::kSigErr);
  }
  guest::Sigaction old{};
  core::SetAction(sig, nullptr, &old);
  const bool was_blocked = (core::CurrentSignalMask() & core::SignalBit(sig)) != 0;
  void* previous = was_blocked ? core::kSigHold : old.sa_handler;
  if (disposition == core::kSigHold) {
    core::SetSignalMask(core::CurrentSignalMask() | core::SignalBit(sig));
    return previous;
  }
  guest::Sigaction action{};
  action.sa_handler = disposition;
  const int error = core::SetAction(sig, &action, nullptr);
  if (error != 0) return FailWith(error, core::kSigErr);
  core::SetSignalMask(core::CurrentSignalMask() & ~core::SignalBit(sig));
  return previous;
}

ELIXIR_BIONIC_FN(int, sighold)(int sig) noexcept {
  if (sig < 1 || sig > core::kMaxSignal) return FailWith(guest::kEINVAL, -1);
  core::SetSignalMask(core::CurrentSignalMask() | core::SignalBit(sig));
  return 0;
}
ELIXIR_BIONIC_FN(int, sigrelse)(int sig) noexcept {
  if (sig < 1 || sig > core::kMaxSignal) return FailWith(guest::kEINVAL, -1);
  core::SetSignalMask(core::CurrentSignalMask() & ~core::SignalBit(sig));
  return 0;
}
ELIXIR_BIONIC_FN(int, sigignore)(int sig) noexcept {
  guest::Sigaction action{};
  action.sa_handler = core::kSigIgn;
  const int error = core::SetAction(sig, &action, nullptr);
  return error == 0 ? 0 : FailWith(error, -1);
}

ELIXIR_BIONIC_FN(int, sigprocmask)(int how, const Sigset* set, Sigset* old) noexcept {
  const int error = core::ChangeMask(how, set, old);
  return error == 0 ? 0 : FailWith(error, -1);
}
ELIXIR_BIONIC_FN(int, pthread_sigmask)(int how, const Sigset* set, Sigset* old) noexcept {
  return core::ChangeMask(how, set, old);
}

ELIXIR_BIONIC_FN(int, sigblock)(int mask) noexcept {
  const Sigset old = core::CurrentSignalMask();
  core::SetSignalMask(old | static_cast<std::uint32_t>(mask));
  return static_cast<int>(old & 0xffffffffu);
}
ELIXIR_BIONIC_FN(int, sigsetmask)(int mask) noexcept {
  const Sigset old = core::CurrentSignalMask();
  core::SetSignalMask((old & ~Sigset{0xffffffffu}) | static_cast<std::uint32_t>(mask));
  return static_cast<int>(old & 0xffffffffu);
}

ELIXIR_BIONIC_FN(int, sigpending)(Sigset* set) noexcept {
  if (set == nullptr) return FailWith(guest::kEFAULT, -1);
  *set = core::t_signals.pending | core::g_process_pending.load();
  return 0;
}

ELIXIR_BIONIC_FN(int, sigsuspend)(const Sigset* mask) noexcept {
  if (mask == nullptr) return FailWith(guest::kEFAULT, -1);
  core::SuspendWithMask(*mask);
  return FailWith(guest::kEINTR, -1);
}

ELIXIR_BIONIC_FN(int, pause)() noexcept {
  core::SuspendWithMask(core::CurrentSignalMask());
  return FailWith(guest::kEINTR, -1);
}

ELIXIR_BIONIC_FN(int, sigpause)(int sig) noexcept {
  if (sig < 1 || sig > core::kMaxSignal) return FailWith(guest::kEINVAL, -1);
  core::SuspendWithMask(core::CurrentSignalMask() & ~core::SignalBit(sig));
  return FailWith(guest::kEINTR, -1);
}

ELIXIR_BIONIC_FN(int, sigwait)(const Sigset* set, int* sig) noexcept {
  if (set == nullptr || sig == nullptr) return guest::kEINVAL;
  core::PendingInfo info;
  const int received = core::WaitForSignal(*set, -1, &info);
  if (received == 0) return guest::kEINTR;
  *sig = received;
  return 0;
}

ELIXIR_BIONIC_FN(int, sigtimedwait)(const Sigset* set, guest::Siginfo* info, const guest::Timespec* timeout) noexcept {
  if (set == nullptr) return FailWith(guest::kEINVAL, -1);
  std::int64_t timeout_ns = -1;
  if (timeout != nullptr) {
    if (!core::ValidTimespec(*timeout)) return FailWith(guest::kEINVAL, -1);
    timeout_ns = core::TimespecToNanoseconds(*timeout);
  }
  core::PendingInfo pending;
  const int received = core::WaitForSignal(*set, timeout_ns, &pending);
  if (received == 0) return FailWith(guest::kEAGAIN, -1);
  if (info != nullptr) core::FillSiginfo(info, received, pending);
  return received;
}

ELIXIR_BIONIC_FN(int, sigwaitinfo)(const Sigset* set, guest::Siginfo* info) noexcept {
  return elixir_bionic_sigtimedwait(set, info, nullptr);
}

ELIXIR_BIONIC_FN(int, sigaltstack)(const guest::StackT* stack, guest::StackT* old) noexcept {
  core::ThreadSignals& self = core::Self();
  const bool on_alt = core::OnAltStack(self, core::CurrentStackPointer());
  guest::StackT incoming{};
  if (stack != nullptr) std::memcpy(&incoming, stack, sizeof(incoming));
  if (old != nullptr) {
    guest::StackT out{};
    core::FillStackT(&out, self, on_alt);
    if (self.alt_enabled && !on_alt) out.ss_flags = self.alt_flags;
    std::memcpy(old, &out, sizeof(out));
  }
  if (stack == nullptr) return 0;
  if (on_alt) return FailWith(guest::kEPERM, -1);
  const std::int32_t flags = incoming.ss_flags & ~core::kSsAutodisarm;
  if (flags != 0 && flags != guest::kSS_DISABLE) return FailWith(guest::kEINVAL, -1);
  if (flags == guest::kSS_DISABLE) {
    self.alt_enabled = false;
    self.alt_sp = nullptr;
    self.alt_size = 0;
    self.alt_flags = 0;
    return 0;
  }
  if (incoming.ss_size < static_cast<guest::g_size>(guest::kMINSIGSTKSZ)) return FailWith(guest::kENOMEM, -1);
  self.alt_sp = incoming.ss_sp;
  self.alt_size = incoming.ss_size;
  self.alt_flags = incoming.ss_flags & core::kSsAutodisarm;
  self.alt_enabled = true;
  return 0;
}

ELIXIR_BIONIC_FN(int, raise)(int sig) noexcept {
  const int error = core::RaiseOnCurrentThread(sig, guest::kSI_TKILL);
  return error == 0 ? 0 : FailWith(error, -1);
}

ELIXIR_BIONIC_FN(int, kill)(abi::g_pid pid, int sig) noexcept {
  if (sig < 0 || sig > core::kMaxSignal) return FailWith(guest::kEINVAL, -1);
  const abi::g_pid self = core::CurrentPid();
  if (pid == self || pid == 0 || pid == -self) {
    if (sig != 0) core::SendToProcess(sig, core::MakeInfo(guest::kSI_USER));
    return 0;
  }
  if (pid == -1 || pid < -1) return FailWith(guest::kESRCH, -1);
  if (!core::ProcessExists(pid)) return FailWith(guest::kESRCH, -1);
  return FailWith(guest::kEPERM, -1);
}

ELIXIR_BIONIC_FN(int, killpg)(abi::g_pid group, int sig) noexcept {
  if (group < 0) return FailWith(guest::kEINVAL, -1);
  return elixir_bionic_kill(-group, sig);
}

ELIXIR_BIONIC_FN(int, tgkill)(abi::g_pid tgid, abi::g_pid tid, int sig) noexcept {
  if (tgid <= 0 || tid <= 0 || sig < 0 || sig > core::kMaxSignal) return FailWith(guest::kEINVAL, -1);
  if (tgid != core::CurrentPid()) return FailWith(guest::kESRCH, -1);
  const int error = core::SendToThread(threads::WindowsThreadIdFromTid(tid), sig, core::MakeInfo(guest::kSI_TKILL));
  return error == 0 ? 0 : FailWith(error, -1);
}

ELIXIR_BIONIC_FN(int, sigqueue)(abi::g_pid pid, int sig, std::uint64_t value) noexcept {
  if (sig < 0 || sig > core::kMaxSignal) return FailWith(guest::kEINVAL, -1);
  if (pid == core::CurrentPid() || pid == 0) {
    if (sig != 0) core::SendToProcess(sig, core::MakeInfo(guest::kSI_QUEUE, value));
    return 0;
  }
  if (pid < 0 || !core::ProcessExists(pid)) return FailWith(guest::kESRCH, -1);
  return FailWith(guest::kEPERM, -1);
}

ELIXIR_BIONIC_FN(void, psignal)(int sig, const char* prefix) noexcept {
  char buffer[64];
  const char* description = core::DescribeSignal(sig, buffer, sizeof(buffer));
  char line[512];
  int length;
  if (prefix != nullptr && *prefix != '\0') {
    length = std::snprintf(line, sizeof(line), "%s: %s\n", prefix, description);
  } else {
    length = std::snprintf(line, sizeof(line), "%s\n", description);
  }
  if (length > 0) {
    const std::size_t size = static_cast<std::size_t>(length) < sizeof(line) ? static_cast<std::size_t>(length)
                                                                             : sizeof(line) - 1;
    core::WriteHostStderr(line, size);
  }
}

ELIXIR_BIONIC_FN(void, psiginfo)(const guest::Siginfo* info, const char* prefix) noexcept {
  elixir_bionic_psignal(info != nullptr ? info->si_signo : 0, prefix);
}


ELIXIR_BIONIC_FN(int, __libc_current_sigrtmin)() noexcept { return core::kSigRtMin; }
ELIXIR_BIONIC_FN(int, __libc_current_sigrtmax)() noexcept { return core::kSigRtMax; }

namespace core {

int SendGuestSignalToThread(std::int32_t tid, int sig) noexcept {
  if (tid <= 0 || sig < 0 || sig > kMaxSignal) return guest::kEINVAL;
  return SendToThread(threads::WindowsThreadIdFromTid(tid), sig, MakeInfo(guest::kSI_TKILL));
}

int QueueGuestSignalToThread(std::int32_t tid, int sig, std::uint64_t value) noexcept {
  if (tid <= 0 || sig < 0 || sig > kMaxSignal) return guest::kEINVAL;
  return SendToThread(threads::WindowsThreadIdFromTid(tid), sig, MakeInfo(guest::kSI_QUEUE, value));
}

void RegisterSignalExports(ExportTable& table) {
  for (int sig = 1; sig <= guest::kSIGSYS; ++sig) {
    g_sys_siglist[sig] = kSignalDescriptions[sig];
    g_sys_signame[sig] = kSignalNames[sig];
  }
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
#define ELIXIR_ADD_AS(alias, name) table.Add(kLibc, alias, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(sigemptyset);
  ELIXIR_ADD(sigfillset);
  ELIXIR_ADD(sigaddset);
  ELIXIR_ADD(sigdelset);
  ELIXIR_ADD(sigismember);
  ELIXIR_ADD_AS("sigemptyset64", sigemptyset);
  ELIXIR_ADD_AS("sigfillset64", sigfillset);
  ELIXIR_ADD_AS("sigaddset64", sigaddset);
  ELIXIR_ADD_AS("sigdelset64", sigdelset);
  ELIXIR_ADD_AS("sigismember64", sigismember);
  ELIXIR_ADD(sigaction);
  ELIXIR_ADD_AS("sigaction64", sigaction);
  ELIXIR_ADD(signal);
  ELIXIR_ADD(bsd_signal);
  ELIXIR_ADD(siginterrupt);
  ELIXIR_ADD(sigset);
  ELIXIR_ADD(sighold);
  ELIXIR_ADD(sigrelse);
  ELIXIR_ADD(sigignore);
  ELIXIR_ADD(sigprocmask);
  ELIXIR_ADD_AS("sigprocmask64", sigprocmask);
  ELIXIR_ADD(pthread_sigmask);
  ELIXIR_ADD_AS("pthread_sigmask64", pthread_sigmask);
  ELIXIR_ADD(sigblock);
  ELIXIR_ADD(sigsetmask);
  ELIXIR_ADD(sigpending);
  ELIXIR_ADD_AS("sigpending64", sigpending);
  ELIXIR_ADD(sigsuspend);
  ELIXIR_ADD_AS("sigsuspend64", sigsuspend);
  ELIXIR_ADD(pause);
  ELIXIR_ADD(sigpause);
  ELIXIR_ADD(sigwait);
  ELIXIR_ADD_AS("sigwait64", sigwait);
  ELIXIR_ADD(sigtimedwait);
  ELIXIR_ADD_AS("sigtimedwait64", sigtimedwait);
  ELIXIR_ADD(sigwaitinfo);
  ELIXIR_ADD_AS("sigwaitinfo64", sigwaitinfo);
  ELIXIR_ADD(sigaltstack);
  ELIXIR_ADD(raise);
  ELIXIR_ADD(kill);
  ELIXIR_ADD(killpg);
  ELIXIR_ADD(tgkill);
  ELIXIR_ADD(sigqueue);
  ELIXIR_ADD(psignal);
  ELIXIR_ADD(psiginfo);
  ELIXIR_ADD(__libc_current_sigrtmin);
  ELIXIR_ADD(__libc_current_sigrtmax);
#undef ELIXIR_ADD
#undef ELIXIR_ADD_AS
}

}
}
