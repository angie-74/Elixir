// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_NET_NET_INTERNAL_H_
#define ELIXIR_BIONIC_NET_NET_INTERNAL_H_

#include "bionic/net.h"
#include "bionic/runtime.h"
#include "bionic/vfs.h"
#include "platform/windows/guest_tls.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <mstcpip.h>
#include <afunix.h>
#include <iphlpapi.h>
#include <windows.h>

#undef s_addr
#undef s6_addr
#undef s_host
#undef s_net
#undef s_imp
#undef s_impno
#undef s_lh
#undef h_addr

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace elixir::bionic::net::internal {

using abi::g_size;
using abi::g_ssize;
using abi::g_socklen;

inline constexpr int kIPV6_V6ONLY = 26;
inline constexpr int kUDP_CORK = 1;
inline constexpr int kUDP_SEGMENT = 103;
inline constexpr int kUDP_GRO = 104;
inline constexpr int kFD_SETSIZE = 1024;
inline constexpr int kIOV_MAX = 1024;
inline constexpr std::uint32_t kIFF_UP = 0x1;
inline constexpr std::uint32_t kIFF_BROADCAST = 0x2;
inline constexpr std::uint32_t kIFF_LOOPBACK = 0x8;
inline constexpr std::uint32_t kIFF_POINTOPOINT = 0x10;
inline constexpr std::uint32_t kIFF_RUNNING = 0x40;
inline constexpr std::uint32_t kIFF_NOARP = 0x80;
inline constexpr std::uint32_t kIFF_MULTICAST = 0x1000;
inline constexpr int kNETDB_INTERNAL = -1;
inline constexpr int kNETDB_SUCCESS = 0;
inline constexpr int kHOST_NOT_FOUND = 1;
inline constexpr int kTRY_AGAIN = 2;
inline constexpr int kNO_RECOVERY = 3;
inline constexpr int kNO_DATA = 4;
inline constexpr int kTFD_TIMER_ABSTIME = 1;
inline constexpr int kTFD_TIMER_CANCEL_ON_SET = 2;
inline constexpr std::uint32_t kSIN6_LEN_RFC2133 = 24;
inline constexpr std::uint32_t kGuestUid = 10100;

struct GuestMmsghdr {
  guest::Msghdr msg_hdr;
  std::uint32_t msg_len;
};
static_assert(sizeof(GuestMmsghdr) == 64, "struct mmsghdr");

struct GuestItimerspec {
  guest::Timespec it_interval;
  guest::Timespec it_value;
};
static_assert(sizeof(GuestItimerspec) == 32, "struct itimerspec");

struct GuestIfaddrs {
  GuestIfaddrs* ifa_next;
  char* ifa_name;
  std::uint32_t ifa_flags;
  guest::Sockaddr* ifa_addr;
  guest::Sockaddr* ifa_netmask;
  guest::Sockaddr* ifa_ifu;
  void* ifa_data;
};
static_assert(sizeof(GuestIfaddrs) == layout::kIfaddrs__size, "struct ifaddrs");
static_assert(offsetof(GuestIfaddrs, ifa_flags) == layout::kIfaddrs_ifa_flags, "ifa_flags");
static_assert(offsetof(GuestIfaddrs, ifa_addr) == layout::kIfaddrs_ifa_addr, "ifa_addr");
static_assert(offsetof(GuestIfaddrs, ifa_netmask) == layout::kIfaddrs_ifa_netmask, "ifa_netmask");
static_assert(offsetof(GuestIfaddrs, ifa_ifu) == layout::kIfaddrs_ifa_dstaddr, "ifa_dstaddr");
static_assert(offsetof(GuestIfaddrs, ifa_data) == layout::kIfaddrs_ifa_data, "ifa_data");

struct GuestIfNameindex {
  std::uint32_t if_index;
  char* if_name;
};
static_assert(sizeof(GuestIfNameindex) == 16, "struct if_nameindex");

struct GuestServent {
  char* s_name;
  char** s_aliases;
  std::int32_t s_port;
  char* s_proto;
};
static_assert(sizeof(GuestServent) == 32 && offsetof(GuestServent, s_proto) == 24, "struct servent");

struct GuestUcred {
  std::int32_t pid;
  std::uint32_t uid;
  std::uint32_t gid;
};

struct GuestIpMreqn {
  std::uint32_t multiaddr;
  std::uint32_t address;
  std::int32_t ifindex;
};
struct GuestIpMreqSource {
  std::uint32_t multiaddr;
  std::uint32_t interface_addr;
  std::uint32_t sourceaddr;
};
struct GuestIpv6Mreq {
  std::uint8_t addr[16];
  std::int32_t ifindex;
};
static_assert(sizeof(GuestIpv6Mreq) == 20, "struct ipv6_mreq");

struct GuestEtherAddr {
  std::uint8_t octet[6];
};

struct GuestFdSet {
  std::uint64_t bits[kFD_SETSIZE / 64];
};
static_assert(sizeof(GuestFdSet) == layout::kFdSet__size, "fd_set");

template <typename R, typename F>
R Guarded(R failure, F&& body) noexcept {
  const platform::GuestThreadResume resume;
  try {
    return body();
  } catch (const std::bad_alloc&) {
    SetGuestErrno(guest::kENOMEM);
  } catch (...) {
    SetGuestErrno(guest::kEIO);
  }
  return failure;
}

[[noreturn]] void FortifyFatal(const char* function, const char* what) noexcept;

std::int64_t MonotonicNs() noexcept;
std::int64_t RealtimeNs() noexcept;
void EnsureFineTimerResolution() noexcept;
void SleepPreciseNs(std::int64_t ns) noexcept;

class Deadline {
 public:
  static Deadline Infinite() { return Deadline(-1); }
  static Deadline Now() { return Deadline(0); }
  static Deadline FromMs(std::int64_t ms);
  static Deadline FromNs(std::int64_t ns);
  bool infinite() const { return at_ns_ < 0; }
  bool Expired() const;
  int WaitMs() const;
  std::int64_t RemainingNs() const;

 private:
  explicit Deadline(std::int64_t at) : at_ns_(at) {}
  std::int64_t at_ns_;
};

int GuestStatusFlags(int fd) noexcept;
int GuestSetStatusFlags(int fd, int flags) noexcept;
int GuestClose(int fd) noexcept;
void GuestWriteStderr(const char* data, std::size_t length) noexcept;
bool UsingFallbackFdTable() noexcept;
void InstallFilesHooks() noexcept;
bool HookedReadiness(int fd, std::uint32_t events, std::uint32_t* ready) noexcept;
std::uint32_t FdPoll(void* handle, std::uint8_t kind, std::uint32_t events) noexcept;

class Waker {
 public:
  virtual ~Waker() = default;
  virtual void Wake() noexcept = 0;
};

class WatcherList {
 public:
  void Add(const std::shared_ptr<Waker>& waker);
  void Remove(const Waker* waker);
  void Notify() noexcept;
  bool Empty() const noexcept { return count_.load(std::memory_order_acquire) == 0; }

 private:
  std::mutex lock_;
  std::vector<std::weak_ptr<Waker>> list_;
  std::atomic<int> count_{0};
};

enum class SockPhase : std::uint8_t { kFresh, kConnecting, kConnected, kListening };

enum SocketHint : int {
  kHintReuseAddr,
  kHintReusePort,
  kHintPriority,
  kHintPassCred,
  kHintTcpCork,
  kHintTcpQuickAck,
  kHintTcpDeferAccept,
  kHintTcpNotSentLowat,
  kHintTcpUserTimeout,
  kHintBindNoPort,
  kHintIpRecvErr,
  kHintRcvBuf,
  kHintSndBuf,
  kHintCount
};

struct SocketState;

class PairChannel {
 public:
  static constexpr std::size_t kBudget = 212992;
  static constexpr std::size_t kOverhead = 576;
  static constexpr int kHostReceiveBuffer = 1 << 20;

  enum Reserve { kReserved, kFull, kPeerClosed };
  Reserve TryReserve(int to, std::size_t length) noexcept;
  void Cancel(int to) noexcept;
  void Consumed(int to) noexcept;
  bool Writable(int to) noexcept;
  int WaitWritable(int to, const Deadline& deadline) noexcept;
  void Close(int end) noexcept;
  void Attach(int end, const std::shared_ptr<SocketState>& state) noexcept;

 private:
  std::mutex lock_;
  std::condition_variable space_;
  std::deque<std::uint32_t> charges_[2];
  std::size_t queued_[2] = {0, 0};
  bool closed_[2] = {false, false};
  std::weak_ptr<SocketState> ends_[2];
};

struct SocketState {
  SOCKET socket = INVALID_SOCKET;
  std::uint64_t serial = 0;
  int domain = 0;
  int type = 0;
  int protocol = 0;
  int host_family = 0;
  bool emulated_unix = false;
  std::atomic<SockPhase> phase{SockPhase::kFresh};
  std::atomic<bool> bound{false};
  std::atomic<bool> nonblocking{false};
  std::atomic<bool> shut_rd{false};
  std::atomic<bool> shut_wr{false};
  std::atomic<bool> accept_shutdown{false};
  std::atomic<std::int64_t> rcv_timeout_us{0};
  std::atomic<std::int64_t> snd_timeout_us{0};
  std::atomic<std::uint32_t> in_seq{0};
  std::atomic<std::uint32_t> out_seq{0};
  std::atomic<int> reported_so_error{0};
  std::atomic<std::uint64_t> tcp_connect_attempt{0};
  std::atomic<std::uint32_t> gso_size{0};
  std::atomic<int> gro{0};
  std::atomic<int> rcvlowat{1};
  std::atomic<int> linger_on{0};
  std::atomic<int> linger_seconds{0};
  std::atomic<int> hints[kHintCount] = {};
  std::atomic<bool> connreset_enabled{false};
  WatcherList watchers;
  std::shared_ptr<PairChannel> pair;
  int pair_end = 0;

  std::mutex name_lock;
  std::string unix_name;
  std::string unix_peer;
  std::wstring unix_cleanup;
};
using SocketPtr = std::shared_ptr<SocketState>;

void CompleteTcpConnectTrace(SocketState& state, std::uint64_t attempt, int guest_errno) noexcept;

inline void NoteWouldBlock(SocketState* state, bool sending) noexcept {
  if (state == nullptr) return;
  (sending ? state->out_seq : state->in_seq).fetch_add(1, std::memory_order_release);
  state->watchers.Notify();
}

SocketPtr RegisterSocket(SOCKET s, int domain, int type, int protocol, int host_family);
SocketPtr LookupSocket(SOCKET s) noexcept;
SocketPtr TakeSocket(SOCKET s) noexcept;

int PrepareHostSocket(SOCKET s, int host_family, int type) noexcept;

int InstallSocket(SOCKET s, const SocketPtr& state, int flags) noexcept;

struct SocketRef {
  SOCKET s = INVALID_SOCKET;
  SocketPtr state;
};

bool ResolveSocket(int fd, SocketRef* out, bool want_state) noexcept;

bool FdIsNonBlocking(int fd, const SocketState* state) noexcept;

int WaitSocket(SOCKET s, short wsa_events, std::int64_t timeout_us,
               const std::atomic<bool>* interrupted = nullptr) noexcept;

int SocketErrno(int wsa_error, const SocketState* state, bool sending) noexcept;

extern std::atomic<int> g_gso_sockets;
extern std::atomic<int> g_pair_sockets;

struct HostAddr {
  SOCKADDR_STORAGE storage;
  int length = 0;
  sockaddr* get() { return reinterpret_cast<sockaddr*>(&storage); }
  const sockaddr* get() const { return reinterpret_cast<const sockaddr*>(&storage); }
};

enum class AddrUse : std::uint8_t { kBind, kConnect, kSendTo };

int GuestToHostAddr(const SocketState& state, const void* guest_addr, g_socklen guest_len, AddrUse use,
                    HostAddr* out, std::string* unix_name = nullptr) noexcept;
std::uint32_t HostToGuestSockaddr(const sockaddr* host, int host_len, guest::SockaddrStorage* out) noexcept;
int CopySockaddrOut(const guest::SockaddrStorage& value, std::uint32_t length, void* guest_addr,
                    g_socklen* guest_len) noexcept;
std::uint32_t UnixSockaddr(const std::string& name, guest::SockaddrStorage* out) noexcept;
std::wstring UnixHostPath(const std::string& name);
std::string UnixAutobindName();
bool IsLoopbackHost(const sockaddr* host) noexcept;
int GuestFamilyToHost(int family) noexcept;
int HostFamilyToGuest(int family) noexcept;

class SignalSocket {
 public:
  SignalSocket() = default;
  SignalSocket(const SignalSocket&) = delete;
  SignalSocket& operator=(const SignalSocket&) = delete;
  ~SignalSocket();
  bool Open() noexcept;
  SOCKET get() const noexcept { return socket_; }
  bool Post() noexcept;
  int Drain() noexcept;

 private:
  SOCKET socket_ = INVALID_SOCKET;
};

class WakeFlag {
 public:
  bool Open() noexcept { return signal_.Open(); }
  SOCKET get() const noexcept { return signal_.get(); }
  void Raise() noexcept;
  void Consume() noexcept;
  void ConsumeIfRaised() noexcept;

 private:
  SignalSocket signal_;
  std::atomic<bool> raised_{false};
};

bool AddBlockedWaiter(Waker* waker) noexcept;
void RemoveBlockedWaiter(Waker* waker) noexcept;
void PurgeBlockedWaiter(Waker* waker) noexcept;
void WakeBlockedWaiters() noexcept;
void NoteShutdown(SocketState* state) noexcept;

class BlockedWaiterScope {
 public:
  explicit BlockedWaiterScope(Waker* waker) noexcept
      : waker_(waker != nullptr && AddBlockedWaiter(waker) ? waker : nullptr) {}
  BlockedWaiterScope(const BlockedWaiterScope&) = delete;
  BlockedWaiterScope& operator=(const BlockedWaiterScope&) = delete;
  ~BlockedWaiterScope() {
    if (waker_ != nullptr) RemoveBlockedWaiter(waker_);
  }
  bool active() const noexcept { return waker_ != nullptr; }

 private:
  Waker* waker_;
};

class InterruptFlag final : public Waker {
 public:
  InterruptFlag() = default;
  InterruptFlag(const InterruptFlag&) = delete;
  InterruptFlag& operator=(const InterruptFlag&) = delete;
  ~InterruptFlag() override { PurgeBlockedWaiter(this); }
  bool Open() noexcept { return flag_.Open(); }
  SOCKET get() const noexcept { return flag_.get(); }
  void Wake() noexcept override { flag_.Raise(); }
  void Consume() noexcept { flag_.Consume(); }
  void ConsumeIfRaised() noexcept { flag_.ConsumeIfRaised(); }

 private:
  WakeFlag flag_;
};

InterruptFlag* ThreadInterrupt() noexcept;

enum class ObjectType : std::uint8_t { kEventFd, kTimerFd, kEpoll };

class NetObject {
 public:
  explicit NetObject(ObjectType type);
  NetObject(const NetObject&) = delete;
  NetObject& operator=(const NetObject&) = delete;
  virtual ~NetObject();

  ObjectType type() const { return type_; }
  std::uint64_t serial() const { return serial_; }
  HANDLE token() const { return token_; }
  void set_token(HANDLE token) { token_ = token; }

  virtual std::uint32_t Readiness() noexcept = 0;
  virtual SOCKET WaitSocket() const noexcept { return INVALID_SOCKET; }
  virtual void Refresh() noexcept {}
  virtual g_ssize Read(void* buffer, std::size_t size, bool nonblocking, int* err) noexcept;
  virtual g_ssize Write(const void* buffer, std::size_t size, bool nonblocking, int* err) noexcept;
  virtual void Close() noexcept {}
  virtual std::uint64_t InEdges() const noexcept { return 0; }
  virtual std::uint64_t OutEdges() const noexcept { return 0; }

  std::atomic<bool> nonblocking{false};

  void AddWatcher(const std::shared_ptr<Waker>& waker) { watchers_.Add(waker); }
  void RemoveWatcher(const Waker* waker) { watchers_.Remove(waker); }
  void NotifyWatchers() noexcept { watchers_.Notify(); }

 private:
  ObjectType type_;
  std::uint64_t serial_;
  HANDLE token_ = nullptr;
  WatcherList watchers_;
};
using ObjectPtr = std::shared_ptr<NetObject>;

int InstallObject(const ObjectPtr& object, int flags) noexcept;
ObjectPtr LookupObject(void* handle) noexcept;
ObjectPtr TakeObject(void* handle) noexcept;
ObjectPtr ResolveObject(int fd) noexcept;
std::uint64_t NextSerial() noexcept;

struct Target {
  int fd = -1;
  vfs::FdKind kind = vfs::FdKind::kFile;
  void* handle = nullptr;
  SocketPtr sock;
  ObjectPtr object;
  int access_mode = 0;
  bool valid = false;
};

bool ResolveTarget(int fd, Target* target) noexcept;
bool SameTarget(const Target& a, const Target& b) noexcept;

short ToWsaEvents(std::uint32_t guest_events) noexcept;
std::uint32_t FromWsaRevents(short revents, const SocketState* state) noexcept;
std::uint32_t FixedSocketReadiness(const SocketState* state) noexcept;
std::uint32_t ObjectOrFileReadiness(Target& target, std::uint32_t events, bool* needs_slice) noexcept;
SOCKET WaitHandleFor(const Target& target, std::uint32_t events) noexcept;

inline constexpr std::uint32_t kAlwaysReported = guest::kPOLLERR | guest::kPOLLHUP | guest::kPOLLNVAL;

int PollFds(guest::Pollfd* fds, std::uint64_t count, Deadline deadline) noexcept;

inline constexpr int kSliceMs = 4;

void RegisterSocketExports(ExportTable& table);
void RegisterIoExports(ExportTable& table);
void RegisterSockoptExports(ExportTable& table);
void RegisterPollExports(ExportTable& table);
void RegisterEpollExports(ExportTable& table);
void RegisterObjectExports(ExportTable& table);
void RegisterNetdbExports(ExportTable& table);
void RegisterInetExports(ExportTable& table);
void RegisterIfaddrsExports(ExportTable& table);
void RegisterServiceExports(ExportTable& table);

std::wstring Utf8ToWide(const char* text);
std::string WideToUtf8(const wchar_t* text);

int InetPton(int family, const char* text, void* out) noexcept;
const char* InetNtop(int family, const void* addr, char* out, std::size_t size) noexcept;
bool InetAton(const char* text, std::uint32_t* out_network_order) noexcept;
bool InterfaceNameFromIndex(std::uint32_t index, std::string* name);
std::uint32_t InterfaceIndexFromName(const char* name);
bool LookupServiceByName(const char* name, const char* proto, int* port_network_order, std::string* proto_out);
bool LookupServiceByPort(int port_network_order, const char* proto, std::string* name_out);

}

#endif
