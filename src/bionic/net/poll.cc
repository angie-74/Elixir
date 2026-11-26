// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"
#include "bionic/wake_trace.h"

#include <cstring>

namespace elixir::bionic::net::internal {

namespace {

int Fail(int error) {
  SetGuestErrno(error);
  return -1;
}

bool ConsoleInputReady(HANDLE handle) {
  DWORD count = 0;
  if (!GetNumberOfConsoleInputEvents(handle, &count) || count == 0) return false;
  INPUT_RECORD records[128];
  DWORD read = 0;
  if (!PeekConsoleInputW(handle, records, 128, &read)) return false;
  for (DWORD i = 0; i < read; ++i) {
    if (records[i].EventType == KEY_EVENT && records[i].Event.KeyEvent.bKeyDown &&
        records[i].Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
      return true;
    }
  }
  return false;
}

constexpr std::uint32_t kIn = guest::kPOLLIN | guest::kPOLLRDNORM;
constexpr std::uint32_t kOut = guest::kPOLLOUT | guest::kPOLLWRNORM;

}

bool ResolveTarget(int fd, Target* target) noexcept {
  constexpr auto kUnset = static_cast<vfs::FdKind>(0xff);
  vfs::FdKind kind = kUnset;
  void* handle = nullptr;
  const int saved = GuestErrno();
  try {
    handle = vfs::HandleOf(fd, &kind);
  } catch (...) {
  }
  SetGuestErrno(saved);
  *target = Target{};
  if (kind == kUnset) return false;
  target->fd = fd;
  target->kind = kind;
  target->handle = handle;
  target->valid = true;
  switch (kind) {
    case vfs::FdKind::kSocket:
      target->sock = LookupSocket(reinterpret_cast<SOCKET>(handle));
      break;
    case vfs::FdKind::kEvent:
      target->object = LookupObject(handle);
      break;
    case vfs::FdKind::kPipe:
    case vfs::FdKind::kConsole: {
      const int flags = GuestStatusFlags(fd);
      SetGuestErrno(saved);
      target->access_mode = flags < 0 ? guest::kO_RDONLY : (flags & guest::kO_ACCMODE);
      break;
    }
    default:
      break;
  }
  return true;
}

bool SameTarget(const Target& a, const Target& b) noexcept {
  if (a.kind != b.kind || a.handle != b.handle) return false;
  if ((a.sock == nullptr) != (b.sock == nullptr)) return false;
  if (a.sock != nullptr && a.sock->serial != b.sock->serial) return false;
  if ((a.object == nullptr) != (b.object == nullptr)) return false;
  if (a.object != nullptr && a.object->serial() != b.object->serial()) return false;
  return true;
}

short ToWsaEvents(std::uint32_t events) noexcept {
  short out = 0;
  if ((events & (guest::kPOLLIN | guest::kPOLLRDNORM)) != 0) out |= POLLRDNORM;
  if ((events & (guest::kPOLLPRI | guest::kPOLLRDBAND)) != 0) out |= POLLRDBAND;
  if ((events & (guest::kPOLLOUT | guest::kPOLLWRNORM | guest::kPOLLWRBAND)) != 0) out |= POLLWRNORM;
  return out;
}

std::uint32_t FromWsaRevents(short revents, const SocketState* state) noexcept {
  std::uint32_t out = 0;
  const bool stream = state == nullptr || state->type == guest::kSOCK_STREAM;
  if ((revents & POLLRDNORM) != 0) out |= kIn;
  if ((revents & POLLRDBAND) != 0) out |= guest::kPOLLPRI | guest::kPOLLRDBAND;
  if ((revents & POLLWRNORM) != 0) out |= kOut;
  if ((revents & POLLERR) != 0) out |= guest::kPOLLERR;
  if ((revents & POLLNVAL) != 0) out |= guest::kPOLLNVAL;
  if ((revents & POLLHUP) != 0) {
    if (stream) {
      out |= kIn | guest::kPOLLRDHUP;
      if ((revents & POLLERR) != 0 || (state != nullptr && state->shut_wr.load())) {
        out |= guest::kPOLLHUP | kOut;
      }
    } else {
      out |= guest::kPOLLHUP;
    }
  }
  if ((out & kOut) != 0 && state != nullptr && state->pair != nullptr &&
      !state->pair->Writable(1 - state->pair_end)) {
    out &= ~(kOut | guest::kPOLLWRBAND);
  }
  return out | FixedSocketReadiness(state);
}

std::uint32_t FixedSocketReadiness(const SocketState* state) noexcept {
  if (state == nullptr) return 0;
  std::uint32_t out = 0;
  if (state->shut_rd.load()) out |= kIn | guest::kPOLLRDHUP;
  if (state->shut_rd.load() && state->shut_wr.load()) out |= guest::kPOLLHUP;
  if (state->type == guest::kSOCK_STREAM && state->phase.load() == SockPhase::kFresh) {
    out |= kOut | guest::kPOLLHUP;
  }
  return out;
}

std::uint32_t ObjectOrFileReadiness(Target& target, std::uint32_t events, bool* needs_slice) noexcept {
  std::uint32_t ready = 0;
  switch (target.kind) {
    case vfs::FdKind::kEvent:
      if (target.object != nullptr) {
        ready = target.object->Readiness();
        const bool waitable = target.object->WaitSocket() != INVALID_SOCKET;
        const std::uint32_t wanted = events & (kIn | kOut | guest::kPOLLPRI);
        if ((ready & wanted) == 0 && (!waitable || (wanted & kIn) == 0)) *needs_slice = true;
      } else if (HookedReadiness(target.fd, events, &ready)) {
        if ((ready & (events | kAlwaysReported)) == 0) *needs_slice = true;
      }
      break;
    case vfs::FdKind::kFile:
    case vfs::FdKind::kDirectory:
    case vfs::FdKind::kNull:
      ready = kIn | kOut;
      break;
    case vfs::FdKind::kPipe: {
      const auto handle = static_cast<HANDLE>(target.handle);
      DWORD available = 0;
      if (target.access_mode == guest::kO_RDONLY) {
        if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) {
          ready = guest::kPOLLHUP;
        } else if (available > 0) {
          ready = kIn;
        } else {
          *needs_slice = true;
        }
      } else {
        if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) &&
            GetLastError() == ERROR_BROKEN_PIPE) {
          ready = guest::kPOLLERR;
        } else {
          ready = kOut;
        }
      }
      break;
    }
    case vfs::FdKind::kConsole:
      if (target.access_mode == guest::kO_RDONLY || target.access_mode == guest::kO_RDWR) {
        if (ConsoleInputReady(static_cast<HANDLE>(target.handle))) {
          ready = kIn;
        } else if ((events & kIn) != 0) {
          *needs_slice = true;
        }
        if (target.access_mode == guest::kO_RDWR) ready |= kOut;
      } else {
        ready = kOut;
      }
      break;
    case vfs::FdKind::kSocket:
      break;
  }
  return ready & (events | kAlwaysReported);
}

std::uint32_t FdPoll(void* handle, std::uint8_t kind, std::uint32_t events) noexcept {
  const auto fd_kind = static_cast<vfs::FdKind>(kind);
  if (fd_kind == vfs::FdKind::kSocket) {
    const auto s = reinterpret_cast<SOCKET>(handle);
    const SocketPtr state = LookupSocket(s);
    WSAPOLLFD pfd{};
    pfd.fd = s;
    pfd.events = ToWsaEvents(events);
    if (WSAPoll(&pfd, 1, 0) < 0) return guest::kPOLLNVAL;
    return FromWsaRevents(pfd.revents, state.get()) & (events | kAlwaysReported);
  }
  if (fd_kind == vfs::FdKind::kEvent) {
    const ObjectPtr object = LookupObject(handle);
    return object != nullptr ? object->Readiness() & (events | kAlwaysReported) : 0;
  }
  return 0;
}

SOCKET WaitHandleFor(const Target& target, std::uint32_t events) noexcept {
  if (target.kind == vfs::FdKind::kSocket) return reinterpret_cast<SOCKET>(target.handle);
  if (target.kind == vfs::FdKind::kEvent && target.object != nullptr && (events & kIn) != 0) {
    return target.object->WaitSocket();
  }
  return INVALID_SOCKET;
}

namespace {

enum class Slot : std::uint8_t { kSocket, kObject };

struct PollScratch {
  std::vector<Target> targets;
  std::vector<WSAPOLLFD> wsa;
  std::vector<std::pair<std::size_t, Slot>> owners;
  std::vector<WSAPOLLFD> sample;
  std::vector<std::size_t> sample_owner;
  std::vector<char> muted;
};

PollScratch& Scratch() {
  thread_local PollScratch scratch;
  return scratch;
}

}

int PollFds(guest::Pollfd* fds, std::uint64_t count, Deadline deadline) noexcept {
  try {
    if (count > 65536) return Fail(guest::kEINVAL);
    if (count > 0 && fds == nullptr) return Fail(guest::kEFAULT);
    PollScratch& scratch = Scratch();
    const std::size_t n = static_cast<std::size_t>(count);
    scratch.targets.resize(n);
    bool has_socket = false;
    for (std::size_t i = 0; i < n; ++i) {
      if (fds[i].fd >= 0) {
        ResolveTarget(fds[i].fd, &scratch.targets[i]);
        has_socket = has_socket || scratch.targets[i].kind == vfs::FdKind::kSocket;
      } else {
        scratch.targets[i] = Target{};
      }
    }
    InterruptFlag* interrupt = nullptr;
    if (has_socket && !deadline.Expired()) {
      interrupt = ThreadInterrupt();
      if (interrupt != nullptr) interrupt->ConsumeIfRaised();
    }
    const BlockedWaiterScope interruptible(interrupt);
    constexpr std::size_t kInterruptOwner = SIZE_MAX;
    scratch.muted.assign(n, 0);
    struct Clear {
      PollScratch& scratch;
      ~Clear() {
        scratch.targets.clear();
        scratch.wsa.clear();
        scratch.owners.clear();
        scratch.sample.clear();
        scratch.sample_owner.clear();
      }
    } clear{scratch};

    for (;;) {
      int ready = 0;
      bool needs_slice = false;
      scratch.wsa.clear();
      scratch.owners.clear();
      scratch.sample.clear();
      scratch.sample_owner.clear();
      for (std::size_t i = 0; i < n; ++i) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) continue;
        Target& target = scratch.targets[i];
        const auto events = static_cast<std::uint32_t>(static_cast<std::uint16_t>(fds[i].events));
        if (!target.valid) {
          fds[i].revents = static_cast<std::int16_t>(guest::kPOLLNVAL);
          ++ready;
          continue;
        }
        if (target.kind == vfs::FdKind::kSocket) {
          const std::uint32_t fixed = FixedSocketReadiness(target.sock.get()) & (events | kAlwaysReported);
          if (fixed != 0) {
            fds[i].revents = static_cast<std::int16_t>(fixed);
            ++ready;
          }
          WSAPOLLFD pfd{};
          pfd.fd = reinterpret_cast<SOCKET>(target.handle);
          pfd.events = ToWsaEvents(events);
          if (scratch.muted[i] != 0) {
            scratch.sample.push_back(pfd);
            scratch.sample_owner.push_back(i);
          } else {
            scratch.wsa.push_back(pfd);
            scratch.owners.emplace_back(i, Slot::kSocket);
          }
          continue;
        }
        const std::uint32_t now_ready = ObjectOrFileReadiness(target, events, &needs_slice);
        if (now_ready != 0) {
          fds[i].revents = static_cast<std::int16_t>(now_ready);
          ++ready;
          continue;
        }
        const SOCKET wait = WaitHandleFor(target, events);
        if (wait != INVALID_SOCKET) {
          WSAPOLLFD pfd{};
          pfd.fd = wait;
          pfd.events = POLLRDNORM;
          scratch.wsa.push_back(pfd);
          scratch.owners.emplace_back(i, Slot::kObject);
        }
      }

      if (!scratch.sample.empty()) {
        if (WSAPoll(scratch.sample.data(), static_cast<ULONG>(scratch.sample.size()), 0) < 0) {
          return Fail(ErrnoFromWinsock(WSAGetLastError()));
        }
        for (std::size_t k = 0; k < scratch.sample.size(); ++k) {
          const std::size_t i = scratch.sample_owner[k];
          const short revents = scratch.sample[k].revents;
          const auto events = static_cast<std::uint32_t>(static_cast<std::uint16_t>(fds[i].events));
          const std::uint32_t got = FromWsaRevents(revents, scratch.targets[i].sock.get()) & (events | kAlwaysReported);
          if (got != 0) {
            if (fds[i].revents == 0) ++ready;
            fds[i].revents = static_cast<std::int16_t>(static_cast<std::uint16_t>(fds[i].revents) | got);
          } else if (revents != 0) {
            needs_slice = true;
          } else {
            scratch.muted[i] = 0;
            WSAPOLLFD pfd = scratch.sample[k];
            pfd.revents = 0;
            scratch.wsa.push_back(pfd);
            scratch.owners.emplace_back(i, Slot::kSocket);
          }
        }
      }

      int wait_ms = ready > 0 ? 0 : deadline.WaitMs();
      if (needs_slice && (wait_ms < 0 || wait_ms > kSliceMs)) wait_ms = kSliceMs;
      if (interruptible.active() && wait_ms != 0) {
        WSAPOLLFD pfd{};
        pfd.fd = interrupt->get();
        pfd.events = POLLRDNORM;
        scratch.wsa.push_back(pfd);
        scratch.owners.emplace_back(kInterruptOwner, Slot::kSocket);
      }
      if (scratch.wsa.empty()) {
        if (ready > 0 || deadline.Expired()) return ready;
        if (wait_ms < 0) {
          Sleep(INFINITE);
          continue;
        }
        const std::int64_t left = deadline.RemainingNs();
        const std::int64_t nap = needs_slice && left > kSliceMs * 1000000ll ? kSliceMs * 1000000ll : left;
        SleepPreciseNs(nap);
        if (!needs_slice) return 0;
        continue;
      }
      if (wait_ms > 0) EnsureFineTimerResolution();
      const int result = WSAPoll(scratch.wsa.data(), static_cast<ULONG>(scratch.wsa.size()), wait_ms);
      if (result < 0) {
        const int error = WSAGetLastError();
        return Fail(error == WSAENOBUFS ? guest::kENOMEM : ErrnoFromWinsock(error));
      }
      for (std::size_t k = 0; k < scratch.wsa.size(); ++k) {
        const std::size_t i = scratch.owners[k].first;
        const short revents = scratch.wsa[k].revents;
        if (revents == 0) continue;
        if (i == kInterruptOwner) {
          interrupt->Consume();
          continue;
        }
        Target& target = scratch.targets[i];
        const auto events = static_cast<std::uint32_t>(static_cast<std::uint16_t>(fds[i].events));
        std::uint32_t got = 0;
        if (scratch.owners[k].second == Slot::kSocket) {
          got = FromWsaRevents(revents, target.sock.get()) & (events | kAlwaysReported);
          if (got == 0) scratch.muted[i] = 1;
        } else {
          bool ignored = false;
          got = ObjectOrFileReadiness(target, events, &ignored);
          if (got == 0) target.object->Refresh();
        }
        if (got != 0) {
          if (fds[i].revents == 0) ++ready;
          fds[i].revents = static_cast<std::int16_t>(static_cast<std::uint16_t>(fds[i].revents) | got);
        }
      }
      if (ready > 0 || deadline.Expired()) return ready;
    }
  } catch (const std::bad_alloc&) {
    return Fail(guest::kENOMEM);
  } catch (...) {
    return Fail(guest::kEIO);
  }
}

namespace {

int TimespecTimeout(const guest::Timespec* ts, Deadline* out) {
  if (ts == nullptr) {
    *out = Deadline::Infinite();
    return 0;
  }
  if (ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000) return guest::kEINVAL;
  constexpr std::int64_t kMaxSeconds = INT64_MAX / 4000000000;
  *out = Deadline::FromNs(ts->tv_sec > kMaxSeconds ? INT64_MAX / 4 : ts->tv_sec * 1000000000 + ts->tv_nsec);
  return 0;
}

bool FdIsSet(const GuestFdSet* set, int fd) {
  return set != nullptr && (set->bits[fd / 64] & (1ull << (fd % 64))) != 0;
}

int SelectCore(int nfds, GuestFdSet* readfds, GuestFdSet* writefds, GuestFdSet* exceptfds, Deadline deadline) {
  if (nfds < 0 || nfds > kFD_SETSIZE) return Fail(guest::kEINVAL);
  thread_local std::vector<guest::Pollfd> pollfds;
  pollfds.clear();
  for (int fd = 0; fd < nfds; ++fd) {
    std::int16_t events = 0;
    if (FdIsSet(readfds, fd)) events |= static_cast<std::int16_t>(guest::kPOLLIN | guest::kPOLLRDNORM);
    if (FdIsSet(writefds, fd)) events |= static_cast<std::int16_t>(guest::kPOLLOUT | guest::kPOLLWRNORM);
    if (FdIsSet(exceptfds, fd)) events |= static_cast<std::int16_t>(guest::kPOLLPRI);
    if (events != 0) pollfds.push_back(guest::Pollfd{fd, events, 0});
  }
  for (const auto& pfd : pollfds) {
    Target probe;
    if (!ResolveTarget(pfd.fd, &probe)) return Fail(guest::kEBADF);
  }
  const int polled = PollFds(pollfds.data(), pollfds.size(), deadline);
  if (polled < 0) return -1;
  const std::size_t words = static_cast<std::size_t>((nfds + 63) / 64);
  for (GuestFdSet* set : {readfds, writefds, exceptfds}) {
    if (set == nullptr) continue;
    for (std::size_t w = 0; w < words; ++w) set->bits[w] = 0;
  }
  int total = 0;
  for (const auto& pfd : pollfds) {
    const auto revents = static_cast<std::uint32_t>(static_cast<std::uint16_t>(pfd.revents));
    if ((revents & guest::kPOLLNVAL) != 0) return Fail(guest::kEBADF);
    const auto mark = [&](GuestFdSet* set) {
      set->bits[pfd.fd / 64] |= 1ull << (pfd.fd % 64);
      ++total;
    };
    if ((pfd.events & guest::kPOLLIN) != 0 &&
        (revents & (guest::kPOLLIN | guest::kPOLLRDNORM | guest::kPOLLRDBAND | guest::kPOLLHUP | guest::kPOLLERR)) != 0) {
      mark(readfds);
    }
    if ((pfd.events & guest::kPOLLOUT) != 0 &&
        (revents & (guest::kPOLLOUT | guest::kPOLLWRNORM | guest::kPOLLWRBAND | guest::kPOLLERR)) != 0) {
      mark(writefds);
    }
    if ((pfd.events & guest::kPOLLPRI) != 0 && (revents & guest::kPOLLPRI) != 0) mark(exceptfds);
  }
  return total;
}

int PollImpl(guest::Pollfd* fds, std::uint32_t count, Deadline deadline) {
  return PollFds(fds, count, deadline);
}

void CheckFdSet(const char* function, int fd, g_size set_size) {
  if (fd < 0) FortifyFatal(function, "file descriptor is negative");
  if (fd >= kFD_SETSIZE) FortifyFatal(function, "file descriptor >= FD_SETSIZE");
  if (set_size < sizeof(GuestFdSet)) FortifyFatal(function, "set is too small");
}

}

ELIXIR_BIONIC_FN(int, net_poll)(guest::Pollfd* fds, std::uint32_t count, int timeout_ms) noexcept {
  const std::int64_t traced_from = wake_trace::Enabled() ? wake_trace::NowNs() : 0;
  const int result = Guarded(-1, [&] { return PollImpl(fds, count, Deadline::FromMs(timeout_ms)); });
  if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kIoReady, nullptr, traced_from);
  return result;
}
ELIXIR_BIONIC_FN(int, net_ppoll)(guest::Pollfd* fds, std::uint32_t count, const guest::Timespec* timeout,
                             const guest::Sigset* mask) noexcept {
  return Guarded(-1, [&] {
    Deadline deadline = Deadline::Infinite();
    const int error = TimespecTimeout(timeout, &deadline);
    if (error != 0) return Fail(error);
    return PollImpl(fds, count, deadline);
  });
}
ELIXIR_BIONIC_FN(int, net_poll_chk)(guest::Pollfd* fds, std::uint32_t count, int timeout_ms, g_size fds_size) noexcept {
  if (fds_size / sizeof(guest::Pollfd) < count) FortifyFatal("poll", "fd count > fd buffer size");
  return elixir_bionic_net_poll(fds, count, timeout_ms);
}
ELIXIR_BIONIC_FN(int, net_ppoll_chk)(guest::Pollfd* fds, std::uint32_t count, const guest::Timespec* timeout,
                                   const guest::Sigset* mask, g_size fds_size) noexcept {
  if (fds_size / sizeof(guest::Pollfd) < count) FortifyFatal("ppoll", "fd count > fd buffer size");
  return elixir_bionic_net_ppoll(fds, count, timeout, mask);
}
ELIXIR_BIONIC_FN(int, select)(int nfds, GuestFdSet* readfds, GuestFdSet* writefds, GuestFdSet* exceptfds,
                              guest::Timeval* timeout) noexcept {
  const std::int64_t traced_from = wake_trace::Enabled() ? wake_trace::NowNs() : 0;
  struct Traced {
    std::int64_t from;
    ~Traced() {
      if (from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kIoReady, nullptr, from);
    }
  } traced{traced_from};
  return Guarded(-1, [&] {
    Deadline deadline = Deadline::Infinite();
    if (timeout != nullptr) {
      if (timeout->tv_sec < 0 || timeout->tv_usec < 0) return Fail(guest::kEINVAL);
      const std::int64_t seconds = timeout->tv_sec + timeout->tv_usec / 1000000;
      const std::int64_t ns = (timeout->tv_usec % 1000000) * 1000;
      deadline = Deadline::FromNs(seconds > INT64_MAX / 4000000000 ? INT64_MAX / 4 : seconds * 1000000000 + ns);
    }
    const int result = SelectCore(nfds, readfds, writefds, exceptfds, deadline);
    if (timeout != nullptr && result >= 0) {
      const std::int64_t left = deadline.RemainingNs();
      timeout->tv_sec = left / 1000000000;
      timeout->tv_usec = (left % 1000000000) / 1000;
    }
    return result;
  });
}
ELIXIR_BIONIC_FN(int, pselect)(int nfds, GuestFdSet* readfds, GuestFdSet* writefds, GuestFdSet* exceptfds,
                               const guest::Timespec* timeout, const guest::Sigset* mask) noexcept {
  return Guarded(-1, [&] {
    Deadline deadline = Deadline::Infinite();
    const int error = TimespecTimeout(timeout, &deadline);
    if (error != 0) return Fail(error);
    return SelectCore(nfds, readfds, writefds, exceptfds, deadline);
  });
}
ELIXIR_BIONIC_FN(void, __FD_SET_chk)(int fd, GuestFdSet* set, g_size set_size) noexcept {
  CheckFdSet("FD_SET", fd, set_size);
  set->bits[fd / 64] |= 1ull << (fd % 64);
}
ELIXIR_BIONIC_FN(void, __FD_CLR_chk)(int fd, GuestFdSet* set, g_size set_size) noexcept {
  CheckFdSet("FD_CLR", fd, set_size);
  set->bits[fd / 64] &= ~(1ull << (fd % 64));
}
ELIXIR_BIONIC_FN(int, __FD_ISSET_chk)(int fd, const GuestFdSet* set, g_size set_size) noexcept {
  CheckFdSet("FD_ISSET", fd, set_size);
  return (set->bits[fd / 64] & (1ull << (fd % 64))) != 0 ? 1 : 0;
}

void RegisterPollExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("poll", &elixir_bionic_net_poll);
  add("ppoll", &elixir_bionic_net_ppoll);
  add("ppoll64", &elixir_bionic_net_ppoll);
  add("__poll_chk", &elixir_bionic_net_poll_chk);
  add("__ppoll_chk", &elixir_bionic_net_ppoll_chk);
  add("__ppoll64_chk", &elixir_bionic_net_ppoll_chk);
  add("select", &elixir_bionic_select);
  add("pselect", &elixir_bionic_pselect);
  add("pselect64", &elixir_bionic_pselect);
  add("__FD_SET_chk", &elixir_bionic___FD_SET_chk);
  add("__FD_CLR_chk", &elixir_bionic___FD_CLR_chk);
  add("__FD_ISSET_chk", &elixir_bionic___FD_ISSET_chk);
}

}
