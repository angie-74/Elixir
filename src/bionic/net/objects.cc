// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <shared_mutex>
#include <unordered_map>

namespace elixir::bionic::net::internal {
namespace {

struct ObjectRegistry {
  std::shared_mutex lock;
  std::unordered_map<void*, ObjectPtr> map;
};

ObjectRegistry& Objects() {
  static ObjectRegistry* registry = new ObjectRegistry();
  return *registry;
}

int Fail(int error) {
  SetGuestErrno(error);
  return -1;
}

constexpr std::uint64_t kEventFdMax = 0xfffffffffffffffeull;

}

SignalSocket::~SignalSocket() {
  if (socket_ != INVALID_SOCKET) closesocket(socket_);
}

bool SignalSocket::Open() noexcept {
  if (!EnsureWinsock()) {
    SetGuestErrno(guest::kENETDOWN);
    return false;
  }
  SOCKET s = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
  if (s == INVALID_SOCKET) {
    SetGuestErrno(ErrnoFromWinsock(WSAGetLastError()));
    return false;
  }
  sockaddr_in self{};
  self.sin_family = AF_INET;
  self.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
  int length = sizeof(self);
  u_long nonblocking = 1;
  BOOL report = FALSE;
  DWORD bytes = 0;
  if (bind(s, reinterpret_cast<sockaddr*>(&self), sizeof(self)) != 0 ||
      getsockname(s, reinterpret_cast<sockaddr*>(&self), &length) != 0 ||
      connect(s, reinterpret_cast<sockaddr*>(&self), sizeof(self)) != 0 || ioctlsocket(s, FIONBIO, &nonblocking) != 0) {
    SetGuestErrno(ErrnoFromWinsock(WSAGetLastError()));
    closesocket(s);
    return false;
  }
  WSAIoctl(s, SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &bytes, nullptr, nullptr);
  socket_ = s;
  return true;
}

bool SignalSocket::Post() noexcept {
  const char token = 1;
  if (send(socket_, &token, 1, 0) == 1) return true;
  return WSAGetLastError() == WSAEWOULDBLOCK;
}

int SignalSocket::Drain() noexcept {
  int drained = 0;
  char buffer[16];
  while (recv(socket_, buffer, sizeof(buffer), 0) >= 0) ++drained;
  return drained;
}

void WakeFlag::Raise() noexcept {
  if (!raised_.exchange(true, std::memory_order_seq_cst)) signal_.Post();
}

void WakeFlag::Consume() noexcept {
  signal_.Drain();
  raised_.store(false, std::memory_order_seq_cst);
}

void WakeFlag::ConsumeIfRaised() noexcept {
  if (raised_.load(std::memory_order_seq_cst)) Consume();
}

NetObject::NetObject(ObjectType type) : type_(type), serial_(NextSerial()) {}

NetObject::~NetObject() = default;

g_ssize NetObject::Read(void*, std::size_t, bool, int* err) noexcept {
  *err = guest::kEINVAL;
  return -1;
}

g_ssize NetObject::Write(const void*, std::size_t, bool, int* err) noexcept {
  *err = guest::kEINVAL;
  return -1;
}

void WatcherList::Add(const std::shared_ptr<Waker>& waker) {
  std::lock_guard<std::mutex> guard(lock_);
  for (auto it = list_.begin(); it != list_.end();) {
    auto live = it->lock();
    if (live == nullptr) {
      it = list_.erase(it);
    } else {
      if (live == waker) return;
      ++it;
    }
  }
  list_.push_back(waker);
  count_.store(static_cast<int>(list_.size()), std::memory_order_release);
}

void WatcherList::Remove(const Waker* waker) {
  std::lock_guard<std::mutex> guard(lock_);
  for (auto it = list_.begin(); it != list_.end();) {
    auto live = it->lock();
    if (live == nullptr || live.get() == waker) {
      it = list_.erase(it);
    } else {
      ++it;
    }
  }
  count_.store(static_cast<int>(list_.size()), std::memory_order_release);
}

void WatcherList::Notify() noexcept {
  if (count_.load(std::memory_order_acquire) == 0) return;
  try {
    std::shared_ptr<Waker> live[8];
    std::size_t found = 0;
    std::vector<std::shared_ptr<Waker>> extra;
    {
      std::lock_guard<std::mutex> guard(lock_);
      for (const auto& weak : list_) {
        auto strong = weak.lock();
        if (strong == nullptr) continue;
        if (found < 8) {
          live[found++] = std::move(strong);
        } else {
          extra.push_back(std::move(strong));
        }
      }
    }
    for (std::size_t i = 0; i < found; ++i) live[i]->Wake();
    for (const auto& waker : extra) waker->Wake();
  } catch (...) {
  }
}

int InstallObject(const ObjectPtr& object, int flags) noexcept {
  HANDLE token = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (token == nullptr) return Fail(ErrnoFromWin32(GetLastError()));
  object->set_token(token);
  object->nonblocking.store((flags & guest::kO_NONBLOCK) != 0);
  ObjectPtr stale;
  try {
    ObjectRegistry& registry = Objects();
    std::unique_lock<std::shared_mutex> guard(registry.lock);
    auto& slot = registry.map[token];
    stale = std::move(slot);
    slot = object;
  } catch (...) {
    CloseHandle(token);
    return Fail(guest::kENOMEM);
  }
  if (stale != nullptr) stale->Close();
  int fd = -1;
  try {
    fd = vfs::InstallHandle(token, vfs::FdKind::kEvent,
                            guest::kO_RDWR | (flags & (guest::kO_NONBLOCK | guest::kO_CLOEXEC)));
  } catch (...) {
    fd = -1;
    SetGuestErrno(guest::kENOMEM);
  }
  if (fd < 0) {
    const int error = GuestErrno();
    TakeObject(token);
    object->Close();
    CloseHandle(token);
    return Fail(error != 0 ? error : guest::kEMFILE);
  }
  return fd;
}

ObjectPtr LookupObject(void* handle) noexcept {
  try {
    ObjectRegistry& registry = Objects();
    std::shared_lock<std::shared_mutex> guard(registry.lock);
    const auto it = registry.map.find(handle);
    return it == registry.map.end() ? nullptr : it->second;
  } catch (...) {
    return nullptr;
  }
}

ObjectPtr TakeObject(void* handle) noexcept {
  try {
    ObjectRegistry& registry = Objects();
    std::unique_lock<std::shared_mutex> guard(registry.lock);
    const auto it = registry.map.find(handle);
    if (it == registry.map.end()) return nullptr;
    ObjectPtr object = std::move(it->second);
    registry.map.erase(it);
    return object;
  } catch (...) {
    return nullptr;
  }
}

ObjectPtr ResolveObject(int fd) noexcept {
  constexpr auto kUnset = static_cast<vfs::FdKind>(0xff);
  vfs::FdKind kind = kUnset;
  void* handle = nullptr;
  try {
    handle = vfs::HandleOf(fd, &kind);
  } catch (...) {
  }
  if (kind == kUnset) {
    SetGuestErrno(guest::kEBADF);
    return nullptr;
  }
  ObjectPtr object = kind == vfs::FdKind::kEvent ? LookupObject(handle) : nullptr;
  if (object == nullptr) SetGuestErrno(guest::kEINVAL);
  return object;
}

namespace {

bool ObjectFdNonBlocking(int fd, const NetObject& object) {
  const int saved = GuestErrno();
  const int flags = GuestStatusFlags(fd);
  SetGuestErrno(saved);
  return flags < 0 ? object.nonblocking.load() : (flags & guest::kO_NONBLOCK) != 0;
}

class EventFd final : public NetObject {
 public:
  EventFd() : NetObject(ObjectType::kEventFd) {}

  bool Init(std::uint64_t initial, bool semaphore) {
    if (!signal_.Open()) return false;
    semaphore_ = semaphore;
    counter_ = initial;
    if (counter_ > 0) queued_ = signal_.Post();
    return true;
  }

  std::uint32_t Readiness() noexcept override {
    std::lock_guard<std::mutex> guard(lock_);
    std::uint32_t ready = 0;
    if (counter_ > 0) ready |= guest::kPOLLIN | guest::kPOLLRDNORM;
    if (counter_ < kEventFdMax) ready |= guest::kPOLLOUT | guest::kPOLLWRNORM;
    return ready;
  }

  SOCKET WaitSocket() const noexcept override { return signal_.get(); }

  void Refresh() noexcept override { DrainStale(); }

  g_ssize Read(void* buffer, std::size_t size, bool nonblocking, int* err) noexcept override {
    if (size < sizeof(std::uint64_t)) {
      *err = guest::kEINVAL;
      return -1;
    }
    std::uint64_t value = 0;
    {
      std::unique_lock<std::mutex> guard(lock_);
      while (counter_ == 0) {
        if (nonblocking) {
          *err = guest::kEAGAIN;
          return -1;
        }
        if (closed_) {
          *err = guest::kEBADF;
          return -1;
        }
        changed_.wait(guard);
      }
      value = semaphore_ ? 1 : counter_;
      counter_ -= value;
      SyncSignalLocked();
      out_edges_.fetch_add(1, std::memory_order_relaxed);
    }
    changed_.notify_all();
    NotifyWatchers();
    std::memcpy(buffer, &value, sizeof(value));
    return sizeof(value);
  }

  g_ssize Write(const void* buffer, std::size_t size, bool nonblocking, int* err) noexcept override {
    if (size < sizeof(std::uint64_t)) {
      *err = guest::kEINVAL;
      return -1;
    }
    std::uint64_t value;
    std::memcpy(&value, buffer, sizeof(value));
    if (value == UINT64_MAX) {
      *err = guest::kEINVAL;
      return -1;
    }
    {
      std::unique_lock<std::mutex> guard(lock_);
      while (kEventFdMax - counter_ < value) {
        if (nonblocking) {
          *err = guest::kEAGAIN;
          return -1;
        }
        if (closed_) {
          *err = guest::kEBADF;
          return -1;
        }
        changed_.wait(guard);
      }
      counter_ += value;
      SyncSignalLocked();
      in_edges_.fetch_add(1, std::memory_order_relaxed);
    }
    changed_.notify_all();
    NotifyWatchers();
    return sizeof(value);
  }

  void Close() noexcept override {
    {
      std::lock_guard<std::mutex> guard(lock_);
      closed_ = true;
    }
    changed_.notify_all();
  }

  std::uint64_t InEdges() const noexcept override { return in_edges_.load(std::memory_order_relaxed); }
  std::uint64_t OutEdges() const noexcept override { return out_edges_.load(std::memory_order_relaxed); }

 private:
  void SyncSignalLocked() noexcept {
    if (counter_ > 0) {
      if (!queued_) queued_ = signal_.Post();
    } else if (queued_) {
      signal_.Drain();
      queued_ = false;
    }
  }

 public:
  void DrainStale() noexcept {
    std::lock_guard<std::mutex> guard(lock_);
    if (counter_ == 0) {
      signal_.Drain();
      queued_ = false;
    }
  }

 private:

  std::mutex lock_;
  std::condition_variable changed_;
  std::uint64_t counter_ = 0;
  bool semaphore_ = false;
  bool queued_ = false;
  bool closed_ = false;
  SignalSocket signal_;
  std::atomic<std::uint64_t> in_edges_{0};
  std::atomic<std::uint64_t> out_edges_{0};
};

class TimerFd final : public NetObject {
 public:
  explicit TimerFd(int clock) : NetObject(ObjectType::kTimerFd), clock_(clock) {}
  ~TimerFd() override { Close(); }

  bool Init() {
    if (!signal_.Open()) return false;
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer_ == nullptr) timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    if (timer_ == nullptr) {
      SetGuestErrno(ErrnoFromWin32(GetLastError()));
      return false;
    }
    wait_ = CreateThreadpoolWait(&TimerFd::OnTimer, this, nullptr);
    if (wait_ == nullptr) {
      SetGuestErrno(ErrnoFromWin32(GetLastError()));
      return false;
    }
    return true;
  }

  std::uint32_t Readiness() noexcept override {
    std::lock_guard<std::mutex> guard(lock_);
    AccountLocked(MonotonicNs());
    return pending_ > 0 ? (guest::kPOLLIN | guest::kPOLLRDNORM) : 0;
  }

  SOCKET WaitSocket() const noexcept override { return signal_.get(); }

  void Refresh() noexcept override {
    std::lock_guard<std::mutex> guard(lock_);
    AccountLocked(MonotonicNs());
    SyncSignalLocked();
  }

  g_ssize Read(void* buffer, std::size_t size, bool nonblocking, int* err) noexcept override {
    if (size < sizeof(std::uint64_t)) {
      *err = guest::kEINVAL;
      return -1;
    }
    std::uint64_t value = 0;
    {
      std::unique_lock<std::mutex> guard(lock_);
      for (;;) {
        const std::int64_t now = MonotonicNs();
        AccountLocked(now);
        if (pending_ > 0) break;
        if (nonblocking) {
          *err = guest::kEAGAIN;
          return -1;
        }
        if (closed_) {
          *err = guest::kEBADF;
          return -1;
        }
        if (armed_) {
          const std::int64_t wait_ns = next_ns_ - now;
          changed_.wait_for(guard, std::chrono::nanoseconds(wait_ns > 0 ? wait_ns : 0));
        } else {
          changed_.wait(guard);
        }
      }
      value = pending_;
      pending_ = 0;
      SyncSignalLocked();
      ScheduleLocked();
    }
    std::memcpy(buffer, &value, sizeof(value));
    return sizeof(value);
  }

  void Set(bool absolute, const GuestItimerspec& value, GuestItimerspec* old) {
    std::lock_guard<std::mutex> guard(lock_);
    const std::int64_t now = MonotonicNs();
    if (old != nullptr) GetLocked(now, old);
    pending_ = 0;
    interval_ns_ = value.it_interval.tv_sec * 1000000000 + value.it_interval.tv_nsec;
    const std::int64_t first = value.it_value.tv_sec * 1000000000 + value.it_value.tv_nsec;
    armed_ = first != 0;
    if (armed_) {
      if (!absolute) {
        next_ns_ = now + first;
      } else if (clock_ == guest::kCLOCK_REALTIME) {
        next_ns_ = now + (first - RealtimeNs());
      } else {
        next_ns_ = first;
      }
    }
    SyncSignalLocked();
    ScheduleLocked();
  }

  void Get(GuestItimerspec* out) {
    std::lock_guard<std::mutex> guard(lock_);
    GetLocked(MonotonicNs(), out);
  }

  void Close() noexcept override {
    {
      std::lock_guard<std::mutex> guard(lock_);
      if (closed_ && wait_ == nullptr) return;
      closed_ = true;
      armed_ = false;
    }
    changed_.notify_all();
    if (wait_ != nullptr) {
      SetThreadpoolWait(wait_, nullptr, nullptr);
      WaitForThreadpoolWaitCallbacks(wait_, TRUE);
      CloseThreadpoolWait(wait_);
      wait_ = nullptr;
    }
    if (timer_ != nullptr) {
      CancelWaitableTimer(timer_);
      CloseHandle(timer_);
      timer_ = nullptr;
    }
  }

  std::uint64_t InEdges() const noexcept override { return edges_.load(std::memory_order_relaxed); }

 private:
  static void CALLBACK OnTimer(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WAIT, TP_WAIT_RESULT) {
    auto* self = static_cast<TimerFd*>(context);
    bool notify = false;
    {
      std::lock_guard<std::mutex> guard(self->lock_);
      if (self->closed_) return;
      self->AccountLocked(MonotonicNs());
      if (self->pending_ > 0) {
        self->SyncSignalLocked();
        notify = true;
      } else {
        self->ScheduleLocked();
      }
    }
    if (notify) {
      self->changed_.notify_all();
      self->NotifyWatchers();
    }
  }

  void AccountLocked(std::int64_t now) {
    if (!armed_ || now < next_ns_) return;
    std::uint64_t expirations = 1;
    if (interval_ns_ > 0) {
      const std::int64_t periods = (now - next_ns_) / interval_ns_;
      expirations += static_cast<std::uint64_t>(periods);
      next_ns_ += (periods + 1) * interval_ns_;
    } else {
      armed_ = false;
    }
    pending_ += expirations;
    edges_.fetch_add(1, std::memory_order_relaxed);
  }

  void SyncSignalLocked() {
    if (pending_ > 0) {
      if (!queued_) queued_ = signal_.Post();
    } else {
      signal_.Drain();
      queued_ = false;
    }
  }

  void ScheduleLocked() {
    if (timer_ == nullptr || wait_ == nullptr) return;
    if (!armed_ || pending_ > 0 || closed_) {
      CancelWaitableTimer(timer_);
      return;
    }
    EnsureFineTimerResolution();
    const std::int64_t delta = next_ns_ - MonotonicNs();
    LARGE_INTEGER due;
    due.QuadPart = -(delta > 0 ? (delta + 99) / 100 : 1);
    SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE);
    SetThreadpoolWait(wait_, timer_, nullptr);
  }

  void GetLocked(std::int64_t now, GuestItimerspec* out) {
    AccountLocked(now);
    out->it_interval.tv_sec = interval_ns_ / 1000000000;
    out->it_interval.tv_nsec = interval_ns_ % 1000000000;
    if (armed_) {
      const std::int64_t left = next_ns_ > now ? next_ns_ - now : 1;
      out->it_value.tv_sec = left / 1000000000;
      out->it_value.tv_nsec = left % 1000000000;
    } else {
      out->it_value.tv_sec = 0;
      out->it_value.tv_nsec = 0;
    }
  }

  int clock_;
  std::mutex lock_;
  std::condition_variable changed_;
  SignalSocket signal_;
  HANDLE timer_ = nullptr;
  PTP_WAIT wait_ = nullptr;
  bool armed_ = false;
  bool queued_ = false;
  bool closed_ = false;
  std::int64_t next_ns_ = 0;
  std::int64_t interval_ns_ = 0;
  std::uint64_t pending_ = 0;
  std::atomic<std::uint64_t> edges_{0};
};

bool ValidTimespec(const guest::Timespec& ts) {
  return ts.tv_sec >= 0 && ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000 && ts.tv_sec < INT64_MAX / 4000000000;
}

int EventFdCreate(unsigned int initial, int flags) {
  if ((flags & ~(guest::kEFD_SEMAPHORE | guest::kEFD_NONBLOCK | guest::kEFD_CLOEXEC)) != 0) {
    return Fail(guest::kEINVAL);
  }
  auto object = std::make_shared<EventFd>();
  if (!object->Init(initial, (flags & guest::kEFD_SEMAPHORE) != 0)) return -1;
  return InstallObject(object, flags & (guest::kEFD_NONBLOCK | guest::kEFD_CLOEXEC));
}

int EventFdTransfer(int fd, std::uint64_t* value, bool write) {
  if (value == nullptr) return Fail(guest::kEFAULT);
  ObjectPtr object = ResolveObject(fd);
  if (object == nullptr) return -1;
  int error = 0;
  const bool nonblocking = ObjectFdNonBlocking(fd, *object);
  const g_ssize result = write ? object->Write(value, sizeof(*value), nonblocking, &error)
                               : object->Read(value, sizeof(*value), nonblocking, &error);
  if (result != static_cast<g_ssize>(sizeof(*value))) return Fail(error != 0 ? error : guest::kEINVAL);
  return 0;
}

int TimerFdCreate(int clock, int flags) {
  switch (clock) {
    case guest::kCLOCK_REALTIME:
    case guest::kCLOCK_MONOTONIC:
    case guest::kCLOCK_BOOTTIME:
      break;
    case guest::kCLOCK_REALTIME_ALARM:
    case guest::kCLOCK_BOOTTIME_ALARM:
      return Fail(guest::kEPERM);
    default:
      return Fail(guest::kEINVAL);
  }
  if ((flags & ~(guest::kO_NONBLOCK | guest::kO_CLOEXEC)) != 0) return Fail(guest::kEINVAL);
  auto object = std::make_shared<TimerFd>(clock);
  if (!object->Init()) return -1;
  return InstallObject(object, flags);
}

std::shared_ptr<TimerFd> ResolveTimer(int fd) {
  ObjectPtr object = ResolveObject(fd);
  if (object == nullptr) return nullptr;
  if (object->type() != ObjectType::kTimerFd) {
    SetGuestErrno(guest::kEINVAL);
    return nullptr;
  }
  return std::static_pointer_cast<TimerFd>(object);
}

int TimerFdSet(int fd, int flags, const GuestItimerspec* value, GuestItimerspec* old) {
  if ((flags & ~(kTFD_TIMER_ABSTIME | kTFD_TIMER_CANCEL_ON_SET)) != 0) return Fail(guest::kEINVAL);
  auto timer = ResolveTimer(fd);
  if (timer == nullptr) return -1;
  if (value == nullptr) return Fail(guest::kEFAULT);
  if (!ValidTimespec(value->it_value) || !ValidTimespec(value->it_interval)) return Fail(guest::kEINVAL);
  timer->Set((flags & kTFD_TIMER_ABSTIME) != 0, *value, old);
  return 0;
}

int TimerFdGet(int fd, GuestItimerspec* out) {
  auto timer = ResolveTimer(fd);
  if (timer == nullptr) return -1;
  if (out == nullptr) return Fail(guest::kEFAULT);
  timer->Get(out);
  return 0;
}

}

ELIXIR_BIONIC_FN(int, eventfd)(unsigned int initial, int flags) noexcept {
  return Guarded(-1, [&] { return EventFdCreate(initial, flags); });
}
ELIXIR_BIONIC_FN(int, eventfd_read)(int fd, std::uint64_t* value) noexcept {
  return Guarded(-1, [&] { return EventFdTransfer(fd, value, false); });
}
ELIXIR_BIONIC_FN(int, eventfd_write)(int fd, std::uint64_t value) noexcept {
  return Guarded(-1, [&] { return EventFdTransfer(fd, &value, true); });
}
ELIXIR_BIONIC_FN(int, timerfd_create)(int clock, int flags) noexcept {
  return Guarded(-1, [&] { return TimerFdCreate(clock, flags); });
}
ELIXIR_BIONIC_FN(int, timerfd_settime)(int fd, int flags, const GuestItimerspec* value,
                                       GuestItimerspec* old) noexcept {
  return Guarded(-1, [&] { return TimerFdSet(fd, flags, value, old); });
}
ELIXIR_BIONIC_FN(int, timerfd_gettime)(int fd, GuestItimerspec* out) noexcept {
  return Guarded(-1, [&] { return TimerFdGet(fd, out); });
}

void RegisterObjectExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("eventfd", &elixir_bionic_eventfd);
  add("eventfd_read", &elixir_bionic_eventfd_read);
  add("eventfd_write", &elixir_bionic_eventfd_write);
  add("timerfd_create", &elixir_bionic_timerfd_create);
  add("timerfd_settime", &elixir_bionic_timerfd_settime);
  add("timerfd_gettime", &elixir_bionic_timerfd_gettime);
}

}
