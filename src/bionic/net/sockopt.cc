// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <atomic>
#include <cerrno>
#include <cstring>

namespace elixir::bionic::net::internal {
namespace {

int Fail(int error) {
  SetGuestErrno(error);
  return -1;
}

int ReadInt(const void* value, g_socklen length, int* out) {
  if (static_cast<std::int32_t>(length) < static_cast<std::int32_t>(sizeof(int))) return guest::kEINVAL;
  if (value == nullptr) return guest::kEFAULT;
  std::memcpy(out, value, sizeof(int));
  return 0;
}

int ReadIpInt(const void* value, g_socklen length, int* out) {
  if (static_cast<std::int32_t>(length) >= static_cast<std::int32_t>(sizeof(int))) return ReadInt(value, length, out);
  if (static_cast<std::int32_t>(length) >= 1) {
    if (value == nullptr) return guest::kEFAULT;
    *out = *static_cast<const unsigned char*>(value);
    return 0;
  }
  return guest::kEINVAL;
}

int WriteBytes(void* value, g_socklen* length, const void* data, std::uint32_t size) {
  if (length == nullptr) return guest::kEFAULT;
  const std::int32_t room = static_cast<std::int32_t>(*length);
  if (room < 0) return guest::kEINVAL;
  const std::uint32_t copy = static_cast<std::uint32_t>(room) < size ? static_cast<std::uint32_t>(room) : size;
  if (copy > 0) {
    if (value == nullptr) return guest::kEFAULT;
    std::memcpy(value, data, copy);
  }
  *length = copy;
  return 0;
}

int WriteInt(void* value, g_socklen* length, int v) { return WriteBytes(value, length, &v, sizeof(v)); }

int WriteIpInt(void* value, g_socklen* length, int v) {
  if (length != nullptr && static_cast<std::int32_t>(*length) > 0 &&
      static_cast<std::int32_t>(*length) < static_cast<std::int32_t>(sizeof(int)) && v >= 0 && v <= 255) {
    const unsigned char byte = static_cast<unsigned char>(v);
    return WriteBytes(value, length, &byte, 1);
  }
  return WriteInt(value, length, v);
}

int HostSet(const SocketState& state, int level, int name, const void* value, int size) {
  if (setsockopt(state.socket, level, name, static_cast<const char*>(value), size) != 0) {
    const int error = WSAGetLastError();
    return error == WSAENOPROTOOPT ? guest::kENOPROTOOPT : SocketErrno(error, &state, false);
  }
  return 0;
}

int HostSetInt(const SocketState& state, int level, int name, int v) { return HostSet(state, level, name, &v, sizeof(v)); }

int HostGetInt(const SocketState& state, int level, int name, int* out) {
  int v = 0;
  int size = sizeof(v);
  if (getsockopt(state.socket, level, name, reinterpret_cast<char*>(&v), &size) != 0) {
    const int error = WSAGetLastError();
    return error == WSAENOPROTOOPT ? guest::kENOPROTOOPT : SocketErrno(error, &state, false);
  }
  if (size == 1) v &= 0xff;
  *out = v;
  return 0;
}

bool IsInet(const SocketState& state) {
  return (state.domain == guest::kAF_INET || state.domain == guest::kAF_INET6) && !state.emulated_unix;
}

int MtuDiscoverToHost(int mode, int* out) {
  switch (mode) {
    case guest::kIP_PMTUDISC_DONT: *out = IP_PMTUDISC_DONT; return 0;
    case guest::kIP_PMTUDISC_WANT: *out = IP_PMTUDISC_NOT_SET; return 0;
    case guest::kIP_PMTUDISC_DO: *out = IP_PMTUDISC_DO; return 0;
    case guest::kIP_PMTUDISC_PROBE: *out = IP_PMTUDISC_PROBE; return 0;
    case guest::kIP_PMTUDISC_INTERFACE:
    case guest::kIP_PMTUDISC_OMIT: *out = IP_PMTUDISC_DONT; return 0;
    default: return guest::kEINVAL;
  }
}

int GuestFromMtuDiscover(int host) {
  switch (host) {
    case IP_PMTUDISC_DO: return guest::kIP_PMTUDISC_DO;
    case IP_PMTUDISC_DONT: return guest::kIP_PMTUDISC_DONT;
    case IP_PMTUDISC_PROBE: return guest::kIP_PMTUDISC_PROBE;
    default: return guest::kIP_PMTUDISC_WANT;
  }
}

int TimeoutIn(SocketState& state, std::atomic<std::int64_t>* slot, const void* value, g_socklen length) {
  if (static_cast<std::int32_t>(length) < static_cast<std::int32_t>(sizeof(guest::Timeval))) return guest::kEINVAL;
  if (value == nullptr) return guest::kEFAULT;
  guest::Timeval tv;
  std::memcpy(&tv, value, sizeof(tv));
  if (tv.tv_usec < 0 || tv.tv_usec >= 1000000) return guest::kEDOM;
  if (tv.tv_sec < 0) {
    slot->store(0);
    return 0;
  }
  constexpr std::int64_t kMaxSeconds = INT64_MAX / 2000000;
  const std::int64_t us = tv.tv_sec > kMaxSeconds ? INT64_MAX / 2 : tv.tv_sec * 1000000 + tv.tv_usec;
  slot->store(us);
  (void)state;
  return 0;
}

int TimeoutOut(const std::atomic<std::int64_t>& slot, void* value, g_socklen* length) {
  const std::int64_t us = slot.load();
  guest::Timeval tv{us / 1000000, us % 1000000};
  return WriteBytes(value, length, &tv, sizeof(tv));
}

int SetHint(SocketState& state, SocketHint hint, const void* value, g_socklen length) {
  int v;
  const int read = ReadInt(value, length, &v);
  if (read != 0) return read;
  state.hints[hint].store(v);
  return 0;
}

int SetSocketLevel(SocketState& state, int name, const void* value, g_socklen length) {
  int v = 0;
  switch (name) {
    case guest::kSO_REUSEADDR:
    case guest::kSO_REUSEPORT: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      state.hints[name == guest::kSO_REUSEADDR ? kHintReuseAddr : kHintReusePort].store(v != 0 ? 1 : 0);
      if (state.type == guest::kSOCK_DGRAM && IsInet(state)) {
        const int any = (state.hints[kHintReuseAddr].load() | state.hints[kHintReusePort].load()) != 0;
        return HostSetInt(state, SOL_SOCKET, SO_REUSEADDR, any);
      }
      return 0;
    }
    case guest::kSO_KEEPALIVE:
    case guest::kSO_BROADCAST:
    case guest::kSO_OOBINLINE:
    case guest::kSO_DONTROUTE: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      if (state.host_family == AF_UNIX) return 0;
      const int host = name == guest::kSO_KEEPALIVE    ? SO_KEEPALIVE
                       : name == guest::kSO_BROADCAST  ? SO_BROADCAST
                       : name == guest::kSO_OOBINLINE  ? SO_OOBINLINE
                                                       : SO_DONTROUTE;
      return HostSetInt(state, SOL_SOCKET, host, v != 0 ? 1 : 0);
    }
    case guest::kSO_LINGER: {
      if (static_cast<std::int32_t>(length) < static_cast<std::int32_t>(sizeof(guest::Linger))) return guest::kEINVAL;
      if (value == nullptr) return guest::kEFAULT;
      guest::Linger in;
      std::memcpy(&in, value, sizeof(in));
      state.linger_on.store(in.l_onoff != 0 ? 1 : 0);
      if (in.l_onoff != 0) state.linger_seconds.store(in.l_linger < 0 ? 0 : in.l_linger);
      if (state.type != guest::kSOCK_STREAM || state.host_family == AF_UNIX) return 0;
      LINGER host{};
      host.l_onoff = in.l_onoff != 0 ? 1 : 0;
      host.l_linger = static_cast<u_short>(in.l_linger < 0 ? 0 : (in.l_linger > 65535 ? 65535 : in.l_linger));
      return HostSet(state, SOL_SOCKET, SO_LINGER, &host, sizeof(host));
    }
    case guest::kSO_RCVBUF:
    case guest::kSO_SNDBUF: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      const int minimum = name == guest::kSO_RCVBUF ? 2304 : 4608;
      long long doubled = static_cast<long long>(v < 0 ? 0 : v) * 2;
      if (doubled > 0x3fffffff) doubled = 0x3fffffff;
      if (doubled < minimum) doubled = minimum;
      if (state.pair != nullptr) {
        state.hints[name == guest::kSO_RCVBUF ? kHintRcvBuf : kHintSndBuf].store(static_cast<int>(doubled));
        return 0;
      }
      return HostSetInt(state, SOL_SOCKET, name == guest::kSO_RCVBUF ? SO_RCVBUF : SO_SNDBUF,
                        static_cast<int>(doubled));
    }
    case guest::kSO_RCVBUFFORCE:
    case guest::kSO_SNDBUFFORCE:
    case guest::kSO_MARK:
    case guest::kSO_BINDTODEVICE:
    case guest::kSO_BINDTOIFINDEX:
      return guest::kEPERM;
    case guest::kSO_RCVTIMEO_OLD:
    case guest::kSO_RCVTIMEO_NEW:
      return TimeoutIn(state, &state.rcv_timeout_us, value, length);
    case guest::kSO_SNDTIMEO_OLD:
    case guest::kSO_SNDTIMEO_NEW:
      return TimeoutIn(state, &state.snd_timeout_us, value, length);
    case guest::kSO_RCVLOWAT: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      if (v > 1) return guest::kENOPROTOOPT;
      state.rcvlowat.store(1);
      return 0;
    }
    case guest::kSO_SNDLOWAT:
      return guest::kENOPROTOOPT;
    case guest::kSO_PRIORITY: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      if (v < 0 || v > 6) return guest::kEPERM;
      state.hints[kHintPriority].store(v);
      return 0;
    }
    case guest::kSO_PASSCRED:
      return SetHint(state, kHintPassCred, value, length);
    case guest::kSO_DEBUG: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      return v != 0 ? guest::kEACCES : 0;
    }
    case guest::kSO_BSDCOMPAT:
      return ReadInt(value, length, &v);
    default:
      return guest::kENOPROTOOPT;
  }
}

int GetSocketLevel(SocketState& state, int name, void* value, g_socklen* length) {
  int v = 0;
  switch (name) {
    case guest::kSO_TYPE: return WriteInt(value, length, state.type);
    case guest::kSO_DOMAIN: return WriteInt(value, length, state.domain);
    case guest::kSO_PROTOCOL: return WriteInt(value, length, state.protocol);
    case guest::kSO_ACCEPTCONN: return WriteInt(value, length, state.phase.load() == SockPhase::kListening ? 1 : 0);
    case guest::kSO_ERROR: {
      const auto attempt = state.tcp_connect_attempt.load(std::memory_order_relaxed);
      int pending = 0;
      int size = sizeof(pending);
      const int received = getsockopt(state.socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&pending), &size);
      int result = 0;
      if (pending != 0 && pending != state.reported_so_error.load()) {
        state.reported_so_error.store(pending);
        result = SocketErrno(pending, &state, false);
        if (received == 0) CompleteTcpConnectTrace(state, attempt, result);
      }
      return WriteInt(value, length, result);
    }
    case guest::kSO_REUSEADDR: return WriteInt(value, length, state.hints[kHintReuseAddr].load());
    case guest::kSO_REUSEPORT: return WriteInt(value, length, state.hints[kHintReusePort].load());
    case guest::kSO_KEEPALIVE:
    case guest::kSO_BROADCAST:
    case guest::kSO_OOBINLINE:
    case guest::kSO_DONTROUTE: {
      if (state.host_family == AF_UNIX) return WriteInt(value, length, 0);
      const int host = name == guest::kSO_KEEPALIVE    ? SO_KEEPALIVE
                       : name == guest::kSO_BROADCAST  ? SO_BROADCAST
                       : name == guest::kSO_OOBINLINE  ? SO_OOBINLINE
                                                       : SO_DONTROUTE;
      const int got = HostGetInt(state, SOL_SOCKET, host, &v);
      if (got != 0) return got;
      return WriteInt(value, length, v != 0 ? 1 : 0);
    }
    case guest::kSO_LINGER: {
      if (state.type != guest::kSOCK_STREAM || state.host_family == AF_UNIX) {
        const guest::Linger recorded{state.linger_on.load(), state.linger_seconds.load()};
        return WriteBytes(value, length, &recorded, sizeof(recorded));
      }
      LINGER host{};
      int size = sizeof(host);
      if (getsockopt(state.socket, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&host), &size) != 0) {
        return SocketErrno(WSAGetLastError(), &state, false);
      }
      guest::Linger out{host.l_onoff != 0 ? 1 : 0, host.l_linger};
      return WriteBytes(value, length, &out, sizeof(out));
    }
    case guest::kSO_RCVBUF:
    case guest::kSO_SNDBUF: {
      if (state.pair != nullptr) {
        const int recorded = state.hints[name == guest::kSO_RCVBUF ? kHintRcvBuf : kHintSndBuf].load();
        return WriteInt(value, length, recorded != 0 ? recorded : static_cast<int>(PairChannel::kBudget));
      }
      const int got = HostGetInt(state, SOL_SOCKET, name == guest::kSO_RCVBUF ? SO_RCVBUF : SO_SNDBUF, &v);
      if (got != 0) return got;
      return WriteInt(value, length, v);
    }
    case guest::kSO_RCVTIMEO_OLD:
    case guest::kSO_RCVTIMEO_NEW:
      return TimeoutOut(state.rcv_timeout_us, value, length);
    case guest::kSO_SNDTIMEO_OLD:
    case guest::kSO_SNDTIMEO_NEW:
      return TimeoutOut(state.snd_timeout_us, value, length);
    case guest::kSO_RCVLOWAT: return WriteInt(value, length, state.rcvlowat.load());
    case guest::kSO_SNDLOWAT: return WriteInt(value, length, 1);
    case guest::kSO_PRIORITY: return WriteInt(value, length, state.hints[kHintPriority].load());
    case guest::kSO_PASSCRED: return WriteInt(value, length, state.hints[kHintPassCred].load() != 0 ? 1 : 0);
    case guest::kSO_DEBUG:
    case guest::kSO_BSDCOMPAT:
      return WriteInt(value, length, 0);
    case guest::kSO_PEERCRED: {
      GuestUcred credentials{0, 65534, 65534};
      if (state.domain == guest::kAF_UNIX) {
        if (state.phase.load() != SockPhase::kConnected) return guest::kENOTCONN;
        credentials = {static_cast<std::int32_t>(GetCurrentProcessId()), kGuestUid, kGuestUid};
      }
      return WriteBytes(value, length, &credentials, sizeof(credentials));
    }
    case guest::kSO_COOKIE: {
      if (length == nullptr) return guest::kEFAULT;
      if (static_cast<std::int32_t>(*length) < 8) return guest::kEINVAL;
      const std::uint64_t cookie = state.serial;
      return WriteBytes(value, length, &cookie, sizeof(cookie));
    }
    default:
      return guest::kENOPROTOOPT;
  }
}

struct GuestTcpInfo {
  std::uint8_t state, ca_state, retransmits, probes, backoff, options, wscale, app_limited;
  std::uint32_t rto, ato, snd_mss, rcv_mss, unacked, sacked, lost, retrans, fackets;
  std::uint32_t last_data_sent, last_ack_sent, last_data_recv, last_ack_recv;
  std::uint32_t pmtu, rcv_ssthresh, rtt, rttvar, snd_ssthresh, snd_cwnd, advmss, reordering;
  std::uint32_t rcv_rtt, rcv_space, total_retrans;
  std::uint64_t pacing_rate, max_pacing_rate, bytes_acked, bytes_received;
  std::uint32_t segs_out, segs_in, notsent_bytes, min_rtt, data_segs_in, data_segs_out;
  std::uint64_t delivery_rate, busy_time, rwnd_limited, sndbuf_limited;
  std::uint32_t delivered, delivered_ce;
  std::uint64_t bytes_sent, bytes_retrans;
  std::uint32_t dsack_dups, reord_seen, rcv_ooopack, snd_wnd;
};
static_assert(sizeof(GuestTcpInfo) == 232, "struct tcp_info");
static_assert(offsetof(GuestTcpInfo, rtt) == 68, "tcpi_rtt");
static_assert(offsetof(GuestTcpInfo, bytes_acked) == 120, "tcpi_bytes_acked");
static_assert(offsetof(GuestTcpInfo, snd_wnd) == 228, "tcpi_snd_wnd");

std::uint8_t LinuxTcpState(int host_state) {
  switch (host_state) {
    case 1: return 10;
    case 2: return 2;
    case 3: return 3;
    case 4: return 1;
    case 5: return 4;
    case 6: return 5;
    case 7: return 8;
    case 8: return 11;
    case 9: return 9;
    case 10: return 6;
    default: return 7;
  }
}

int TcpInfo(SocketState& state, void* value, g_socklen* length) {
  GuestTcpInfo info{};
  const SockPhase phase = state.phase.load();
  info.state = phase == SockPhase::kListening ? 10 : phase == SockPhase::kConnecting ? 2
               : phase == SockPhase::kConnected ? 1 : 7;
  if (phase == SockPhase::kConnected || phase == SockPhase::kConnecting) {
    DWORD version = 0;
    TCP_INFO_v0 host{};
    DWORD bytes = 0;
    if (WSAIoctl(state.socket, SIO_TCP_INFO, &version, sizeof(version), &host, sizeof(host), &bytes, nullptr,
                 nullptr) == 0) {
      info.state = LinuxTcpState(static_cast<int>(host.State));
      if (phase == SockPhase::kConnecting && host.State == TCPSTATE_ESTABLISHED && Options().log_sink != nullptr) {
        static std::atomic_flag logged = ATOMIC_FLAG_INIT;
        if (!logged.test_and_set(std::memory_order_relaxed)) {
          const DWORD last_error = GetLastError();
          const int host_errno = errno;
          const int guest_errno = GuestErrno();
          HostLog(4, "elixir", "TCP_INFO: Winsock reports an established connection after a nonblocking connect");
          SetGuestErrno(guest_errno);
          errno = host_errno;
          SetLastError(last_error);
        }
      }
      info.snd_mss = host.Mss;
      info.rcv_mss = host.Mss;
      info.advmss = host.Mss;
      info.rtt = host.RttUs;
      info.rttvar = host.RttUs / 2;
      info.min_rtt = host.MinRttUs;
      info.snd_cwnd = host.Mss != 0 ? host.Cwnd / host.Mss : 0;
      info.snd_wnd = host.SndWnd;
      info.rcv_space = host.RcvWnd;
      info.unacked = host.Mss != 0 ? host.BytesInFlight / host.Mss : 0;
      info.total_retrans = host.FastRetrans + host.TimeoutEpisodes;
      info.bytes_acked = host.BytesOut > host.BytesInFlight ? host.BytesOut - host.BytesInFlight : 0;
      info.bytes_sent = host.BytesOut;
      info.bytes_received = host.BytesIn;
      info.bytes_retrans = host.BytesRetrans;
      info.rto = 200000 + 4 * host.RttUs;
      info.pmtu = host.Mss + 40;
      info.reordering = 3;
      info.options = host.TimestampsEnabled ? 1 : 0;
    }
  }
  return WriteBytes(value, length, &info, sizeof(info));
}

int SetTcpLevel(SocketState& state, int name, const void* value, g_socklen length) {
  if (state.type != guest::kSOCK_STREAM || !IsInet(state)) {
    return state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
  }
  int v = 0;
  switch (name) {
    case guest::kTCP_NODELAY: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      return HostSetInt(state, IPPROTO_TCP, TCP_NODELAY, v != 0 ? 1 : 0);
    }
    case guest::kTCP_KEEPIDLE:
    case guest::kTCP_KEEPINTVL:
    case guest::kTCP_KEEPCNT: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      const int limit = name == guest::kTCP_KEEPCNT ? 127 : 32767;
      if (v < 1 || v > limit) return guest::kEINVAL;
      const int host = name == guest::kTCP_KEEPIDLE ? TCP_KEEPIDLE : name == guest::kTCP_KEEPINTVL ? TCP_KEEPINTVL
                                                                                                     : TCP_KEEPCNT;
      return HostSetInt(state, IPPROTO_TCP, host, v);
    }
    case guest::kTCP_MAXSEG: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      if (v != 0 && (v < 88 || v > 32767)) return guest::kEINVAL;
      return HostSetInt(state, IPPROTO_TCP, TCP_MAXSEG, v);
    }
    case guest::kTCP_USER_TIMEOUT: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      if (v < 0) return guest::kEINVAL;
      state.hints[kHintTcpUserTimeout].store(v);
      return v > 0 ? HostSetInt(state, IPPROTO_TCP, TCP_MAXRTMS, v) : 0;
    }
    case guest::kTCP_QUICKACK: return SetHint(state, kHintTcpQuickAck, value, length);
    case guest::kTCP_CORK: return SetHint(state, kHintTcpCork, value, length);
    case guest::kTCP_DEFER_ACCEPT: return SetHint(state, kHintTcpDeferAccept, value, length);
    case guest::kTCP_NOTSENT_LOWAT: return SetHint(state, kHintTcpNotSentLowat, value, length);
    case guest::kTCP_CONGESTION: {
      if (value == nullptr) return guest::kEFAULT;
      char name_buffer[17] = {};
      const std::uint32_t size = length < 16 ? length : 16;
      std::memcpy(name_buffer, value, size);
      return std::strcmp(name_buffer, "cubic") == 0 ? 0 : guest::kENOENT;
    }
    default:
      return guest::kENOPROTOOPT;
  }
}

int GetTcpLevel(SocketState& state, int name, void* value, g_socklen* length) {
  if (state.type != guest::kSOCK_STREAM || !IsInet(state)) {
    return state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
  }
  int v = 0;
  switch (name) {
    case guest::kTCP_NODELAY:
    case guest::kTCP_KEEPIDLE:
    case guest::kTCP_KEEPINTVL:
    case guest::kTCP_KEEPCNT:
    case guest::kTCP_MAXSEG: {
      const int host = name == guest::kTCP_NODELAY     ? TCP_NODELAY
                       : name == guest::kTCP_KEEPIDLE  ? TCP_KEEPIDLE
                       : name == guest::kTCP_KEEPINTVL ? TCP_KEEPINTVL
                       : name == guest::kTCP_KEEPCNT   ? TCP_KEEPCNT
                                                       : TCP_MAXSEG;
      const int got = HostGetInt(state, IPPROTO_TCP, host, &v);
      if (got != 0) {
        if (name == guest::kTCP_MAXSEG) v = 536;
        else return got;
      }
      if (name == guest::kTCP_NODELAY) v = v != 0 ? 1 : 0;
      return WriteInt(value, length, v);
    }
    case guest::kTCP_USER_TIMEOUT: return WriteInt(value, length, state.hints[kHintTcpUserTimeout].load());
    case guest::kTCP_QUICKACK: return WriteInt(value, length, state.hints[kHintTcpQuickAck].load() != 0 ? 1 : 0);
    case guest::kTCP_CORK: return WriteInt(value, length, state.hints[kHintTcpCork].load() != 0 ? 1 : 0);
    case guest::kTCP_DEFER_ACCEPT: return WriteInt(value, length, state.hints[kHintTcpDeferAccept].load());
    case guest::kTCP_NOTSENT_LOWAT: return WriteInt(value, length, state.hints[kHintTcpNotSentLowat].load());
    case guest::kTCP_CONGESTION: {
      static const char kName[16] = "cubic";
      return WriteBytes(value, length, kName, sizeof(kName));
    }
    case guest::kTCP_INFO:
      return TcpInfo(state, value, length);
    default:
      return guest::kENOPROTOOPT;
  }
}

std::uint32_t MulticastInterface(const void* value, g_socklen length) {
  if (length >= sizeof(GuestIpMreqn)) {
    GuestIpMreqn request;
    std::memcpy(&request, value, sizeof(request));
    if (request.ifindex != 0) return htonl(static_cast<std::uint32_t>(request.ifindex));
    return request.address;
  }
  std::uint32_t address;
  std::memcpy(&address, value, sizeof(address));
  return address;
}

int SetIpLevel(SocketState& state, int name, const void* value, g_socklen length) {
  if (!IsInet(state)) return state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
  int v = 0;
  switch (name) {
    case guest::kIP_TOS: {
      const int read = ReadIpInt(value, length, &v);
      if (read != 0) return read;
      return HostSetInt(state, IPPROTO_IP, IP_TOS, v & 0xff);
    }
    case guest::kIP_TTL: {
      const int read = ReadIpInt(value, length, &v);
      if (read != 0) return read;
      if (v == -1) v = 64;
      if (v < 1 || v > 255) return guest::kEINVAL;
      return HostSetInt(state, IPPROTO_IP, IP_TTL, v);
    }
    case guest::kIP_MULTICAST_TTL: {
      const int read = ReadIpInt(value, length, &v);
      if (read != 0) return read;
      if (v == -1) v = 1;
      if (v < 0 || v > 255) return guest::kEINVAL;
      return HostSetInt(state, IPPROTO_IP, IP_MULTICAST_TTL, v);
    }
    case guest::kIP_MULTICAST_LOOP: {
      const int read = ReadIpInt(value, length, &v);
      if (read != 0) return read;
      return HostSetInt(state, IPPROTO_IP, IP_MULTICAST_LOOP, v != 0 ? 1 : 0);
    }
    case guest::kIP_MULTICAST_IF: {
      if (static_cast<std::int32_t>(length) < 4) return guest::kEINVAL;
      if (value == nullptr) return guest::kEFAULT;
      const std::uint32_t host = MulticastInterface(value, length);
      return HostSet(state, IPPROTO_IP, IP_MULTICAST_IF, &host, sizeof(host));
    }
    case guest::kIP_ADD_MEMBERSHIP:
    case guest::kIP_DROP_MEMBERSHIP: {
      if (static_cast<std::int32_t>(length) < 8) return guest::kEINVAL;
      if (value == nullptr) return guest::kEFAULT;
      IP_MREQ request{};
      std::memcpy(&request.imr_multiaddr, value, 4);
      const std::uint32_t iface = MulticastInterface(static_cast<const char*>(value), length);
      if (length >= sizeof(GuestIpMreqn)) {
        std::memcpy(&request.imr_interface, &iface, 4);
      } else {
        std::memcpy(&request.imr_interface, static_cast<const char*>(value) + 4, 4);
      }
      return HostSet(state, IPPROTO_IP, name == guest::kIP_ADD_MEMBERSHIP ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
                     &request, sizeof(request));
    }
    case guest::kIP_ADD_SOURCE_MEMBERSHIP:
    case guest::kIP_DROP_SOURCE_MEMBERSHIP:
    case guest::kIP_BLOCK_SOURCE:
    case guest::kIP_UNBLOCK_SOURCE: {
      if (static_cast<std::int32_t>(length) < static_cast<std::int32_t>(sizeof(GuestIpMreqSource))) {
        return guest::kEINVAL;
      }
      if (value == nullptr) return guest::kEFAULT;
      GuestIpMreqSource in;
      std::memcpy(&in, value, sizeof(in));
      IP_MREQ_SOURCE request{};
      std::memcpy(&request.imr_multiaddr, &in.multiaddr, 4);
      std::memcpy(&request.imr_sourceaddr, &in.sourceaddr, 4);
      std::memcpy(&request.imr_interface, &in.interface_addr, 4);
      const int host = name == guest::kIP_ADD_SOURCE_MEMBERSHIP    ? IP_ADD_SOURCE_MEMBERSHIP
                       : name == guest::kIP_DROP_SOURCE_MEMBERSHIP ? IP_DROP_SOURCE_MEMBERSHIP
                       : name == guest::kIP_BLOCK_SOURCE           ? IP_BLOCK_SOURCE
                                                                   : IP_UNBLOCK_SOURCE;
      return HostSet(state, IPPROTO_IP, host, &request, sizeof(request));
    }
    case guest::kIP_MTU_DISCOVER: {
      const int read = ReadIpInt(value, length, &v);
      if (read != 0) return read;
      int host = 0;
      const int mapped = MtuDiscoverToHost(v, &host);
      if (mapped != 0) return mapped;
      if (state.domain == guest::kAF_INET6) return HostSetInt(state, IPPROTO_IPV6, IPV6_MTU_DISCOVER, host);
      if (HostSetInt(state, IPPROTO_IP, IP_MTU_DISCOVER, host) != 0) {
        return HostSetInt(state, IPPROTO_IP, IP_DONTFRAGMENT, host == IP_PMTUDISC_DO || host == IP_PMTUDISC_PROBE);
      }
      return 0;
    }
    case guest::kIP_UNICAST_IF: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      return HostSetInt(state, IPPROTO_IP, IP_UNICAST_IF, v);
    }
    case guest::kIP_BIND_ADDRESS_NO_PORT: return SetHint(state, kHintBindNoPort, value, length);
    case guest::kIP_RECVERR: return SetHint(state, kHintIpRecvErr, value, length);
    case guest::kIP_MULTICAST_ALL: {
      const int read = ReadIpInt(value, length, &v);
      if (read != 0) return read;
      return 0;
    }
    case guest::kIP_TRANSPARENT:
      return guest::kEPERM;
    default:
      return guest::kENOPROTOOPT;
  }
}

int GetIpLevel(SocketState& state, int name, void* value, g_socklen* length) {
  if (!IsInet(state)) return state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
  int v = 0;
  switch (name) {
    case guest::kIP_TOS:
    case guest::kIP_TTL:
    case guest::kIP_MULTICAST_TTL:
    case guest::kIP_MULTICAST_LOOP: {
      const int host = name == guest::kIP_TOS             ? IP_TOS
                       : name == guest::kIP_TTL           ? IP_TTL
                       : name == guest::kIP_MULTICAST_TTL ? IP_MULTICAST_TTL
                                                          : IP_MULTICAST_LOOP;
      const int got = HostGetInt(state, IPPROTO_IP, host, &v);
      if (got != 0) return got;
      if (name == guest::kIP_MULTICAST_LOOP) v = v != 0 ? 1 : 0;
      return WriteIpInt(value, length, v);
    }
    case guest::kIP_MULTICAST_IF: {
      std::uint32_t address = 0;
      int size = sizeof(address);
      if (getsockopt(state.socket, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<char*>(&address), &size) != 0) {
        return SocketErrno(WSAGetLastError(), &state, false);
      }
      return WriteBytes(value, length, &address, sizeof(address));
    }
    case guest::kIP_MTU_DISCOVER: {
      int host = 0;
      const int got = state.domain == guest::kAF_INET6 ? HostGetInt(state, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &host)
                                                       : HostGetInt(state, IPPROTO_IP, IP_MTU_DISCOVER, &host);
      return WriteIpInt(value, length, got == 0 ? GuestFromMtuDiscover(host) : guest::kIP_PMTUDISC_WANT);
    }
    case guest::kIP_MTU: {
      if (state.phase.load() != SockPhase::kConnected) return guest::kENOTCONN;
      const int got = HostGetInt(state, IPPROTO_IP, IP_MTU, &v);
      if (got != 0) return got;
      return WriteInt(value, length, v);
    }
    case guest::kIP_UNICAST_IF: {
      const int got = HostGetInt(state, IPPROTO_IP, IP_UNICAST_IF, &v);
      if (got != 0) return got;
      return WriteInt(value, length, v);
    }
    case guest::kIP_BIND_ADDRESS_NO_PORT: return WriteInt(value, length, state.hints[kHintBindNoPort].load() != 0);
    case guest::kIP_RECVERR: return WriteInt(value, length, state.hints[kHintIpRecvErr].load() != 0);
    case guest::kIP_MULTICAST_ALL: return WriteInt(value, length, 1);
    default:
      return guest::kENOPROTOOPT;
  }
}

int Ipv6HostName(int name) {
  switch (name) {
    case kIPV6_V6ONLY: return IPV6_V6ONLY;
    case guest::kIPV6_UNICAST_HOPS: return IPV6_UNICAST_HOPS;
    case guest::kIPV6_MULTICAST_IF: return IPV6_MULTICAST_IF;
    case guest::kIPV6_MULTICAST_HOPS: return IPV6_MULTICAST_HOPS;
    case guest::kIPV6_MULTICAST_LOOP: return IPV6_MULTICAST_LOOP;
    case guest::kIPV6_TCLASS: return IPV6_TCLASS;
    case guest::kIPV6_UNICAST_IF: return IPV6_UNICAST_IF;
    case guest::kIPV6_DONTFRAG: return IPV6_DONTFRAG;
    default: return -1;
  }
}

int SetIpv6Level(SocketState& state, int name, const void* value, g_socklen length) {
  if (state.domain != guest::kAF_INET6 || state.emulated_unix) {
    return state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
  }
  int v = 0;
  switch (name) {
    case guest::kIPV6_JOIN_GROUP:
    case guest::kIPV6_LEAVE_GROUP: {
      if (static_cast<std::int32_t>(length) < static_cast<std::int32_t>(sizeof(GuestIpv6Mreq))) return guest::kEINVAL;
      if (value == nullptr) return guest::kEFAULT;
      GuestIpv6Mreq in;
      std::memcpy(&in, value, sizeof(in));
      IPV6_MREQ request{};
      std::memcpy(request.ipv6mr_multiaddr.u.Byte, in.addr, 16);
      request.ipv6mr_interface = static_cast<ULONG>(in.ifindex);
      return HostSet(state, IPPROTO_IPV6, name == guest::kIPV6_JOIN_GROUP ? IPV6_ADD_MEMBERSHIP : IPV6_DROP_MEMBERSHIP,
                     &request, sizeof(request));
    }
    case guest::kIPV6_MTU_DISCOVER: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      int host = 0;
      const int mapped = MtuDiscoverToHost(v, &host);
      if (mapped != 0) return mapped;
      return HostSetInt(state, IPPROTO_IPV6, IPV6_MTU_DISCOVER, host);
    }
    case guest::kIPV6_RECVERR: return SetHint(state, kHintIpRecvErr, value, length);
    default: {
      const int host = Ipv6HostName(name);
      if (host < 0) return guest::kENOPROTOOPT;
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      if (name == guest::kIPV6_UNICAST_HOPS || name == guest::kIPV6_MULTICAST_HOPS) {
        if (v == -1) v = name == guest::kIPV6_UNICAST_HOPS ? 64 : 1;
        if (v < 0 || v > 255) return guest::kEINVAL;
      }
      if (name == kIPV6_V6ONLY || name == guest::kIPV6_MULTICAST_LOOP || name == guest::kIPV6_DONTFRAG) {
        v = v != 0 ? 1 : 0;
      }
      if (name == guest::kIPV6_UNICAST_IF) v = static_cast<int>(ntohl(static_cast<u_long>(v)));
      return HostSetInt(state, IPPROTO_IPV6, host, v);
    }
  }
}

int GetIpv6Level(SocketState& state, int name, void* value, g_socklen* length) {
  if (state.domain != guest::kAF_INET6 || state.emulated_unix) {
    return state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
  }
  int v = 0;
  switch (name) {
    case guest::kIPV6_MTU_DISCOVER: {
      int host = 0;
      const int got = HostGetInt(state, IPPROTO_IPV6, IPV6_MTU_DISCOVER, &host);
      return WriteInt(value, length, got == 0 ? GuestFromMtuDiscover(host) : guest::kIP_PMTUDISC_WANT);
    }
    case guest::kIPV6_MTU: {
      if (state.phase.load() != SockPhase::kConnected) return guest::kENOTCONN;
      const int got = HostGetInt(state, IPPROTO_IPV6, IPV6_MTU, &v);
      if (got != 0) return got;
      return WriteInt(value, length, v);
    }
    case guest::kIPV6_RECVERR: return WriteInt(value, length, state.hints[kHintIpRecvErr].load() != 0);
    default: {
      const int host = Ipv6HostName(name);
      if (host < 0) return guest::kENOPROTOOPT;
      const int got = HostGetInt(state, IPPROTO_IPV6, host, &v);
      if (got != 0) return got;
      if (name == kIPV6_V6ONLY || name == guest::kIPV6_MULTICAST_LOOP || name == guest::kIPV6_DONTFRAG) {
        v = v != 0 ? 1 : 0;
      }
      if (name == guest::kIPV6_UNICAST_IF) v = static_cast<int>(htonl(static_cast<u_long>(v)));
      return WriteInt(value, length, v);
    }
  }
}

int SetUdpLevel(SocketState& state, int name, const void* value, g_socklen length) {
  if (state.type != guest::kSOCK_DGRAM || !IsInet(state)) {
    return state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
  }
  int v = 0;
  switch (name) {
    case kUDP_SEGMENT: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      if (v < 0 || v > 0xffff) return guest::kEINVAL;
      const std::uint32_t previous = state.gso_size.exchange(static_cast<std::uint32_t>(v));
      if (previous == 0 && v != 0) g_gso_sockets.fetch_add(1);
      if (previous != 0 && v == 0) g_gso_sockets.fetch_sub(1);
      return 0;
    }
    case kUDP_GRO: {
      const int read = ReadInt(value, length, &v);
      if (read != 0) return read;
      state.gro.store(v != 0 ? 1 : 0);
      return 0;
    }
    default:
      return guest::kENOPROTOOPT;
  }
}

int GetUdpLevel(SocketState& state, int name, void* value, g_socklen* length) {
  if (state.type != guest::kSOCK_DGRAM || !IsInet(state)) {
    return state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
  }
  switch (name) {
    case kUDP_SEGMENT: return WriteInt(value, length, static_cast<int>(state.gso_size.load()));
    case kUDP_GRO: return WriteInt(value, length, state.gro.load());
    default: return guest::kENOPROTOOPT;
  }
}

int SetSockOptImpl(int fd, int level, int name, const void* value, g_socklen length) {
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  SocketState& state = *ref.state;
  if (static_cast<std::int32_t>(length) < 0) return Fail(guest::kEINVAL);
  int error = guest::kENOPROTOOPT;
  switch (level) {
    case guest::kSOL_SOCKET: error = SetSocketLevel(state, name, value, length); break;
    case guest::kIPPROTO_TCP: error = SetTcpLevel(state, name, value, length); break;
    case guest::kIPPROTO_IP: error = SetIpLevel(state, name, value, length); break;
    case guest::kIPPROTO_IPV6: error = SetIpv6Level(state, name, value, length); break;
    case guest::kSOL_UDP: error = SetUdpLevel(state, name, value, length); break;
    default:
      error = state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
      break;
  }
  return error == 0 ? 0 : Fail(error);
}

int GetSockOptImpl(int fd, int level, int name, void* value, g_socklen* length) {
  if (length == nullptr) return Fail(guest::kEFAULT);
  if (static_cast<std::int32_t>(*length) < 0) return Fail(guest::kEINVAL);
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  SocketState& state = *ref.state;
  int error = guest::kENOPROTOOPT;
  switch (level) {
    case guest::kSOL_SOCKET: error = GetSocketLevel(state, name, value, length); break;
    case guest::kIPPROTO_TCP: error = GetTcpLevel(state, name, value, length); break;
    case guest::kIPPROTO_IP: error = GetIpLevel(state, name, value, length); break;
    case guest::kIPPROTO_IPV6: error = GetIpv6Level(state, name, value, length); break;
    case guest::kSOL_UDP: error = GetUdpLevel(state, name, value, length); break;
    default:
      error = state.domain == guest::kAF_UNIX ? guest::kEOPNOTSUPP : guest::kENOPROTOOPT;
      break;
  }
  return error == 0 ? 0 : Fail(error);
}

}

ELIXIR_BIONIC_FN(int, setsockopt)(int fd, int level, int name, const void* value, g_socklen length) noexcept {
  return Guarded(-1, [&] { return SetSockOptImpl(fd, level, name, value, length); });
}
ELIXIR_BIONIC_FN(int, getsockopt)(int fd, int level, int name, void* value, g_socklen* length) noexcept {
  return Guarded(-1, [&] { return GetSockOptImpl(fd, level, name, value, length); });
}

void RegisterSockoptExports(ExportTable& table) {
  table.Add(kLibc, "setsockopt", reinterpret_cast<void*>(&elixir_bionic_setsockopt));
  table.Add(kLibc, "getsockopt", reinterpret_cast<void*>(&elixir_bionic_getsockopt));
}

}
