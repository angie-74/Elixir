// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <cstdio>
#include <shared_mutex>
#include <unordered_map>

namespace elixir::bionic::net {

bool EnsureWinsock() noexcept {
  static const bool started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  return started;
}

namespace internal {

std::atomic<int> g_gso_sockets{0};
std::atomic<int> g_pair_sockets{0};

namespace {

struct BlockedRegistry {
  std::mutex lock;
  std::vector<Waker*> waiters;
};

BlockedRegistry& Blocked() {
  static BlockedRegistry* registry = new BlockedRegistry();
  return *registry;
}

void ForgetCounters(const SocketState& state) noexcept {
  if (state.gso_size.load() != 0) g_gso_sockets.fetch_sub(1);
  if (state.pair != nullptr) g_pair_sockets.fetch_sub(1);
}

std::int64_t QpcFrequency() noexcept {
  static const std::int64_t frequency = [] {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    return static_cast<std::int64_t>(value.QuadPart);
  }();
  return frequency;
}

struct SocketRegistry {
  std::shared_mutex lock;
  std::unordered_map<SOCKET, SocketPtr> map;
};

SocketRegistry& Sockets() {
  static SocketRegistry* registry = new SocketRegistry();
  return *registry;
}

}

std::uint64_t NextSerial() noexcept {
  static std::atomic<std::uint64_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

std::int64_t MonotonicNs() noexcept {
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  const std::int64_t frequency = QpcFrequency();
  const std::int64_t ticks = counter.QuadPart;
  return (ticks / frequency) * 1000000000 + (ticks % frequency) * 1000000000 / frequency;
}

std::int64_t RealtimeNs() noexcept {
  FILETIME now;
  GetSystemTimePreciseAsFileTime(&now);
  const std::int64_t value =
      (static_cast<std::int64_t>(now.dwHighDateTime) << 32) | static_cast<std::int64_t>(now.dwLowDateTime);
  return (value - 116444736000000000) * 100;
}

void EnsureFineTimerResolution() noexcept {
  static std::once_flag once;
  std::call_once(once, [] {
    HMODULE winmm = LoadLibraryExW(L"winmm.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (winmm == nullptr) return;
    using TimeBeginPeriod = UINT(WINAPI*)(UINT);
    auto begin = reinterpret_cast<TimeBeginPeriod>(reinterpret_cast<void*>(GetProcAddress(winmm, "timeBeginPeriod")));
    if (begin != nullptr) begin(1);
  });
}

void SleepPreciseNs(std::int64_t ns) noexcept {
  if (ns <= 0) return;
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  if (timer != nullptr) {
    LARGE_INTEGER due;
    due.QuadPart = -((ns + 99) / 100);
    if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
      WaitForSingleObject(timer, INFINITE);
      CloseHandle(timer);
      return;
    }
    CloseHandle(timer);
  }
  EnsureFineTimerResolution();
  const std::int64_t deadline = MonotonicNs() + ns;
  for (;;) {
    const std::int64_t remaining = deadline - MonotonicNs();
    if (remaining <= 0) return;
    Sleep(static_cast<DWORD>((remaining + 999999) / 1000000));
  }
}

Deadline Deadline::FromMs(std::int64_t ms) {
  if (ms < 0) return Infinite();
  return FromNs(ms > INT64_MAX / 4000000 ? INT64_MAX / 4 : ms * 1000000);
}

Deadline Deadline::FromNs(std::int64_t ns) {
  if (ns < 0) return Infinite();
  if (ns > INT64_MAX / 4) ns = INT64_MAX / 4;
  return Deadline(MonotonicNs() + ns);
}

bool Deadline::Expired() const { return !infinite() && MonotonicNs() >= at_ns_; }

std::int64_t Deadline::RemainingNs() const {
  if (infinite()) return INT64_MAX;
  const std::int64_t left = at_ns_ - MonotonicNs();
  return left > 0 ? left : 0;
}

int Deadline::WaitMs() const {
  if (infinite()) return -1;
  const std::int64_t left = RemainingNs();
  if (left <= 0) return 0;
  const std::int64_t ms = (left + 999999) / 1000000;
  return ms > 0x7ffffff0 ? 0x7ffffff0 : static_cast<int>(ms);
}

std::wstring Utf8ToWide(const char* text) {
  if (text == nullptr || *text == '\0') return {};
  const int chars = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (chars <= 1) return {};
  std::wstring out(static_cast<std::size_t>(chars - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), chars);
  return out;
}

std::string WideToUtf8(const wchar_t* text) {
  if (text == nullptr || *text == L'\0') return {};
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (bytes <= 1) return {};
  std::string out(static_cast<std::size_t>(bytes - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), bytes, nullptr, nullptr);
  return out;
}

[[noreturn]] void FortifyFatal(const char* function, const char* what) noexcept {
  char message[256];
  std::snprintf(message, sizeof(message), "FORTIFY: %s: %s", function, what);
  HostLog(7 , "libc", message);
  TerminateProcess(GetCurrentProcess(), 128 + guest::kSIGABRT);
  __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

SocketPtr RegisterSocket(SOCKET s, int domain, int type, int protocol, int host_family) {
  auto state = std::make_shared<SocketState>();
  state->socket = s;
  state->serial = NextSerial();
  state->domain = domain;
  state->type = type;
  state->protocol = protocol;
  state->host_family = host_family;
  SocketRegistry& registry = Sockets();
  SocketPtr stale;
  {
    std::unique_lock<std::shared_mutex> guard(registry.lock);
    auto& slot = registry.map[s];
    stale = std::move(slot);
    slot = state;
  }
  if (stale != nullptr) ForgetCounters(*stale);
  return state;
}

SocketPtr LookupSocket(SOCKET s) noexcept {
  try {
    SocketRegistry& registry = Sockets();
    std::shared_lock<std::shared_mutex> guard(registry.lock);
    const auto it = registry.map.find(s);
    return it == registry.map.end() ? nullptr : it->second;
  } catch (...) {
    return nullptr;
  }
}

SocketPtr TakeSocket(SOCKET s) noexcept {
  try {
    SocketRegistry& registry = Sockets();
    SocketPtr state;
    {
      std::unique_lock<std::shared_mutex> guard(registry.lock);
      const auto it = registry.map.find(s);
      if (it == registry.map.end()) return nullptr;
      state = std::move(it->second);
      registry.map.erase(it);
    }
    ForgetCounters(*state);
    return state;
  } catch (...) {
    return nullptr;
  }
}

bool AddBlockedWaiter(Waker* waker) noexcept {
  try {
    BlockedRegistry& registry = Blocked();
    std::lock_guard<std::mutex> guard(registry.lock);
    registry.waiters.push_back(waker);
    return true;
  } catch (...) {
    return false;
  }
}

void RemoveBlockedWaiter(Waker* waker) noexcept {
  BlockedRegistry& registry = Blocked();
  std::lock_guard<std::mutex> guard(registry.lock);
  auto& list = registry.waiters;
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (list[i] == waker) {
      list[i] = list.back();
      list.pop_back();
      return;
    }
  }
}

void PurgeBlockedWaiter(Waker* waker) noexcept {
  BlockedRegistry& registry = Blocked();
  std::lock_guard<std::mutex> guard(registry.lock);
  auto& list = registry.waiters;
  for (std::size_t i = 0; i < list.size();) {
    if (list[i] == waker) {
      list[i] = list.back();
      list.pop_back();
    } else {
      ++i;
    }
  }
}

void WakeBlockedWaiters() noexcept {
  BlockedRegistry& registry = Blocked();
  std::lock_guard<std::mutex> guard(registry.lock);
  for (Waker* waker : registry.waiters) waker->Wake();
}

void NoteShutdown(SocketState* state) noexcept {
  if (state != nullptr) {
    state->in_seq.fetch_add(1, std::memory_order_release);
    state->out_seq.fetch_add(1, std::memory_order_release);
    state->watchers.Notify();
  }
  WakeBlockedWaiters();
}

InterruptFlag* ThreadInterrupt() noexcept {
  thread_local std::unique_ptr<InterruptFlag> flag;
  thread_local bool unavailable = false;
  if (flag == nullptr && !unavailable) {
    const int saved = GuestErrno();
    std::unique_ptr<InterruptFlag> fresh(new (std::nothrow) InterruptFlag());
    if (fresh != nullptr && fresh->Open()) {
      flag = std::move(fresh);
    } else {
      unavailable = true;
    }
    SetGuestErrno(saved);
  }
  return flag.get();
}

int PrepareHostSocket(SOCKET s, int host_family, int type) noexcept {
  u_long nonblocking = 1;
  if (ioctlsocket(s, FIONBIO, &nonblocking) != 0) return ErrnoFromWinsock(WSAGetLastError());
  if (host_family == AF_INET6) {
    DWORD v6only = 0;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&v6only), sizeof(v6only));
  }
  if (type == SOCK_DGRAM && (host_family == AF_INET || host_family == AF_INET6)) {
    BOOL report = FALSE;
    DWORD bytes = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &bytes, nullptr, nullptr);
    WSAIoctl(s, SIO_UDP_NETRESET, &report, sizeof(report), nullptr, 0, &bytes, nullptr, nullptr);
  }
  return 0;
}

int InstallSocket(SOCKET s, const SocketPtr& state, int flags) noexcept {
  int open_flags = guest::kO_RDWR;
  if ((flags & guest::kSOCK_NONBLOCK) != 0) open_flags |= guest::kO_NONBLOCK;
  if ((flags & guest::kSOCK_CLOEXEC) != 0) open_flags |= guest::kO_CLOEXEC;
  if (state != nullptr) state->nonblocking.store((flags & guest::kSOCK_NONBLOCK) != 0);
  int fd = -1;
  try {
    fd = vfs::InstallHandle(reinterpret_cast<void*>(s), vfs::FdKind::kSocket, open_flags);
  } catch (...) {
    SetGuestErrno(guest::kENOMEM);
    fd = -1;
  }
  if (fd < 0) {
    const int error = GuestErrno();
    TakeSocket(s);
    closesocket(s);
    SetGuestErrno(error != 0 ? error : guest::kEMFILE);
    return -1;
  }
  return fd;
}

bool ResolveSocket(int fd, SocketRef* out, bool want_state) noexcept {
  constexpr auto kUnset = static_cast<vfs::FdKind>(0xff);
  vfs::FdKind kind = kUnset;
  void* handle = nullptr;
  try {
    handle = vfs::HandleOf(fd, &kind);
  } catch (...) {
    handle = nullptr;
  }
  if (kind == kUnset) {
    SetGuestErrno(guest::kEBADF);
    return false;
  }
  if (kind != vfs::FdKind::kSocket || handle == nullptr) {
    SetGuestErrno(guest::kENOTSOCK);
    return false;
  }
  out->s = reinterpret_cast<SOCKET>(handle);
  if (want_state) {
    out->state = LookupSocket(out->s);
    if (out->state == nullptr) {
      WSAPROTOCOL_INFOW info{};
      int length = sizeof(info);
      int host_family = AF_INET;
      int type = SOCK_STREAM;
      int protocol = 0;
      if (getsockopt(out->s, SOL_SOCKET, SO_PROTOCOL_INFOW, reinterpret_cast<char*>(&info), &length) == 0) {
        host_family = info.iAddressFamily;
        type = info.iSocketType;
        protocol = info.iProtocol;
      }
      try {
        out->state = RegisterSocket(out->s, HostFamilyToGuest(host_family), type, protocol, host_family);
      } catch (...) {
        SetGuestErrno(guest::kENOMEM);
        return false;
      }
    }
  }
  return true;
}

bool FdIsNonBlocking(int fd, const SocketState* state) noexcept {
  const int saved = GuestErrno();
  const int flags = GuestStatusFlags(fd);
  SetGuestErrno(saved);
  if (flags < 0) return state != nullptr ? state->nonblocking.load() : true;
  return (flags & guest::kO_NONBLOCK) != 0;
}

int WaitSocket(SOCKET s, short wsa_events, std::int64_t timeout_us, const std::atomic<bool>* interrupted) noexcept {
  const Deadline deadline = timeout_us <= 0 ? Deadline::Infinite()
                            : timeout_us > INT64_MAX / 4000 ? Deadline::FromNs(INT64_MAX / 4)
                                                             : Deadline::FromNs(timeout_us * 1000);
  InterruptFlag* interrupt = interrupted != nullptr ? ThreadInterrupt() : nullptr;
  if (interrupt != nullptr) interrupt->ConsumeIfRaised();
  const BlockedWaiterScope scope(interrupt);
  for (;;) {
    if (interrupted != nullptr && interrupted->load()) return 0;
    WSAPOLLFD pfd[2] = {};
    pfd[0].fd = s;
    pfd[0].events = wsa_events;
    ULONG count = 1;
    if (scope.active()) {
      pfd[1].fd = interrupt->get();
      pfd[1].events = POLLRDNORM;
      count = 2;
    }
    const int wait = deadline.WaitMs();
    if (wait > 0) EnsureFineTimerResolution();
    const int result = WSAPoll(pfd, count, wait);
    if (result < 0) {
      const int error = WSAGetLastError();
      return error == WSAENOTSOCK ? guest::kEBADF : ErrnoFromWinsock(error);
    }
    if ((pfd[0].revents & POLLNVAL) != 0) return guest::kEBADF;
    if (pfd[0].revents != 0) return 0;
    if (count == 2 && pfd[1].revents != 0) interrupt->Consume();
    if (deadline.Expired()) return guest::kEAGAIN;
  }
}

int SocketErrno(int wsa_error, const SocketState* state, bool sending) noexcept {
  const bool datagram = state != nullptr && state->type == guest::kSOCK_DGRAM;
  switch (wsa_error) {
    case WSAEWOULDBLOCK:
      return guest::kEAGAIN;
    case WSAECONNRESET:
      return datagram ? guest::kECONNREFUSED : guest::kECONNRESET;
    case WSAESHUTDOWN:
      return sending ? guest::kEPIPE : guest::kESHUTDOWN;
    case WSAECONNABORTED:
      return sending ? guest::kEPIPE : guest::kECONNABORTED;
    case WSAENOTSOCK:
      return guest::kEBADF;
    case WSAENOTCONN:
      if (sending) {
        if (datagram) return guest::kEDESTADDRREQ;
        if (state != nullptr && state->phase.load() == SockPhase::kConnecting) return guest::kEAGAIN;
        return guest::kEPIPE;
      }
      return guest::kENOTCONN;
    default:
      return ErrnoFromWinsock(wsa_error);
  }
}

}

int FdClose(void* handle, std::uint8_t kind, int* guest_errno) noexcept {
  using namespace internal;
  try {
    const auto fd_kind = static_cast<vfs::FdKind>(kind);
    if (fd_kind == vfs::FdKind::kSocket) {
      const auto s = reinterpret_cast<SOCKET>(handle);
      SocketPtr state = TakeSocket(s);
      if (closesocket(s) != 0) {
        const int error = WSAGetLastError();
        if (guest_errno != nullptr) *guest_errno = error == WSAENOTSOCK ? guest::kEBADF : ErrnoFromWinsock(error);
        return -1;
      }
      if (state != nullptr) {
        if (state->pair != nullptr) state->pair->Close(state->pair_end);
        std::wstring cleanup;
        {
          std::lock_guard<std::mutex> guard(state->name_lock);
          cleanup = state->unix_cleanup;
        }
        if (!cleanup.empty()) DeleteFileW(cleanup.c_str());
      }
      return 0;
    }
    if (fd_kind == vfs::FdKind::kEvent) {
      ObjectPtr object = TakeObject(handle);
      if (object != nullptr) object->Close();
      if (handle != nullptr) CloseHandle(static_cast<HANDLE>(handle));
      return 0;
    }
    if (guest_errno != nullptr) *guest_errno = guest::kEBADF;
    return -1;
  } catch (...) {
    if (guest_errno != nullptr) *guest_errno = guest::kEIO;
    return -1;
  }
}

int FdSetNonBlocking(void* handle, std::uint8_t kind, bool nonblocking, int* guest_errno) noexcept {
  using namespace internal;
  const auto fd_kind = static_cast<vfs::FdKind>(kind);
  if (fd_kind == vfs::FdKind::kSocket) {
    SocketPtr state = LookupSocket(reinterpret_cast<SOCKET>(handle));
    if (state != nullptr) state->nonblocking.store(nonblocking);
    return 0;
  }
  if (fd_kind == vfs::FdKind::kEvent) {
    ObjectPtr object = LookupObject(handle);
    if (object != nullptr) object->nonblocking.store(nonblocking);
    return 0;
  }
  if (guest_errno != nullptr) *guest_errno = guest::kEBADF;
  return -1;
}

}
