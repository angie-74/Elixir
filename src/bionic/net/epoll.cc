// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"
#include "bionic/wake_trace.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace elixir::bionic::net::internal {
namespace {

int Fail(int error) {
  SetGuestErrno(error);
  return -1;
}

constexpr std::uint32_t kEventBits = guest::kEPOLLIN | guest::kEPOLLPRI | guest::kEPOLLOUT | guest::kEPOLLRDNORM |
                                     guest::kEPOLLRDBAND | guest::kEPOLLWRNORM | guest::kEPOLLWRBAND |
                                     guest::kEPOLLMSG | guest::kEPOLLRDHUP;
constexpr std::uint32_t kAlways = guest::kEPOLLERR | guest::kEPOLLHUP;
constexpr std::uint32_t kInBits = guest::kEPOLLIN | guest::kEPOLLRDNORM | guest::kEPOLLRDHUP | guest::kEPOLLPRI |
                                  guest::kEPOLLRDBAND;
constexpr std::uint32_t kOutBits = guest::kEPOLLOUT | guest::kEPOLLWRNORM | guest::kEPOLLWRBAND;
constexpr std::uint32_t kFlagET = guest::kEPOLLET;
constexpr std::uint32_t kFlagOneShot = guest::kEPOLLONESHOT;
constexpr int kMaxNesting = 4;

struct Interest {
  int fd = -1;
  Target target;
  std::uint32_t events = 0;
  std::uint64_t data = 0;
  std::uint64_t generation = 0;
  std::uint32_t reported = 0;
  std::uint32_t in_seq = 0;
  std::uint32_t out_seq = 0;
  std::uint64_t in_edges = 0;
  std::uint64_t out_edges = 0;
  std::int64_t available = -1;
  bool disabled = false;
  bool muted = false;
};

std::int64_t QueuedBytes(SOCKET s) {
  u_long pending = 0;
  if (ioctlsocket(s, FIONREAD, &pending) != 0) return -1;
  return static_cast<std::int64_t>(pending);
}

class Epoll final : public NetObject, public Waker, public std::enable_shared_from_this<Epoll> {
 public:
  Epoll() : NetObject(ObjectType::kEpoll) {}
  ~Epoll() override { PurgeBlockedWaiter(this); }

  bool Init() { return wake_.Open(); }

  void Wake() noexcept override { wake_.Raise(); }

  std::uint32_t Readiness() noexcept override;

  int Control(int op, int fd, const guest::EpollEvent* event);
  int Wait(guest::EpollEvent* events, int max_events, Deadline deadline);

  bool Reaches(const NetObject* target, int depth);

 private:
  struct Snapshot {
    int fd;
    Target target;
    std::uint32_t events;
    std::uint64_t data;
    std::uint64_t generation;
    std::uint32_t reported;
    std::uint32_t in_seq, out_seq;
    std::uint64_t in_edges, out_edges;
    std::int64_t available;
    bool muted;
    std::uint32_t ready = 0;
    bool alive = true;
  };

  int Scan(guest::EpollEvent* events, int max_events, int wait_ms_limit, bool consume, Deadline deadline);
  void ApplyEdgeRearm(Snapshot& snap);

  std::mutex lock_;
  std::map<int, Interest> interests_;
  std::uint64_t next_generation_ = 1;
  int rotation_ = -1;
  WakeFlag wake_;
  std::atomic<int> waiters_{0};
};

struct EpollScratch {
  std::vector<WSAPOLLFD> wsa;
  std::vector<std::size_t> wsa_owner;
  std::vector<WSAPOLLFD> sample;
  std::vector<std::size_t> sample_owner;
};

EpollScratch& ScratchFor(int depth) {
  thread_local EpollScratch scratch[kMaxNesting + 2];
  return scratch[depth < 0 ? 0 : (depth > kMaxNesting + 1 ? kMaxNesting + 1 : depth)];
}

thread_local int t_scan_depth = 0;

void Epoll::ApplyEdgeRearm(Snapshot& snap) {
  if ((snap.events & kFlagET) == 0 || snap.reported == 0) return;
  if (snap.target.sock != nullptr) {
    if ((snap.reported & kInBits) != 0 && snap.target.sock->in_seq.load(std::memory_order_acquire) != snap.in_seq) {
      snap.reported &= ~kInBits;
    }
    if ((snap.reported & kOutBits) != 0 &&
        snap.target.sock->out_seq.load(std::memory_order_acquire) != snap.out_seq) {
      snap.reported &= ~kOutBits;
    }
  } else if (snap.target.object != nullptr) {
    if ((snap.reported & kInBits) != 0 && snap.target.object->InEdges() != snap.in_edges) snap.reported &= ~kInBits;
    if ((snap.reported & kOutBits) != 0 && snap.target.object->OutEdges() != snap.out_edges) {
      snap.reported &= ~kOutBits;
    }
  }
}

int Epoll::Scan(guest::EpollEvent* events, int max_events, int wait_ms_limit, bool consume, Deadline deadline) {
  thread_local std::vector<Snapshot> snapshots_by_depth[kMaxNesting + 2];
  const int depth = t_scan_depth;
  std::vector<Snapshot>& snaps = snapshots_by_depth[depth > kMaxNesting + 1 ? kMaxNesting + 1 : depth];
  EpollScratch& scratch = ScratchFor(depth);
  struct DepthGuard {
    DepthGuard() { ++t_scan_depth; }
    ~DepthGuard() { --t_scan_depth; }
  } depth_guard;
  struct Clear {
    std::vector<Snapshot>& snaps;
    ~Clear() { snaps.clear(); }
  } clear{snaps};

  if (consume) wake_.Consume();
  int rotation = -1;
  {
    std::lock_guard<std::mutex> guard(lock_);
    snaps.clear();
    snaps.reserve(interests_.size());
    rotation = rotation_;
    auto first = interests_.upper_bound(rotation);
    for (std::size_t pass = 0; pass < 2; ++pass) {
      auto begin = pass == 0 ? first : interests_.begin();
      auto end = pass == 0 ? interests_.end() : first;
      for (auto it = begin; it != end; ++it) {
        const Interest& in = it->second;
        if (in.disabled) continue;
        Snapshot snap{in.fd,      in.target,  in.events,   in.data,      in.generation, in.reported,
                      in.in_seq,  in.out_seq, in.in_edges, in.out_edges, in.available,  in.muted};
        snaps.push_back(std::move(snap));
      }
    }
  }

  bool any_dead = false;
  for (Snapshot& snap : snaps) {
    Target current;
    if (!ResolveTarget(snap.fd, &current) || !SameTarget(current, snap.target)) {
      snap.alive = false;
      any_dead = true;
    }
  }
  if (any_dead) {
    std::lock_guard<std::mutex> guard(lock_);
    for (const Snapshot& snap : snaps) {
      if (snap.alive) continue;
      auto it = interests_.find(snap.fd);
      if (it != interests_.end() && it->second.generation == snap.generation) interests_.erase(it);
    }
  }

  scratch.wsa.clear();
  scratch.wsa_owner.clear();
  scratch.sample.clear();
  scratch.sample_owner.clear();
  int immediate = 0;
  bool needs_slice = false;
  for (std::size_t i = 0; i < snaps.size(); ++i) {
    Snapshot& snap = snaps[i];
    if (!snap.alive) continue;
    ApplyEdgeRearm(snap);
    const std::uint32_t want = (snap.events & kEventBits) | kAlways;
    const bool et = (snap.events & kFlagET) != 0;
    const std::uint32_t armed = et ? want & ~snap.reported : want;
    Target& target = snap.target;
    if (target.kind == vfs::FdKind::kSocket) {
      const SOCKET s = reinterpret_cast<SOCKET>(target.handle);
      const std::uint32_t fixed = FixedSocketReadiness(target.sock.get()) & armed;
      if (fixed != 0) {
        snap.ready |= fixed;
        ++immediate;
      }
      WSAPOLLFD pfd{};
      pfd.fd = s;
      if (snap.muted || (et && (snap.reported & kInBits) != 0)) {
        pfd.events = ToWsaEvents(want);
        scratch.sample.push_back(pfd);
        scratch.sample_owner.push_back(i);
      } else {
        pfd.events = ToWsaEvents(armed);
        scratch.wsa.push_back(pfd);
        scratch.wsa_owner.push_back(i);
      }
      continue;
    }
    if (target.kind == vfs::FdKind::kEvent && target.object != nullptr && target.object.get() == this) continue;
    bool slice = false;
    const std::uint32_t level = ObjectOrFileReadiness(target, want, &slice);
    if (et) {
      if ((level & kInBits) == 0) snap.reported &= ~kInBits;
      if ((level & kOutBits) == 0) snap.reported &= ~kOutBits;
    }
    const std::uint32_t now = level & (et ? want & ~snap.reported : want);
    if (now != 0) {
      snap.ready |= now;
      ++immediate;
      continue;
    }
    const std::uint32_t still_armed = et ? want & ~snap.reported : want;
    const SOCKET wait = WaitHandleFor(target, still_armed);
    if (wait != INVALID_SOCKET) {
      WSAPOLLFD pfd{};
      pfd.fd = wait;
      pfd.events = POLLRDNORM;
      scratch.wsa.push_back(pfd);
      scratch.wsa_owner.push_back(i);
    } else if (slice) {
      needs_slice = true;
    }
  }

  if (!scratch.sample.empty()) {
    WSAPoll(scratch.sample.data(), static_cast<ULONG>(scratch.sample.size()), 0);
    for (std::size_t k = 0; k < scratch.sample.size(); ++k) {
      const std::size_t owner = scratch.sample_owner[k];
      Snapshot& snap = snaps[owner];
      const short revents = scratch.sample[k].revents;
      const std::uint32_t want = (snap.events & kEventBits) | kAlways;
      const bool et = (snap.events & kFlagET) != 0;
      const std::uint32_t level = FromWsaRevents(revents, snap.target.sock.get());
      if (et && (snap.reported & kInBits) != 0) {
        bool rearm = (level & (guest::kEPOLLIN | guest::kEPOLLRDNORM)) == 0;
        if (!rearm && snap.target.sock != nullptr && snap.target.sock->phase.load() != SockPhase::kListening) {
          const std::int64_t queued = QueuedBytes(scratch.sample[k].fd);
          if (queued > snap.available && snap.available >= 0) rearm = true;
          if (queued >= 0) snap.available = queued;
        }
        if (rearm) snap.reported &= ~kInBits;
      }
      if (et && (level & kOutBits) == 0) snap.reported &= ~kOutBits;
      const std::uint32_t armed = et ? want & ~snap.reported : want;
      const std::uint32_t reportable = level & armed;
      if (reportable != 0) {
        snap.ready |= reportable;
        snap.muted = false;
        ++immediate;
      } else if (revents != 0) {
        snap.muted = true;
        needs_slice = true;
      } else {
        snap.muted = false;
        WSAPOLLFD pfd{};
        pfd.fd = scratch.sample[k].fd;
        pfd.events = ToWsaEvents(armed);
        scratch.wsa.push_back(pfd);
        scratch.wsa_owner.push_back(owner);
      }
    }
  }

  int wait_ms = immediate > 0 ? 0 : deadline.WaitMs();
  if (wait_ms_limit >= 0 && (wait_ms < 0 || wait_ms > wait_ms_limit)) wait_ms = wait_ms_limit;
  if (needs_slice && (wait_ms < 0 || wait_ms > kSliceMs)) wait_ms = kSliceMs;
  if (consume) {
    WSAPOLLFD pfd{};
    pfd.fd = wake_.get();
    pfd.events = POLLRDNORM;
    scratch.wsa.push_back(pfd);
    scratch.wsa_owner.push_back(SIZE_MAX);
  }
  if (!scratch.wsa.empty()) {
    if (wait_ms > 0) EnsureFineTimerResolution();
    if (WSAPoll(scratch.wsa.data(), static_cast<ULONG>(scratch.wsa.size()), wait_ms) < 0) {
      const int error = WSAGetLastError();
      return Fail(error == WSAENOBUFS ? guest::kENOMEM : ErrnoFromWinsock(error));
    }
    for (std::size_t k = 0; k < scratch.wsa.size(); ++k) {
      const std::size_t owner = scratch.wsa_owner[k];
      const short revents = scratch.wsa[k].revents;
      if (owner == SIZE_MAX || revents == 0) continue;
      Snapshot& snap = snaps[owner];
      const std::uint32_t want = (snap.events & kEventBits) | kAlways;
      const bool et = (snap.events & kFlagET) != 0;
      const std::uint32_t armed = et ? want & ~snap.reported : want;
      if (snap.target.kind == vfs::FdKind::kSocket) {
        const std::uint32_t got = FromWsaRevents(revents, snap.target.sock.get()) & armed;
        snap.ready |= got;
        if (got == 0) snap.muted = true;
      } else {
        bool ignored = false;
        const std::uint32_t level = ObjectOrFileReadiness(snap.target, want, &ignored) & armed;
        if (level == 0 && snap.target.object != nullptr) snap.target.object->Refresh();
        snap.ready |= level;
      }
    }
  } else if (wait_ms != 0) {
    if (wait_ms < 0) {
      Sleep(INFINITE);
    } else {
      SleepPreciseNs(static_cast<std::int64_t>(wait_ms) * 1000000);
    }
  }

  int count = 0;
  int last_fd = -1;
  for (Snapshot& snap : snaps) {
    if (!snap.alive || snap.ready == 0) continue;
    if (events != nullptr && count >= max_events) break;
    if (events != nullptr) {
      guest::EpollEvent out;
      out.events = snap.ready;
      out.data = snap.data;
      std::memcpy(&events[count], &out, sizeof(out));
    }
    ++count;
    last_fd = snap.fd;
    if (events == nullptr) break;
  }
  if (events != nullptr) {
    std::lock_guard<std::mutex> guard(lock_);
    int seen = 0;
    for (Snapshot& snap : snaps) {
      if (!snap.alive) continue;
      const bool emitted = snap.ready != 0 && seen < count;
      if (snap.ready != 0) ++seen;
      auto it = interests_.find(snap.fd);
      if (it == interests_.end() || it->second.generation != snap.generation) continue;
      Interest& in = it->second;
      if ((in.events & kFlagET) != 0) {
        in.reported &= snap.reported;
        in.available = snap.available;
      }
      in.muted = snap.muted && !emitted;
      if (!emitted) continue;
      if ((in.events & kFlagET) != 0) {
        in.reported |= snap.ready;
        if (in.target.sock != nullptr) {
          in.in_seq = in.target.sock->in_seq.load(std::memory_order_acquire);
          in.out_seq = in.target.sock->out_seq.load(std::memory_order_acquire);
          if ((snap.ready & kInBits) != 0 && in.target.sock->phase.load() != SockPhase::kListening) {
            in.available = QueuedBytes(reinterpret_cast<SOCKET>(in.target.handle));
          }
        } else if (in.target.object != nullptr) {
          in.in_edges = in.target.object->InEdges();
          in.out_edges = in.target.object->OutEdges();
        }
      }
      if ((in.events & kFlagOneShot) != 0) in.disabled = true;
    }
    if (count > 0) rotation_ = last_fd;
  }
  return count;
}

std::uint32_t Epoll::Readiness() noexcept {
  if (t_scan_depth > kMaxNesting) return 0;
  try {
    const int ready = Scan(nullptr, 0, 0, false, Deadline::Now());
    return ready > 0 ? (guest::kPOLLIN | guest::kPOLLRDNORM) : 0;
  } catch (...) {
    return 0;
  }
}

bool Epoll::Reaches(const NetObject* target, int depth) {
  if (depth > kMaxNesting) return true;
  std::vector<std::shared_ptr<Epoll>> children;
  {
    std::lock_guard<std::mutex> guard(lock_);
    for (const auto& [fd, in] : interests_) {
      if (in.target.object == nullptr || in.target.object->type() != ObjectType::kEpoll) continue;
      if (in.target.object.get() == target) return true;
      children.push_back(std::static_pointer_cast<Epoll>(in.target.object));
    }
  }
  for (const auto& child : children) {
    if (child->Reaches(target, depth + 1)) return true;
  }
  return false;
}

int Epoll::Control(int op, int fd, const guest::EpollEvent* event) {
  Target target;
  if (!ResolveTarget(fd, &target)) return guest::kEBADF;
  if (target.object.get() == this) return guest::kEINVAL;
  if (target.kind == vfs::FdKind::kFile || target.kind == vfs::FdKind::kDirectory ||
      target.kind == vfs::FdKind::kNull) {
    return guest::kEPERM;
  }
  guest::EpollEvent request{};
  if (op == guest::kEPOLL_CTL_ADD || op == guest::kEPOLL_CTL_MOD) {
    if (event == nullptr) return guest::kEFAULT;
    std::memcpy(&request, event, sizeof(request));
    if ((request.events & guest::kEPOLLEXCLUSIVE) != 0) {
      if (op == guest::kEPOLL_CTL_MOD) return guest::kEINVAL;
      const std::uint32_t allowed = guest::kEPOLLIN | guest::kEPOLLOUT | guest::kEPOLLERR | guest::kEPOLLHUP |
                                    guest::kEPOLLWAKEUP | guest::kEPOLLET | guest::kEPOLLEXCLUSIVE |
                                    guest::kEPOLLRDNORM | guest::kEPOLLWRNORM | guest::kEPOLLRDHUP;
      if ((request.events & ~allowed) != 0) return guest::kEINVAL;
      if (target.object != nullptr && target.object->type() == ObjectType::kEpoll) return guest::kEINVAL;
    }
    if (target.object != nullptr && target.object->type() == ObjectType::kEpoll && op == guest::kEPOLL_CTL_ADD) {
      auto nested = std::static_pointer_cast<Epoll>(target.object);
      if (nested->Reaches(this, 1)) return guest::kELOOP;
    }
  } else if (op != guest::kEPOLL_CTL_DEL) {
    return guest::kEINVAL;
  }

  const bool et = (request.events & kFlagET) != 0;
  std::shared_ptr<Waker> self = shared_from_this();
  {
    std::lock_guard<std::mutex> guard(lock_);
    auto it = interests_.find(fd);
    if (it != interests_.end() && !SameTarget(it->second.target, target)) {
      interests_.erase(it);
      it = interests_.end();
    }
    switch (op) {
      case guest::kEPOLL_CTL_ADD: {
        if (it != interests_.end()) return guest::kEEXIST;
        Interest in;
        in.fd = fd;
        in.target = target;
        in.events = request.events;
        in.data = request.data;
        in.generation = next_generation_++;
        interests_.emplace(fd, std::move(in));
        break;
      }
      case guest::kEPOLL_CTL_MOD: {
        if (it == interests_.end()) return guest::kENOENT;
        if ((it->second.events & guest::kEPOLLEXCLUSIVE) != 0) return guest::kEINVAL;
        it->second.events = request.events;
        it->second.data = request.data;
        it->second.generation = next_generation_++;
        it->second.reported = 0;
        it->second.available = -1;
        it->second.disabled = false;
        it->second.muted = false;
        break;
      }
      default: {
        if (it == interests_.end()) return guest::kENOENT;
        interests_.erase(it);
        break;
      }
    }
  }
  if (op == guest::kEPOLL_CTL_DEL || !et) {
    if (target.sock != nullptr) target.sock->watchers.Remove(this);
    if (target.object != nullptr) target.object->RemoveWatcher(this);
  } else {
    if (target.sock != nullptr) target.sock->watchers.Add(self);
    if (target.object != nullptr) target.object->AddWatcher(self);
  }
  if (waiters_.load(std::memory_order_acquire) > 0) wake_.Raise();
  return 0;
}

int Epoll::Wait(guest::EpollEvent* events, int max_events, Deadline deadline) {
  waiters_.fetch_add(1, std::memory_order_acq_rel);
  struct Leave {
    std::atomic<int>& waiters;
    ~Leave() { waiters.fetch_sub(1, std::memory_order_acq_rel); }
  } leave{waiters_};
  const BlockedWaiterScope interruptible(deadline.Expired() ? nullptr : static_cast<Waker*>(this));
  for (;;) {
    const int count = Scan(events, max_events, -1, true, deadline);
    if (count != 0) return count;
    if (deadline.Expired()) return 0;
  }
}

std::shared_ptr<Epoll> ResolveEpoll(int fd) {
  ObjectPtr object = ResolveObject(fd);
  if (object == nullptr) return nullptr;
  if (object->type() != ObjectType::kEpoll) {
    SetGuestErrno(guest::kEINVAL);
    return nullptr;
  }
  return std::static_pointer_cast<Epoll>(object);
}

int EpollCreate(int flags) {
  if ((flags & ~guest::kEPOLL_CLOEXEC) != 0) return Fail(guest::kEINVAL);
  auto epoll = std::make_shared<Epoll>();
  if (!epoll->Init()) return -1;
  return InstallObject(epoll, flags & guest::kEPOLL_CLOEXEC);
}

int EpollWait(int fd, guest::EpollEvent* events, int max_events, Deadline deadline) {
  if (max_events <= 0 || max_events > INT32_MAX / static_cast<int>(sizeof(guest::EpollEvent))) {
    return Fail(guest::kEINVAL);
  }
  if (events == nullptr) return Fail(guest::kEFAULT);
  auto epoll = ResolveEpoll(fd);
  if (epoll == nullptr) return -1;
  return epoll->Wait(events, max_events, deadline);
}

int TimespecDeadline(const guest::Timespec* ts, Deadline* out) {
  if (ts == nullptr) {
    *out = Deadline::Infinite();
    return 0;
  }
  if (ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000) return guest::kEINVAL;
  constexpr std::int64_t kMaxSeconds = INT64_MAX / 4000000000;
  *out = Deadline::FromNs(ts->tv_sec > kMaxSeconds ? INT64_MAX / 4 : ts->tv_sec * 1000000000 + ts->tv_nsec);
  return 0;
}

}

ELIXIR_BIONIC_FN(int, epoll_create)(int size) noexcept {
  return Guarded(-1, [&] { return size <= 0 ? Fail(guest::kEINVAL) : EpollCreate(0); });
}
ELIXIR_BIONIC_FN(int, epoll_create1)(int flags) noexcept {
  return Guarded(-1, [&] { return EpollCreate(flags); });
}
ELIXIR_BIONIC_FN(int, epoll_ctl)(int epfd, int op, int fd, const guest::EpollEvent* event) noexcept {
  return Guarded(-1, [&] {
    auto epoll = ResolveEpoll(epfd);
    if (epoll == nullptr) return -1;
    const int error = epoll->Control(op, fd, event);
    return error == 0 ? 0 : Fail(error);
  });
}

template <typename Fn>
int TracedEpollWait(Fn&& wait) {
  const std::int64_t traced_from = wake_trace::Enabled() ? wake_trace::NowNs() : 0;
  const int result = Guarded(-1, wait);
  if (traced_from != 0) wake_trace::RecordWaitEnd(wake_trace::Event::kIoReady, nullptr, traced_from);
  return result;
}

ELIXIR_BIONIC_FN(int, epoll_wait)(int epfd, guest::EpollEvent* events, int max_events, int timeout_ms) noexcept {
  return TracedEpollWait([&] { return EpollWait(epfd, events, max_events, Deadline::FromMs(timeout_ms)); });
}
ELIXIR_BIONIC_FN(int, epoll_pwait)(int epfd, guest::EpollEvent* events, int max_events, int timeout_ms,
                                   const guest::Sigset* mask) noexcept {
  return TracedEpollWait([&] { return EpollWait(epfd, events, max_events, Deadline::FromMs(timeout_ms)); });
}
ELIXIR_BIONIC_FN(int, epoll_pwait2)(int epfd, guest::EpollEvent* events, int max_events,
                                    const guest::Timespec* timeout, const guest::Sigset* mask) noexcept {
  return Guarded(-1, [&] {
    Deadline deadline = Deadline::Infinite();
    const int error = TimespecDeadline(timeout, &deadline);
    if (error != 0) return Fail(error);
    return EpollWait(epfd, events, max_events, deadline);
  });
}

void RegisterEpollExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("epoll_create", &elixir_bionic_epoll_create);
  add("epoll_create1", &elixir_bionic_epoll_create1);
  add("epoll_ctl", &elixir_bionic_epoll_ctl);
  add("epoll_wait", &elixir_bionic_epoll_wait);
  add("epoll_pwait", &elixir_bionic_epoll_pwait);
  add("epoll_pwait64", &elixir_bionic_epoll_pwait);
  add("epoll_pwait2", &elixir_bionic_epoll_pwait2);
  add("epoll_pwait2_64", &elixir_bionic_epoll_pwait2);
}

}
