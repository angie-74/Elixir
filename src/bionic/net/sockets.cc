// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <map>

#include "platform/trace_clock.h"

namespace elixir::bionic::net::internal {
namespace {

constexpr int kTypeMask = 0xf;
constexpr int kSockMax = 11;

int Fail(int error) {
  SetGuestErrno(error);
  return -1;
}

SOCKET NewHostSocket(int family, int type, int protocol) {
  return WSASocketW(family, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
}

void FastLoopbackRefusal(SOCKET s, const sockaddr* target) {
  if (!IsLoopbackHost(target)) return;
  TCP_INITIAL_RTO_PARAMETERS parameters{TCP_INITIAL_RTO_UNSPECIFIED_RTT, TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS};
  DWORD bytes = 0;
  WSAIoctl(s, SIO_TCP_INITIAL_RTO, &parameters, sizeof(parameters), nullptr, 0, &bytes, nullptr, nullptr);
}

std::uint16_t PortOf(const HostAddr& host) {
  if (host.get()->sa_family == AF_INET) return ntohs(reinterpret_cast<const sockaddr_in*>(host.get())->sin_port);
  if (host.get()->sa_family == AF_INET6) return ntohs(reinterpret_cast<const sockaddr_in6*>(host.get())->sin6_port);
  return 0;
}

int PendingError(SOCKET s, bool* observed = nullptr) {
  int error = 0;
  int length = sizeof(error);
  const bool read = getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0;
  if (observed != nullptr) *observed = read;
  return read ? error : 0;
}

std::uint64_t NextTcpConnectAttempt(const SocketState& state) noexcept {
  if (state.protocol != guest::kIPPROTO_TCP || Options().log_sink == nullptr) return 0;
  static std::atomic<std::uint64_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}

void WriteTcpConnectTrace(std::uint64_t attempt, const char* phase, int guest_errno) noexcept {
  if (attempt == 0) return;
  const DWORD last_error = GetLastError();
  const int host_errno = errno;
  const int saved_guest_errno = GuestErrno();
  char message[160];
  std::snprintf(message, sizeof(message), "network event=tcp_connect phase=%s attempt_id=%llu guest_errno=%d utc_ms=%llu",
                phase, static_cast<unsigned long long>(attempt), guest_errno,
                static_cast<unsigned long long>(elixir::DiagnosticUtcMillis()));
  HostLog(4, "elixir", message);
  SetGuestErrno(saved_guest_errno);
  errno = host_errno;
  SetLastError(last_error);
}

void TraceTcpConnectReturn(SocketState& state, std::uint64_t previous, int guest_errno) noexcept {
  if (previous != 0) {
    CompleteTcpConnectTrace(state, previous, guest_errno);
    return;
  }
  WriteTcpConnectTrace(NextTcpConnectAttempt(state), guest_errno == 0 ? "connected" : "failed", guest_errno);
}

struct UnixBinding {
  SOCKET socket = INVALID_SOCKET;
  std::weak_ptr<SocketState> state;
};

std::mutex& UnixBindingsLock() {
  static std::mutex* lock = new std::mutex();
  return *lock;
}

std::map<std::wstring, UnixBinding>& UnixBindings() {
  static auto* bindings = new std::map<std::wstring, UnixBinding>();
  return *bindings;
}

bool UnixAddressInUse(const std::wstring& path) {
  UnixBinding binding;
  {
    std::lock_guard<std::mutex> guard(UnixBindingsLock());
    const auto it = UnixBindings().find(path);
    if (it == UnixBindings().end()) return false;
    binding = it->second;
  }
  SocketPtr owner = binding.state.lock();
  if (owner == nullptr || LookupSocket(binding.socket) != owner) return false;
  int type = 0;
  int length = sizeof(type);
  return getsockopt(binding.socket, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&type), &length) == 0;
}

void RememberUnixBinding(const std::wstring& path, const SocketPtr& state) {
  std::lock_guard<std::mutex> guard(UnixBindingsLock());
  UnixBindings()[path] = UnixBinding{state->socket, state};
}

std::wstring HostPathOf(const HostAddr& host) {
  const auto* un = reinterpret_cast<const SOCKADDR_UN*>(host.get());
  const int chars = MultiByteToWideChar(CP_UTF8, 0, un->sun_path, -1, nullptr, 0);
  if (chars <= 1) return {};
  std::wstring path(static_cast<std::size_t>(chars - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, un->sun_path, -1, path.data(), chars);
  return path;
}

int CreateSocket(int domain, int type, int protocol) {
  if (!EnsureWinsock()) return Fail(guest::kENETDOWN);
  const int flags = type & (guest::kSOCK_NONBLOCK | guest::kSOCK_CLOEXEC);
  const int base = type & ~(guest::kSOCK_NONBLOCK | guest::kSOCK_CLOEXEC);
  if ((base & ~kTypeMask) != 0 || base < 0 || base >= kSockMax) return Fail(guest::kEINVAL);

  int host_family = 0;
  int host_type = 0;
  int host_protocol = 0;
  int guest_protocol = protocol;
  switch (domain) {
    case guest::kAF_INET:
    case guest::kAF_INET6:
      host_family = domain == guest::kAF_INET ? AF_INET : AF_INET6;
      switch (base) {
        case guest::kSOCK_STREAM:
          if (protocol != 0 && protocol != guest::kIPPROTO_TCP) return Fail(guest::kEPROTONOSUPPORT);
          host_type = SOCK_STREAM;
          host_protocol = IPPROTO_TCP;
          guest_protocol = guest::kIPPROTO_TCP;
          break;
        case guest::kSOCK_DGRAM:
          if (protocol == guest::kIPPROTO_ICMP || protocol == guest::kIPPROTO_ICMPV6) {
            return Fail(guest::kEACCES);
          }
          if (protocol != 0 && protocol != guest::kIPPROTO_UDP) return Fail(guest::kEPROTONOSUPPORT);
          host_type = SOCK_DGRAM;
          host_protocol = IPPROTO_UDP;
          guest_protocol = guest::kIPPROTO_UDP;
          break;
        case guest::kSOCK_RAW:
          return Fail(guest::kEPERM);
        default:
          return Fail(guest::kESOCKTNOSUPPORT);
      }
      break;
    case guest::kAF_UNIX:
      if (protocol != 0 && protocol != guest::kAF_UNIX) return Fail(guest::kEPROTONOSUPPORT);
      if (base != guest::kSOCK_STREAM) {
        return Fail(guest::kESOCKTNOSUPPORT);
      }
      host_family = AF_UNIX;
      host_type = SOCK_STREAM;
      host_protocol = 0;
      guest_protocol = 0;
      break;
    case guest::kAF_PACKET:
      return Fail(guest::kEPERM);
    case guest::kAF_NETLINK:
    default:
      return Fail(guest::kEAFNOSUPPORT);
  }

  const SOCKET s = NewHostSocket(host_family, host_type, host_protocol);
  if (s == INVALID_SOCKET) {
    const int error = WSAGetLastError();
    return Fail(error == WSAEAFNOSUPPORT || error == WSAEPROTONOSUPPORT ? guest::kEAFNOSUPPORT
                                                                         : ErrnoFromWinsock(error));
  }
  const int prepared = PrepareHostSocket(s, host_family, host_type);
  if (prepared != 0) {
    closesocket(s);
    return Fail(prepared);
  }
  SocketPtr state;
  try {
    state = RegisterSocket(s, domain, base, guest_protocol, host_family);
  } catch (...) {
    closesocket(s);
    return Fail(guest::kENOMEM);
  }
  return InstallSocket(s, state, flags);
}

bool StreamPair(SOCKET* a, SOCKET* b, int* host_family) {
  static std::atomic<std::uint32_t> counter{0};
  SOCKET listener = NewHostSocket(AF_UNIX, SOCK_STREAM, 0);
  if (listener != INVALID_SOCKET) {
    LARGE_INTEGER nonce;
    QueryPerformanceCounter(&nonce);
    const std::string name = "\x01pair-" + std::to_string(GetCurrentProcessId()) + "-" +
                             std::to_string(counter.fetch_add(1)) + "-" + std::to_string(nonce.QuadPart);
    const std::wstring path = UnixHostPath(name);
    SOCKADDR_UN address{};
    address.sun_family = AF_UNIX;
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, address.sun_path,
                                          static_cast<int>(sizeof(address.sun_path)), nullptr, nullptr);
    DeleteFileW(path.c_str());
    if (bytes > 0 && bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
        listen(listener, 1) == 0) {
      SOCKET client = NewHostSocket(AF_UNIX, SOCK_STREAM, 0);
      if (client != INVALID_SOCKET && connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
        SOCKET server = accept(listener, nullptr, nullptr);
        if (server != INVALID_SOCKET) {
          closesocket(listener);
          DeleteFileW(path.c_str());
          *a = client;
          *b = server;
          *host_family = AF_UNIX;
          return true;
        }
      }
      if (client != INVALID_SOCKET) closesocket(client);
    }
    closesocket(listener);
    DeleteFileW(path.c_str());
  }

  listener = NewHostSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) return false;
  sockaddr_in loopback{};
  loopback.sin_family = AF_INET;
  loopback.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
  int length = sizeof(loopback);
  if (bind(listener, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) != 0 || listen(listener, 1) != 0 ||
      getsockname(listener, reinterpret_cast<sockaddr*>(&loopback), &length) != 0) {
    closesocket(listener);
    return false;
  }
  SOCKET client = NewHostSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client == INVALID_SOCKET || connect(client, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) != 0) {
    if (client != INVALID_SOCKET) closesocket(client);
    closesocket(listener);
    return false;
  }
  sockaddr_in client_name{};
  length = sizeof(client_name);
  getsockname(client, reinterpret_cast<sockaddr*>(&client_name), &length);
  for (;;) {
    sockaddr_in peer{};
    length = sizeof(peer);
    SOCKET server = accept(listener, reinterpret_cast<sockaddr*>(&peer), &length);
    if (server == INVALID_SOCKET) break;
    if (peer.sin_port == client_name.sin_port && peer.sin_addr.S_un.S_addr == client_name.sin_addr.S_un.S_addr) {
      closesocket(listener);
      BOOL nodelay = TRUE;
      setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
      setsockopt(server, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
      *a = client;
      *b = server;
      *host_family = AF_INET;
      return true;
    }
    closesocket(server);
  }
  closesocket(client);
  closesocket(listener);
  return false;
}

bool DatagramPair(SOCKET* a, SOCKET* b) {
  SOCKET first = NewHostSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  SOCKET second = NewHostSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  sockaddr_in first_name{};
  sockaddr_in second_name{};
  bool ok = first != INVALID_SOCKET && second != INVALID_SOCKET;
  for (SOCKET s : {first, second}) {
    if (!ok) break;
    sockaddr_in loopback{};
    loopback.sin_family = AF_INET;
    loopback.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    ok = bind(s, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == 0;
  }
  int length = sizeof(sockaddr_in);
  ok = ok && getsockname(first, reinterpret_cast<sockaddr*>(&first_name), &length) == 0;
  length = sizeof(sockaddr_in);
  ok = ok && getsockname(second, reinterpret_cast<sockaddr*>(&second_name), &length) == 0;
  ok = ok && connect(first, reinterpret_cast<sockaddr*>(&second_name), sizeof(second_name)) == 0;
  ok = ok && connect(second, reinterpret_cast<sockaddr*>(&first_name), sizeof(first_name)) == 0;
  if (!ok) {
    if (first != INVALID_SOCKET) closesocket(first);
    if (second != INVALID_SOCKET) closesocket(second);
    return false;
  }
  *a = first;
  *b = second;
  return true;
}

int SocketPairImpl(int domain, int type, int protocol, int* sv) {
  if (!EnsureWinsock()) return Fail(guest::kENETDOWN);
  if (sv == nullptr) return Fail(guest::kEFAULT);
  if (domain == guest::kAF_INET || domain == guest::kAF_INET6) return Fail(guest::kEOPNOTSUPP);
  if (domain != guest::kAF_UNIX) return Fail(guest::kEAFNOSUPPORT);
  if (protocol != 0 && protocol != guest::kAF_UNIX) return Fail(guest::kEPROTONOSUPPORT);
  const int flags = type & (guest::kSOCK_NONBLOCK | guest::kSOCK_CLOEXEC);
  const int base = type & ~(guest::kSOCK_NONBLOCK | guest::kSOCK_CLOEXEC);
  if ((base & ~kTypeMask) != 0 || base < 0 || base >= kSockMax) return Fail(guest::kEINVAL);

  SOCKET a = INVALID_SOCKET;
  SOCKET b = INVALID_SOCKET;
  int host_family = AF_INET;
  int host_type = SOCK_STREAM;
  int guest_type = base;
  switch (base) {
    case guest::kSOCK_STREAM:
      if (!StreamPair(&a, &b, &host_family)) return Fail(ErrnoFromWinsock(WSAGetLastError()));
      break;
    case guest::kSOCK_RAW:
      guest_type = guest::kSOCK_DGRAM;
      [[fallthrough]];
    case guest::kSOCK_DGRAM:
    case guest::kSOCK_SEQPACKET:
      if (!DatagramPair(&a, &b)) return Fail(ErrnoFromWinsock(WSAGetLastError()));
      host_type = SOCK_DGRAM;
      break;
    default:
      return Fail(guest::kESOCKTNOSUPPORT);
  }
  for (SOCKET s : {a, b}) {
    const int prepared = PrepareHostSocket(s, host_family, host_type);
    if (prepared != 0) {
      closesocket(a);
      closesocket(b);
      return Fail(prepared);
    }
  }
  SocketPtr first;
  SocketPtr second;
  std::shared_ptr<PairChannel> channel;
  try {
    if (host_type == SOCK_DGRAM) channel = std::make_shared<PairChannel>();
    first = RegisterSocket(a, guest::kAF_UNIX, guest_type, 0, host_family);
    second = RegisterSocket(b, guest::kAF_UNIX, guest_type, 0, host_family);
  } catch (...) {
    TakeSocket(a);
    closesocket(a);
    closesocket(b);
    return Fail(guest::kENOMEM);
  }
  int end = 0;
  for (const SocketPtr& state : {first, second}) {
    state->emulated_unix = host_family != AF_UNIX || host_type != SOCK_STREAM;
    state->phase.store(SockPhase::kConnected);
    state->bound.store(true);
    if (host_type == SOCK_DGRAM) {
      BOOL report = TRUE;
      DWORD bytes = 0;
      WSAIoctl(state->socket, SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &bytes, nullptr, nullptr);
      state->connreset_enabled.store(true);
      const int buffer = PairChannel::kHostReceiveBuffer;
      setsockopt(state->socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&buffer), sizeof(buffer));
      state->pair = channel;
      state->pair_end = end;
      channel->Attach(end, state);
      g_pair_sockets.fetch_add(1);
    }
    ++end;
  }
  const int fd0 = InstallSocket(a, first, flags);
  if (fd0 < 0) {
    TakeSocket(b);
    closesocket(b);
    return -1;
  }
  const int fd1 = InstallSocket(b, second, flags);
  if (fd1 < 0) {
    const int error = GuestErrno();
    GuestClose(fd0);
    return Fail(error);
  }
  sv[0] = fd0;
  sv[1] = fd1;
  return 0;
}

int BindImpl(int fd, const void* address, g_socklen length) {
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  SocketState& state = *ref.state;
  HostAddr host;
  std::string unix_name;
  const int translated = GuestToHostAddr(state, address, length, AddrUse::kBind, &host, &unix_name);
  if (translated != 0) return Fail(translated);
  if (state.domain != guest::kAF_UNIX) {
    const std::uint16_t port = PortOf(host);
    if (port != 0 && port < 1024) return Fail(guest::kEACCES);
  } else {
    if (state.bound.load()) return Fail(guest::kEINVAL);
    const std::wstring path = HostPathOf(host);
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
      if (UnixAddressInUse(path)) return Fail(guest::kEADDRINUSE);
      DeleteFileW(path.c_str());
    }
  }
  if (bind(ref.s, host.get(), host.length) != 0) {
    const int error = WSAGetLastError();
    return Fail(error == WSAEINVAL ? guest::kEINVAL : SocketErrno(error, &state, false));
  }
  state.bound.store(true);
  if (state.domain == guest::kAF_UNIX) {
    {
      std::lock_guard<std::mutex> guard(state.name_lock);
      state.unix_name = std::move(unix_name);
      state.unix_cleanup = HostPathOf(host);
    }
    RememberUnixBinding(HostPathOf(host), ref.state);
  }
  return 0;
}

int ListenImpl(int fd, int backlog) {
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  SocketState& state = *ref.state;
  if (state.type != guest::kSOCK_STREAM || state.emulated_unix) return Fail(guest::kEOPNOTSUPP);
  const SockPhase phase = state.phase.load();
  if (phase == SockPhase::kConnected || phase == SockPhase::kConnecting) return Fail(guest::kEINVAL);
  if (!state.bound.load()) {
    if (state.domain == guest::kAF_UNIX) return Fail(guest::kEINVAL);
    SOCKADDR_STORAGE any{};
    any.ss_family = static_cast<ADDRESS_FAMILY>(state.host_family);
    const int any_length = state.host_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
    if (bind(ref.s, reinterpret_cast<sockaddr*>(&any), any_length) != 0) {
      return Fail(SocketErrno(WSAGetLastError(), &state, false));
    }
    state.bound.store(true);
  }
  if (backlog < 0) backlog = 0;
  if (backlog > 4096) backlog = 4096;
  if (listen(ref.s, backlog) != 0) return Fail(SocketErrno(WSAGetLastError(), &state, false));
  state.phase.store(SockPhase::kListening);
  state.accept_shutdown.store(false);
  return 0;
}

int AcceptImpl(int fd, void* address, g_socklen* length, int flags) {
  if ((flags & ~(guest::kSOCK_NONBLOCK | guest::kSOCK_CLOEXEC)) != 0) return Fail(guest::kEINVAL);
  if (address != nullptr && length == nullptr) return Fail(guest::kEFAULT);
  if (address != nullptr && static_cast<std::int32_t>(*length) < 0) return Fail(guest::kEINVAL);
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  SocketState& state = *ref.state;
  if (state.type != guest::kSOCK_STREAM) return Fail(guest::kEOPNOTSUPP);
  if (state.phase.load() != SockPhase::kListening || state.accept_shutdown.load()) return Fail(guest::kEINVAL);

  HostAddr peer;
  SOCKET accepted = INVALID_SOCKET;
  for (;;) {
    peer.length = sizeof(peer.storage);
    accepted = accept(ref.s, peer.get(), &peer.length);
    if (accepted != INVALID_SOCKET) break;
    const int error = WSAGetLastError();
    if (error == WSAECONNRESET) continue;
    if (error != WSAEWOULDBLOCK) {
      return Fail(error == WSAEINVAL ? guest::kEINVAL : SocketErrno(error, &state, false));
    }
    NoteWouldBlock(&state, false);
    if (FdIsNonBlocking(fd, &state)) return Fail(guest::kEAGAIN);
    const int waited = WaitSocket(ref.s, POLLRDNORM, state.rcv_timeout_us.load(), &state.accept_shutdown);
    if (waited != 0) return Fail(waited);
    if (state.accept_shutdown.load()) return Fail(guest::kEINVAL);
  }
  PrepareHostSocket(accepted, state.host_family, SOCK_STREAM);
  SocketPtr child;
  try {
    child = RegisterSocket(accepted, state.domain, state.type, state.protocol, state.host_family);
  } catch (...) {
    closesocket(accepted);
    return Fail(guest::kENOMEM);
  }
  child->phase.store(SockPhase::kConnected);
  child->bound.store(true);
  guest::SockaddrStorage guest_peer;
  std::uint32_t guest_length = 0;
  if (state.domain == guest::kAF_UNIX) {
    {
      std::lock_guard<std::mutex> guard(state.name_lock);
      child->unix_name = state.unix_name;
    }
    guest_length = UnixSockaddr(std::string(), &guest_peer);
  } else {
    guest_length = HostToGuestSockaddr(peer.get(), peer.length, &guest_peer);
  }
  const int new_fd = InstallSocket(accepted, child, flags);
  if (new_fd < 0) return -1;
  if (address != nullptr) {
    const int copied = CopySockaddrOut(guest_peer, guest_length, address, length);
    if (copied != 0) {
      GuestClose(new_fd);
      return Fail(copied);
    }
  }
  return new_fd;
}

int ConnectImpl(int fd, const void* address, g_socklen length) {
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  SocketState& state = *ref.state;
  if (address == nullptr) return Fail(guest::kEFAULT);

  if (length >= sizeof(std::uint16_t)) {
    std::uint16_t family;
    std::memcpy(&family, address, sizeof(family));
    if (family == guest::kAF_UNSPEC && state.type == guest::kSOCK_DGRAM && !state.emulated_unix) {
      SOCKADDR_STORAGE any{};
      any.ss_family = static_cast<ADDRESS_FAMILY>(state.host_family);
      const int any_length = state.host_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
      connect(ref.s, reinterpret_cast<sockaddr*>(&any), any_length);
      if (state.connreset_enabled.exchange(false)) {
        BOOL report = FALSE;
        DWORD bytes = 0;
        WSAIoctl(ref.s, SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &bytes, nullptr, nullptr);
      }
      state.phase.store(SockPhase::kFresh);
      return 0;
    }
  }
  if (state.emulated_unix) return Fail(guest::kEISCONN);

  if (state.phase.load() == SockPhase::kConnecting) {
    const auto attempt = state.tcp_connect_attempt.load(std::memory_order_relaxed);
    const int pending = PendingError(ref.s);
    if (pending != 0 && pending != state.reported_so_error.load()) {
      state.reported_so_error.store(pending);
      state.phase.store(SockPhase::kFresh);
      const int guest_error = SocketErrno(pending, &state, false);
      CompleteTcpConnectTrace(state, attempt, guest_error);
      return Fail(guest_error);
    }
  }

  HostAddr host;
  std::string unix_name;
  const int translated = GuestToHostAddr(state, address, length, AddrUse::kConnect, &host, &unix_name);
  if (translated != 0) return Fail(translated);
  if (state.domain == guest::kAF_UNIX) {
    const std::wstring path = HostPathOf(host);
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return Fail(guest::kENOENT);
  }
  if (state.type == guest::kSOCK_STREAM && state.phase.load() == SockPhase::kFresh) {
    FastLoopbackRefusal(ref.s, host.get());
  }
  if (state.type == guest::kSOCK_DGRAM && !state.connreset_enabled.load()) {
    BOOL report = TRUE;
    DWORD bytes = 0;
    WSAIoctl(ref.s, SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &bytes, nullptr, nullptr);
    state.connreset_enabled.store(true);
  }
  auto remember_peer = [&] {
    if (state.domain == guest::kAF_UNIX) {
      std::lock_guard<std::mutex> guard(state.name_lock);
      state.unix_peer = unix_name;
    }
  };
  auto attempt = state.tcp_connect_attempt.load(std::memory_order_relaxed);
  if (connect(ref.s, host.get(), host.length) == 0) {
    state.phase.store(SockPhase::kConnected);
    state.bound.store(true);
    remember_peer();
    TraceTcpConnectReturn(state, attempt, 0);
    return 0;
  }
  const int error = WSAGetLastError();
  switch (error) {
    case WSAEWOULDBLOCK:
      break;
    case WSAEISCONN:
      if (state.phase.load() == SockPhase::kConnecting) {
        state.phase.store(SockPhase::kConnected);
        CompleteTcpConnectTrace(state, attempt, 0);
        return 0;
      }
      return Fail(guest::kEISCONN);
    case WSAEALREADY:
      return Fail(guest::kEALREADY);
    case WSAEINVAL: {
      const int guest_error = state.phase.load() == SockPhase::kListening ? guest::kEISCONN : guest::kEINVAL;
      if (attempt == 0 && guest_error != guest::kEISCONN) TraceTcpConnectReturn(state, 0, guest_error);
      return Fail(guest_error);
    }
    default: {
      const int guest_error = SocketErrno(error, &state, false);
      TraceTcpConnectReturn(state, attempt, guest_error);
      return Fail(guest_error);
    }
  }
  state.phase.store(SockPhase::kConnecting);
  state.bound.store(true);
  state.reported_so_error.store(0);
  remember_peer();
  if (state.domain == guest::kAF_UNIX && FdIsNonBlocking(fd, &state)) {
    const int waited = WaitSocket(ref.s, POLLWRNORM, 200000);
    const int pending = waited == 0 ? PendingError(ref.s) : 0;
    if (waited != 0) return Fail(waited == guest::kEAGAIN ? guest::kEAGAIN : waited);
    if (pending != 0) {
      state.phase.store(SockPhase::kFresh);
      return Fail(pending == WSAECONNREFUSED ? guest::kECONNREFUSED : SocketErrno(pending, &state, false));
    }
    state.phase.store(SockPhase::kConnected);
    return 0;
  }
  bool started_trace = false;
  if (attempt == 0) {
    const auto new_attempt = NextTcpConnectAttempt(state);
    if (new_attempt != 0) {
      started_trace = state.tcp_connect_attempt.compare_exchange_strong(attempt, new_attempt, std::memory_order_relaxed);
      if (started_trace) attempt = new_attempt;
    }
  }
  if (FdIsNonBlocking(fd, &state)) {
    if (started_trace) WriteTcpConnectTrace(attempt, "pending", guest::kEINPROGRESS);
    return Fail(guest::kEINPROGRESS);
  }
  const int waited = WaitSocket(ref.s, POLLWRNORM, state.snd_timeout_us.load());
  if (waited != 0) {
    if (started_trace) WriteTcpConnectTrace(attempt, "pending", guest::kEINPROGRESS);
    return Fail(waited == guest::kEAGAIN ? guest::kEINPROGRESS : waited);
  }
  bool observed = false;
  const int pending = PendingError(ref.s, &observed);
  if (pending != 0) {
    state.reported_so_error.store(pending);
    state.phase.store(SockPhase::kFresh);
    const int guest_error = SocketErrno(pending, &state, false);
    CompleteTcpConnectTrace(state, attempt, guest_error);
    return Fail(guest_error);
  }
  state.phase.store(SockPhase::kConnected);
  if (observed) {
    CompleteTcpConnectTrace(state, attempt, 0);
  } else if (started_trace) {
    WriteTcpConnectTrace(attempt, "pending", guest::kEINPROGRESS);
  }
  return 0;
}

int NameImpl(int fd, void* address, g_socklen* length, bool peer) {
  if (address == nullptr || length == nullptr) return Fail(guest::kEFAULT);
  if (static_cast<std::int32_t>(*length) < 0) return Fail(guest::kEINVAL);
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  SocketState& state = *ref.state;
  guest::SockaddrStorage value;
  std::uint32_t value_length = 0;
  if (state.domain == guest::kAF_UNIX) {
    if (peer && state.phase.load() != SockPhase::kConnected) return Fail(guest::kENOTCONN);
    std::lock_guard<std::mutex> guard(state.name_lock);
    value_length = UnixSockaddr(peer ? state.unix_peer : state.unix_name, &value);
  } else {
    HostAddr host;
    host.length = sizeof(host.storage);
    const auto attempt = peer ? state.tcp_connect_attempt.load(std::memory_order_relaxed) : 0;
    const int rc = peer ? getpeername(ref.s, host.get(), &host.length) : getsockname(ref.s, host.get(), &host.length);
    if (rc != 0) {
      const int error = WSAGetLastError();
      if (!peer && error == WSAEINVAL) {
        std::memset(&host.storage, 0, sizeof(host.storage));
        host.storage.ss_family = static_cast<ADDRESS_FAMILY>(state.host_family);
        host.length = state.host_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
      } else {
        return Fail(error == WSAENOTCONN ? guest::kENOTCONN : SocketErrno(error, &state, false));
      }
    } else if (peer) {
      CompleteTcpConnectTrace(state, attempt, 0);
    }
    value_length = HostToGuestSockaddr(host.get(), host.length, &value);
  }
  const int copied = CopySockaddrOut(value, value_length, address, length);
  return copied == 0 ? 0 : Fail(copied);
}

int ShutdownImpl(int fd, int how) {
  if (how < guest::kSHUT_RD || how > guest::kSHUT_RDWR) return Fail(guest::kEINVAL);
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  SocketState& state = *ref.state;
  const SockPhase phase = state.phase.load();
  if (phase == SockPhase::kListening) {
    if (how != guest::kSHUT_WR) {
      state.accept_shutdown.store(true);
      NoteShutdown(&state);
    }
    return 0;
  }
  if (phase != SockPhase::kConnected && phase != SockPhase::kConnecting) {
    if (how != guest::kSHUT_WR) state.shut_rd.store(true);
    if (how != guest::kSHUT_RD) state.shut_wr.store(true);
    NoteShutdown(&state);
    return Fail(guest::kENOTCONN);
  }
  if (how != guest::kSHUT_RD) {
    if (shutdown(ref.s, SD_SEND) != 0) return Fail(SocketErrno(WSAGetLastError(), &state, false));
    state.shut_wr.store(true);
  }
  if (how != guest::kSHUT_WR) state.shut_rd.store(true);
  NoteShutdown(&state);
  return 0;
}

}

void CompleteTcpConnectTrace(SocketState& state, std::uint64_t attempt, int guest_errno) noexcept {
  if (attempt == 0 || !state.tcp_connect_attempt.compare_exchange_strong(attempt, 0, std::memory_order_relaxed)) return;
  WriteTcpConnectTrace(attempt, guest_errno == 0 ? "connected" : "failed", guest_errno);
}

ELIXIR_BIONIC_FN(int, socket)(int domain, int type, int protocol) noexcept {
  return Guarded(-1, [&] { return CreateSocket(domain, type, protocol); });
}
ELIXIR_BIONIC_FN(int, socketpair)(int domain, int type, int protocol, int* sv) noexcept {
  return Guarded(-1, [&] { return SocketPairImpl(domain, type, protocol, sv); });
}
ELIXIR_BIONIC_FN(int, bind)(int fd, const void* address, g_socklen length) noexcept {
  return Guarded(-1, [&] { return BindImpl(fd, address, length); });
}
ELIXIR_BIONIC_FN(int, listen)(int fd, int backlog) noexcept {
  return Guarded(-1, [&] { return ListenImpl(fd, backlog); });
}
ELIXIR_BIONIC_FN(int, accept)(int fd, void* address, g_socklen* length) noexcept {
  return Guarded(-1, [&] { return AcceptImpl(fd, address, length, 0); });
}
ELIXIR_BIONIC_FN(int, accept4)(int fd, void* address, g_socklen* length, int flags) noexcept {
  return Guarded(-1, [&] { return AcceptImpl(fd, address, length, flags); });
}
ELIXIR_BIONIC_FN(int, connect)(int fd, const void* address, g_socklen length) noexcept {
  return Guarded(-1, [&] { return ConnectImpl(fd, address, length); });
}
ELIXIR_BIONIC_FN(int, getsockname)(int fd, void* address, g_socklen* length) noexcept {
  return Guarded(-1, [&] { return NameImpl(fd, address, length, false); });
}
ELIXIR_BIONIC_FN(int, getpeername)(int fd, void* address, g_socklen* length) noexcept {
  return Guarded(-1, [&] { return NameImpl(fd, address, length, true); });
}
ELIXIR_BIONIC_FN(int, shutdown)(int fd, int how) noexcept {
  return Guarded(-1, [&] { return ShutdownImpl(fd, how); });
}
ELIXIR_BIONIC_FN(int, bindresvport)(int fd, void* address) noexcept {
  return Guarded(-1, [&] {
    SocketRef ref;
    if (!ResolveSocket(fd, &ref, true)) return -1;
    if (address != nullptr) {
      std::uint16_t family;
      std::memcpy(&family, address, sizeof(family));
      if (family != guest::kAF_INET) return Fail(guest::kEPFNOSUPPORT);
    }
    return Fail(guest::kEACCES);
  });
}

void RegisterSocketExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("socket", &elixir_bionic_socket);
  add("socketpair", &elixir_bionic_socketpair);
  add("bind", &elixir_bionic_bind);
  add("listen", &elixir_bionic_listen);
  add("accept", &elixir_bionic_accept);
  add("accept4", &elixir_bionic_accept4);
  add("connect", &elixir_bionic_connect);
  add("getsockname", &elixir_bionic_getsockname);
  add("getpeername", &elixir_bionic_getpeername);
  add("shutdown", &elixir_bionic_shutdown);
  add("bindresvport", &elixir_bionic_bindresvport);
}

}
