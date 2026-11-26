// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <cstring>

namespace elixir::bionic::net::internal {
namespace {

constexpr int kSupportedHostFlags = MSG_OOB | MSG_PEEK | MSG_DONTROUTE;
constexpr std::size_t kMaxDatagram = 65536;
constexpr DWORD kMaxChunk = 0x7fffffff;

int Fail(int error) {
  SetGuestErrno(error);
  return -1;
}

DWORD HostFlags(int guest_flags) {
  DWORD flags = 0;
  if ((guest_flags & guest::kMSG_OOB) != 0) flags |= MSG_OOB;
  if ((guest_flags & guest::kMSG_PEEK) != 0) flags |= MSG_PEEK;
  if ((guest_flags & guest::kMSG_DONTROUTE) != 0) flags |= MSG_DONTROUTE;
  return flags & kSupportedHostFlags;
}

char* Scratch() {
  thread_local std::unique_ptr<char[]> buffer;
  if (buffer == nullptr) buffer.reset(new (std::nothrow) char[kMaxDatagram]);
  return buffer.get();
}

class BufferList {
 public:
  BufferList() = default;
  BufferList(const BufferList&) = delete;
  BufferList& operator=(const BufferList&) = delete;

  bool Init(const guest::Iovec* iov, std::size_t count) {
    count_ = 0;
    total_ = 0;
    if (count > kInline) {
      heap_.reset(new (std::nothrow) WSABUF[count]);
      if (heap_ == nullptr) return false;
      data_ = heap_.get();
    } else {
      data_ = inline_;
    }
    for (std::size_t i = 0; i < count; ++i) {
      std::size_t length = iov[i].iov_len;
      if (length > kMaxChunk - total_) length = kMaxChunk - total_;
      if (length == 0) continue;
      data_[count_].buf = static_cast<char*>(iov[i].iov_base);
      data_[count_].len = static_cast<ULONG>(length);
      total_ += length;
      ++count_;
    }
    return true;
  }
  void InitSingle(void* buffer, std::size_t length) {
    data_ = inline_;
    count_ = 0;
    total_ = length > kMaxChunk ? kMaxChunk : length;
    if (total_ > 0) {
      inline_[0].buf = static_cast<char*>(buffer);
      inline_[0].len = static_cast<ULONG>(total_);
      count_ = 1;
    }
  }
  void Advance(std::size_t bytes) {
    total_ -= bytes;
    while (bytes > 0 && count_ > 0) {
      if (bytes >= data_[0].len) {
        bytes -= data_[0].len;
        ++data_;
        --count_;
      } else {
        data_[0].buf += bytes;
        data_[0].len -= static_cast<ULONG>(bytes);
        bytes = 0;
      }
    }
  }
  void Scatter(const char* source, std::size_t size) const {
    for (DWORD i = 0; i < count_ && size > 0; ++i) {
      const std::size_t n = size < data_[i].len ? size : data_[i].len;
      std::memcpy(data_[i].buf, source, n);
      source += n;
      size -= n;
    }
  }
  WSABUF* data() { return data_; }
  DWORD count() const { return count_; }
  std::size_t total() const { return total_; }

 private:
  static constexpr std::size_t kInline = 16;
  WSABUF inline_[kInline];
  std::unique_ptr<WSABUF[]> heap_;
  WSABUF* data_ = inline_;
  DWORD count_ = 0;
  std::size_t total_ = 0;
};

struct RecvResult {
  std::size_t bytes = 0;
  bool truncated = false;
  HostAddr from;
  bool have_from = false;
  bool dequeued = false;
};

bool ShouldNotWait(int fd, SocketState* state, bool nonblocking_known, bool nonblocking, int flags) {
  if ((flags & guest::kMSG_DONTWAIT) != 0) return true;
  if (nonblocking_known) return nonblocking;
  return fd >= 0 ? FdIsNonBlocking(fd, state) : (state != nullptr && state->nonblocking.load());
}

bool EffectiveTimeoutUs(const SocketState* state, bool sending, const Deadline* deadline, std::int64_t* timeout_us) {
  *timeout_us = 0;
  if (state != nullptr) *timeout_us = sending ? state->snd_timeout_us.load() : state->rcv_timeout_us.load();
  if (deadline != nullptr && !deadline->infinite()) {
    const std::int64_t left = deadline->RemainingNs() / 1000;
    if (left <= 0) return false;
    if (*timeout_us == 0 || left < *timeout_us) *timeout_us = left;
  }
  return true;
}

int WaitForIo(int fd, SocketState* state, SOCKET s, bool nonblocking_known, bool nonblocking, int flags,
              bool sending, const Deadline* deadline) {
  if (ShouldNotWait(fd, state, nonblocking_known, nonblocking, flags)) return guest::kEAGAIN;
  std::int64_t timeout_us = 0;
  if (!EffectiveTimeoutUs(state, sending, deadline, &timeout_us)) return guest::kEAGAIN;
  return WaitSocket(s, sending ? POLLWRNORM : POLLRDNORM, timeout_us,
                    !sending && state != nullptr ? &state->shut_rd : nullptr);
}

int ReceiveCoreImpl(int fd, SOCKET s, SocketPtr& state, BufferList& buffers, int flags, bool want_from,
                    bool nonblocking_known, bool nonblocking, const Deadline* deadline, RecvResult* result);

int ReceiveCore(int fd, SOCKET s, SocketPtr& state, BufferList& buffers, int flags, bool want_from,
                bool nonblocking_known, bool nonblocking, const Deadline* deadline, RecvResult* result) {
  const int error =
      ReceiveCoreImpl(fd, s, state, buffers, flags, want_from, nonblocking_known, nonblocking, deadline, result);
  if (error == 0 && result->dequeued && (flags & guest::kMSG_PEEK) == 0 && state != nullptr &&
      state->pair != nullptr) {
    state->pair->Consumed(state->pair_end);
  }
  if (error == 0 && (flags & guest::kMSG_PEEK) == 0 && state != nullptr && !state->watchers.Empty() &&
      (result->dequeued || result->bytes > 0)) {
    u_long pending = 0;
    if (ioctlsocket(s, FIONREAD, &pending) == 0 && pending == 0) NoteWouldBlock(state.get(), false);
  }
  return error;
}

int ReceiveCoreImpl(int fd, SOCKET s, SocketPtr& state, BufferList& buffers, int flags, bool want_from,
                    bool nonblocking_known, bool nonblocking, const Deadline* deadline, RecvResult* result) {
  if ((flags & guest::kMSG_ERRQUEUE) != 0) return guest::kEAGAIN;
  Deadline own_deadline = Deadline::Infinite();
  const DWORD host_flags = HostFlags(flags);
  const bool want_trunc = (flags & guest::kMSG_TRUNC) != 0;
  std::size_t received = 0;
  for (;;) {
    if (state == nullptr) state = LookupSocket(s);
    const bool stream = state != nullptr ? state->type == guest::kSOCK_STREAM : false;
    const bool datagram = !stream;
    DWORD got = 0;
    DWORD out_flags = host_flags;
    result->from.length = sizeof(result->from.storage);
    result->from.storage.ss_family = AF_UNSPEC;
    int rc;
    WSABUF scratch_buffer;
    const bool use_scratch = want_trunc && (datagram || received == 0);
    if (use_scratch) {
      scratch_buffer.buf = Scratch();
      if (scratch_buffer.buf == nullptr) return guest::kENOMEM;
      scratch_buffer.len = static_cast<ULONG>(stream && buffers.total() < kMaxDatagram ? buffers.total() : kMaxDatagram);
      if (stream && scratch_buffer.len == 0) return 0;
      rc = WSARecvFrom(s, &scratch_buffer, 1, &got, &out_flags, want_from ? result->from.get() : nullptr,
                       want_from ? &result->from.length : nullptr, nullptr, nullptr);
    } else if (buffers.count() == 0 && datagram) {
      char byte;
      WSABUF one{1, &byte};
      rc = WSARecvFrom(s, &one, 1, &got, &out_flags, want_from ? result->from.get() : nullptr,
                       want_from ? &result->from.length : nullptr, nullptr, nullptr);
      if (rc == 0 || WSAGetLastError() == WSAEMSGSIZE) {
        result->truncated = got > 0 || rc != 0;
        result->bytes = 0;
        result->have_from = want_from;
        result->dequeued = true;
        return 0;
      }
    } else if (buffers.count() == 0) {
      return 0;
    } else {
      rc = WSARecvFrom(s, buffers.data(), buffers.count(), &got, &out_flags,
                       want_from ? result->from.get() : nullptr, want_from ? &result->from.length : nullptr,
                       nullptr, nullptr);
    }
    if (rc == 0) {
      if (use_scratch) {
        if (datagram) {
          const std::size_t room = buffers.total();
          buffers.Scatter(scratch_buffer.buf, got < room ? got : room);
          result->truncated = got > room;
          result->bytes = got;
          result->dequeued = true;
        } else {
          result->bytes = received + got;
        }
        result->have_from = want_from && datagram;
        return 0;
      }
      received += got;
      if (stream && (flags & guest::kMSG_WAITALL) != 0 && (flags & guest::kMSG_PEEK) == 0 && got > 0 &&
          got < buffers.total()) {
        buffers.Advance(got);
        continue;
      }
      result->bytes = received;
      result->have_from = want_from && datagram;
      result->dequeued = datagram;
      return 0;
    }
    const int error = WSAGetLastError();
    if (error == WSAEMSGSIZE) {
      result->bytes = use_scratch ? kMaxDatagram : buffers.total();
      if (use_scratch) buffers.Scatter(scratch_buffer.buf, buffers.total());
      result->truncated = true;
      result->have_from = want_from;
      result->dequeued = true;
      return 0;
    }
    if (error == WSAESHUTDOWN) {
      result->bytes = received;
      return 0;
    }
    const bool unbound_datagram = error == WSAEINVAL && datagram && state != nullptr && !state->bound.load();
    if (error != WSAEWOULDBLOCK && !unbound_datagram) {
      if (received > 0) {
        result->bytes = received;
        return 0;
      }
      return SocketErrno(error, state.get(), false);
    }
    if (state != nullptr) {
      NoteWouldBlock(state.get(), false);
      if (state->shut_rd.load()) {
        result->bytes = received;
        return 0;
      }
      if (deadline == nullptr && state->rcv_timeout_us.load() > 0) {
        const std::int64_t timeout = state->rcv_timeout_us.load();
        own_deadline = Deadline::FromNs(timeout > INT64_MAX / 4000 ? INT64_MAX / 4 : timeout * 1000);
        deadline = &own_deadline;
      }
    }
    const int waited = WaitForIo(fd, state.get(), s, nonblocking_known, nonblocking, flags, false, deadline);
    if (waited != 0) {
      if (received > 0) {
        result->bytes = received;
        return 0;
      }
      return waited;
    }
  }
}

struct SendPlan {
  const HostAddr* to = nullptr;
  std::uint32_t gso = 0;
};

int SendOnce(SOCKET s, WSABUF* buffers, DWORD count, DWORD host_flags, const HostAddr* to, DWORD* sent) {
  char none = 0;
  WSABUF empty{0, &none};
  if (count == 0) {
    buffers = &empty;
    count = 1;
  }
  return WSASendTo(s, buffers, count, sent, host_flags, to != nullptr ? to->get() : nullptr,
                   to != nullptr ? to->length : 0, nullptr, nullptr);
}

int SendCore(int fd, SOCKET s, SocketPtr& state, BufferList& buffers, int flags, const SendPlan& plan,
             bool nonblocking_known, bool nonblocking, std::size_t* sent_out) {
  const DWORD host_flags = HostFlags(flags) & ~static_cast<DWORD>(MSG_PEEK);
  std::size_t sent_total = 0;
  if ((flags & guest::kMSG_FASTOPEN) != 0) {
    if (state == nullptr) state = LookupSocket(s);
    if (state != nullptr && state->type == guest::kSOCK_STREAM && state->phase.load() != SockPhase::kConnected) {
      return guest::kEOPNOTSUPP;
    }
  }
  if (plan.gso != 0 && buffers.total() > plan.gso) {
    if (buffers.total() > static_cast<std::size_t>(plan.gso) * 128) return guest::kEINVAL;
    while (buffers.total() > 0) {
      const std::size_t segment = buffers.total() < plan.gso ? buffers.total() : plan.gso;
      WSABUF slice[16];
      DWORD slice_count = 0;
      std::size_t need = segment;
      for (DWORD i = 0; i < buffers.count() && need > 0 && slice_count < 16; ++i) {
        const std::size_t take = need < buffers.data()[i].len ? need : buffers.data()[i].len;
        slice[slice_count].buf = buffers.data()[i].buf;
        slice[slice_count].len = static_cast<ULONG>(take);
        ++slice_count;
        need -= take;
      }
      if (need > 0) return guest::kEINVAL;
      DWORD sent = 0;
      if (SendOnce(s, slice, slice_count, host_flags, plan.to, &sent) != 0) {
        const int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
          if (state == nullptr) state = LookupSocket(s);
          NoteWouldBlock(state.get(), true);
          if (sent_total > 0) break;
          const int waited = WaitForIo(fd, state.get(), s, nonblocking_known, nonblocking, flags, true, nullptr);
          if (waited != 0) return waited;
          continue;
        }
        if (sent_total > 0) break;
        if (state == nullptr) state = LookupSocket(s);
        return SocketErrno(error, state.get(), true);
      }
      buffers.Advance(segment);
      sent_total += segment;
    }
    *sent_out = sent_total;
    return 0;
  }
  PairChannel* pair = nullptr;
  if (g_pair_sockets.load(std::memory_order_relaxed) != 0) {
    if (state == nullptr) state = LookupSocket(s);
    if (state != nullptr) pair = state->pair.get();
  }
  const int peer_end = state != nullptr ? 1 - state->pair_end : 0;
  Deadline send_deadline = Deadline::Infinite();
  bool send_deadline_set = false;
  for (;;) {
    const PairChannel::Reserve reserved =
        pair != nullptr ? pair->TryReserve(peer_end, buffers.total()) : PairChannel::kReserved;
    if (reserved == PairChannel::kPeerClosed) {
      return state->type == guest::kSOCK_SEQPACKET ? guest::kEPIPE : guest::kECONNREFUSED;
    }
    if (reserved == PairChannel::kFull) {
      NoteWouldBlock(state.get(), true);
      if (ShouldNotWait(fd, state.get(), nonblocking_known, nonblocking, flags)) return guest::kEAGAIN;
      if (!send_deadline_set && state->snd_timeout_us.load() > 0) {
        const std::int64_t timeout = state->snd_timeout_us.load();
        send_deadline = Deadline::FromNs(timeout > INT64_MAX / 4000 ? INT64_MAX / 4 : timeout * 1000);
        send_deadline_set = true;
      }
      if (pair->WaitWritable(peer_end, send_deadline) != 0) return guest::kEAGAIN;
      continue;
    }
    DWORD sent = 0;
    const bool ok = SendOnce(s, buffers.data(), buffers.count(), host_flags, plan.to, &sent) == 0;
    const int send_error = ok ? 0 : WSAGetLastError();
    if (!ok && pair != nullptr && reserved == PairChannel::kReserved) pair->Cancel(peer_end);
    if (ok) {
      sent_total += sent;
      if (sent < buffers.total()) {
        if (state == nullptr) state = LookupSocket(s);
        const bool stream = state != nullptr && state->type == guest::kSOCK_STREAM;
        if (stream) {
          buffers.Advance(sent);
          bool nb = nonblocking;
          if (!nonblocking_known) nb = FdIsNonBlocking(fd, state.get());
          if (!nb && (flags & guest::kMSG_DONTWAIT) == 0) continue;
        }
      }
      *sent_out = sent_total;
      return 0;
    }
    const int error = send_error;
    if (state == nullptr) state = LookupSocket(s);
    if (error != WSAEWOULDBLOCK) {
      if (sent_total > 0) {
        *sent_out = sent_total;
        return 0;
      }
      if (error == WSAEMSGSIZE) return guest::kEMSGSIZE;
      return SocketErrno(error, state.get(), true);
    }
    NoteWouldBlock(state.get(), true);
    if (!send_deadline_set && state != nullptr && state->snd_timeout_us.load() > 0) {
      const std::int64_t timeout = state->snd_timeout_us.load();
      send_deadline = Deadline::FromNs(timeout > INT64_MAX / 4000 ? INT64_MAX / 4 : timeout * 1000);
      send_deadline_set = true;
    }
    const int waited = WaitForIo(fd, state.get(), s, nonblocking_known, nonblocking, flags, true,
                                 send_deadline_set ? &send_deadline : nullptr);
    if (waited != 0) {
      if (sent_total > 0) {
        *sent_out = sent_total;
        return 0;
      }
      return waited;
    }
  }
}

int StoreSource(const SocketState* state, const RecvResult& result, void* address, g_socklen* length) {
  guest::SockaddrStorage value;
  std::uint32_t value_length = 0;
  if (result.have_from && state != nullptr && state->domain != guest::kAF_UNIX) {
    value_length = HostToGuestSockaddr(result.from.get(), result.from.length, &value);
  }
  if (length == nullptr) return guest::kEFAULT;
  if (value_length == 0) {
    *length = 0;
    return 0;
  }
  return CopySockaddrOut(value, value_length, address, length);
}

int ParseControl(const guest::Msghdr& msg, const SocketState* state, std::uint32_t* gso) {
  if (msg.msg_controllen == 0) return 0;
  if (msg.msg_control == nullptr) return guest::kEFAULT;
  const auto* cursor = static_cast<const std::uint8_t*>(msg.msg_control);
  std::size_t remaining = msg.msg_controllen;
  while (remaining >= sizeof(guest::Cmsghdr)) {
    guest::Cmsghdr header;
    std::memcpy(&header, cursor, sizeof(header));
    if (header.cmsg_len < sizeof(guest::Cmsghdr) || header.cmsg_len > remaining) return guest::kEINVAL;
    const std::uint8_t* data = cursor + sizeof(guest::Cmsghdr);
    const std::size_t data_length = header.cmsg_len - sizeof(guest::Cmsghdr);
    if (state != nullptr && state->emulated_unix) {
      if (header.cmsg_level == guest::kSOL_SOCKET && header.cmsg_type == guest::kSCM_RIGHTS) {
        return guest::kEOPNOTSUPP;
      }
    } else if (header.cmsg_level == guest::kSOL_UDP && header.cmsg_type == kUDP_SEGMENT) {
      if (data_length < sizeof(std::uint16_t)) return guest::kEINVAL;
      std::uint16_t size;
      std::memcpy(&size, data, sizeof(size));
      if (state == nullptr || state->type != guest::kSOCK_DGRAM) return guest::kEINVAL;
      *gso = size;
    } else if (header.cmsg_level == guest::kSOL_SOCKET && header.cmsg_type == guest::kSCM_RIGHTS) {
      return guest::kEOPNOTSUPP;
    }
    const std::size_t aligned = (header.cmsg_len + 7) & ~static_cast<std::size_t>(7);
    if (aligned >= remaining) break;
    cursor += aligned;
    remaining -= aligned;
  }
  return 0;
}

g_ssize SendToImpl(int fd, const void* buffer, g_size length, int flags, const void* address, g_socklen address_length) {
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, false)) return -1;
  if (buffer == nullptr && length != 0) return Fail(guest::kEFAULT);
  BufferList buffers;
  buffers.InitSingle(const_cast<void*>(buffer), length);
  HostAddr target;
  SendPlan plan;
  if (address != nullptr) {
    if (ref.state == nullptr) ref.state = LookupSocket(ref.s);
    if (ref.state == nullptr) ResolveSocket(fd, &ref, true);
    if (ref.state == nullptr) return -1;
    if (ref.state->type == guest::kSOCK_DGRAM) {
      const int translated = GuestToHostAddr(*ref.state, address, address_length, AddrUse::kSendTo, &target);
      if (translated != 0) return Fail(translated);
      plan.to = &target;
    }
  }
  if (g_gso_sockets.load(std::memory_order_relaxed) != 0) {
    if (ref.state == nullptr) ref.state = LookupSocket(ref.s);
    if (ref.state != nullptr) plan.gso = ref.state->gso_size.load();
  }
  std::size_t sent = 0;
  const int error = SendCore(fd, ref.s, ref.state, buffers, flags, plan, false, false, &sent);
  if (error != 0) return Fail(error);
  return static_cast<g_ssize>(sent);
}

g_ssize RecvFromImpl(int fd, void* buffer, g_size length, int flags, void* address, g_socklen* address_length,
                     const Deadline* deadline = nullptr) {
  if (address != nullptr && address_length == nullptr) return Fail(guest::kEFAULT);
  if (address != nullptr && static_cast<std::int32_t>(*address_length) < 0) return Fail(guest::kEINVAL);
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, false)) return -1;
  if (buffer == nullptr && length != 0) return Fail(guest::kEFAULT);
  BufferList buffers;
  buffers.InitSingle(buffer, length);
  RecvResult result;
  const int error = ReceiveCore(fd, ref.s, ref.state, buffers, flags, address != nullptr, false, false, deadline,
                                &result);
  if (error != 0) return Fail(error);
  if (address != nullptr) {
    if (ref.state == nullptr) ref.state = LookupSocket(ref.s);
    const int stored = StoreSource(ref.state.get(), result, address, address_length);
    if (stored != 0) return Fail(stored);
  }
  return static_cast<g_ssize>(result.bytes);
}

bool ValidIovec(const guest::Msghdr* msg) {
  return msg->msg_iovlen == 0 || msg->msg_iov != nullptr;
}

g_ssize SendMsgImpl(int fd, const guest::Msghdr* msg, int flags) {
  if (msg == nullptr) return Fail(guest::kEFAULT);
  if (msg->msg_iovlen > static_cast<g_size>(kIOV_MAX)) return Fail(guest::kEMSGSIZE);
  if (!ValidIovec(msg)) return Fail(guest::kEFAULT);
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, true)) return -1;
  BufferList buffers;
  if (!buffers.Init(msg->msg_iov, static_cast<std::size_t>(msg->msg_iovlen))) return Fail(guest::kENOMEM);
  SendPlan plan;
  plan.gso = ref.state->gso_size.load();
  const int control = ParseControl(*msg, ref.state.get(), &plan.gso);
  if (control != 0) return Fail(control);
  HostAddr target;
  if (msg->msg_name != nullptr && static_cast<std::int32_t>(msg->msg_namelen) < 0) return Fail(guest::kEINVAL);
  const g_socklen name_length = msg->msg_namelen > sizeof(guest::SockaddrStorage)
                                    ? static_cast<g_socklen>(sizeof(guest::SockaddrStorage))
                                    : msg->msg_namelen;
  if (msg->msg_name != nullptr && name_length != 0 && ref.state->type == guest::kSOCK_DGRAM) {
    const int translated = GuestToHostAddr(*ref.state, msg->msg_name, name_length, AddrUse::kSendTo, &target);
    if (translated != 0) return Fail(translated);
    plan.to = &target;
  }
  std::size_t sent = 0;
  const int error = SendCore(fd, ref.s, ref.state, buffers, flags, plan, false, false, &sent);
  if (error != 0) return Fail(error);
  return static_cast<g_ssize>(sent);
}

g_ssize RecvMsgImpl(int fd, guest::Msghdr* msg, int flags, const Deadline* deadline = nullptr) {
  if (msg == nullptr) return Fail(guest::kEFAULT);
  if (msg->msg_iovlen > static_cast<g_size>(kIOV_MAX)) return Fail(guest::kEMSGSIZE);
  if (!ValidIovec(msg)) return Fail(guest::kEFAULT);
  SocketRef ref;
  if (!ResolveSocket(fd, &ref, false)) return -1;
  BufferList buffers;
  if (!buffers.Init(msg->msg_iov, static_cast<std::size_t>(msg->msg_iovlen))) return Fail(guest::kENOMEM);
  RecvResult result;
  const bool want_from = msg->msg_name != nullptr;
  const int error = ReceiveCore(fd, ref.s, ref.state, buffers, flags, want_from, false, false, deadline, &result);
  if (error != 0) return Fail(error);
  if (ref.state == nullptr) ref.state = LookupSocket(ref.s);
  if (want_from) {
    g_socklen name_length = msg->msg_namelen;
    const int stored = StoreSource(ref.state.get(), result, msg->msg_name, &name_length);
    if (stored != 0) return Fail(stored);
    msg->msg_namelen = name_length;
  } else {
    msg->msg_namelen = 0;
  }
  msg->msg_controllen = 0;
  msg->msg_flags = result.truncated ? guest::kMSG_TRUNC : 0;
  return static_cast<g_ssize>(result.bytes);
}

int RecvMmsgImpl(int fd, GuestMmsghdr* vector, unsigned int count, int flags, const guest::Timespec* timeout) {
  if (vector == nullptr && count != 0) return Fail(guest::kEFAULT);
  if (count == 0) {
    SocketRef ref;
    return ResolveSocket(fd, &ref, false) ? 0 : -1;
  }
  if (count > static_cast<unsigned int>(kIOV_MAX)) count = static_cast<unsigned int>(kIOV_MAX);
  Deadline deadline = Deadline::Infinite();
  if (timeout != nullptr) {
    if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000) return Fail(guest::kEINVAL);
    deadline = Deadline::FromNs(timeout->tv_sec > INT64_MAX / 2000000000 ? INT64_MAX / 4
                                                                          : timeout->tv_sec * 1000000000 + timeout->tv_nsec);
  }
  unsigned int done = 0;
  int first_error = 0;
  for (; done < count; ++done) {
    int call_flags = flags & ~guest::kMSG_WAITFORONE;
    if (done > 0 && (flags & guest::kMSG_WAITFORONE) != 0) call_flags |= guest::kMSG_DONTWAIT;
    if (done > 0 && deadline.Expired()) break;
    const g_ssize got = RecvMsgImpl(fd, &vector[done].msg_hdr, call_flags, timeout != nullptr ? &deadline : nullptr);
    if (got < 0) {
      first_error = GuestErrno();
      break;
    }
    vector[done].msg_len = static_cast<std::uint32_t>(got);
  }
  if (done == 0) return Fail(first_error != 0 ? first_error : guest::kEAGAIN);
  return static_cast<int>(done);
}

int SendMmsgImpl(int fd, GuestMmsghdr* vector, unsigned int count, int flags) {
  if (vector == nullptr && count != 0) return Fail(guest::kEFAULT);
  if (count > static_cast<unsigned int>(kIOV_MAX)) count = static_cast<unsigned int>(kIOV_MAX);
  unsigned int done = 0;
  int first_error = 0;
  for (; done < count; ++done) {
    const g_ssize sent = SendMsgImpl(fd, &vector[done].msg_hdr, flags);
    if (sent < 0) {
      first_error = GuestErrno();
      break;
    }
    vector[done].msg_len = static_cast<std::uint32_t>(sent);
  }
  if (done == 0 && count != 0) return Fail(first_error);
  return static_cast<int>(done);
}

}

ELIXIR_BIONIC_FN(g_ssize, send)(int fd, const void* buffer, g_size length, int flags) noexcept {
  return Guarded<g_ssize>(-1, [&] { return SendToImpl(fd, buffer, length, flags, nullptr, 0); });
}
ELIXIR_BIONIC_FN(g_ssize, sendto)(int fd, const void* buffer, g_size length, int flags, const void* address,
                                  g_socklen address_length) noexcept {
  return Guarded<g_ssize>(-1, [&] { return SendToImpl(fd, buffer, length, flags, address, address_length); });
}
ELIXIR_BIONIC_FN(g_ssize, sendmsg)(int fd, const guest::Msghdr* msg, int flags) noexcept {
  return Guarded<g_ssize>(-1, [&] { return SendMsgImpl(fd, msg, flags); });
}
ELIXIR_BIONIC_FN(int, sendmmsg)(int fd, GuestMmsghdr* vector, unsigned int count, int flags) noexcept {
  return Guarded(-1, [&] { return SendMmsgImpl(fd, vector, count, flags); });
}
ELIXIR_BIONIC_FN(g_ssize, recv)(int fd, void* buffer, g_size length, int flags) noexcept {
  return Guarded<g_ssize>(-1, [&] { return RecvFromImpl(fd, buffer, length, flags, nullptr, nullptr); });
}
ELIXIR_BIONIC_FN(g_ssize, recvfrom)(int fd, void* buffer, g_size length, int flags, void* address,
                                    g_socklen* address_length) noexcept {
  return Guarded<g_ssize>(-1, [&] { return RecvFromImpl(fd, buffer, length, flags, address, address_length); });
}
ELIXIR_BIONIC_FN(g_ssize, recvmsg)(int fd, guest::Msghdr* msg, int flags) noexcept {
  return Guarded<g_ssize>(-1, [&] { return RecvMsgImpl(fd, msg, flags); });
}
ELIXIR_BIONIC_FN(int, recvmmsg)(int fd, GuestMmsghdr* vector, unsigned int count, int flags,
                                const guest::Timespec* timeout) noexcept {
  return Guarded(-1, [&] { return RecvMmsgImpl(fd, vector, count, flags, timeout); });
}
ELIXIR_BIONIC_FN(g_ssize, __recvfrom_chk)(int fd, void* buffer, g_size length, g_size buffer_size, int flags,
                                          void* address, g_socklen* address_length) noexcept {
  if (length > buffer_size) FortifyFatal("recvfrom", "prevented write past end of buffer");
  return elixir_bionic_recvfrom(fd, buffer, length, flags, address, address_length);
}
ELIXIR_BIONIC_FN(g_ssize, __sendto_chk)(int fd, const void* buffer, g_size length, g_size buffer_size, int flags,
                                        const void* address, g_socklen address_length) noexcept {
  if (length > buffer_size) FortifyFatal("sendto", "prevented read past end of buffer");
  return elixir_bionic_sendto(fd, buffer, length, flags, address, address_length);
}

void RegisterIoExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("send", &elixir_bionic_send);
  add("sendto", &elixir_bionic_sendto);
  add("sendmsg", &elixir_bionic_sendmsg);
  add("sendmmsg", &elixir_bionic_sendmmsg);
  add("recv", &elixir_bionic_recv);
  add("recvfrom", &elixir_bionic_recvfrom);
  add("recvmsg", &elixir_bionic_recvmsg);
  add("recvmmsg", &elixir_bionic_recvmmsg);
  add("__recvfrom_chk", &elixir_bionic___recvfrom_chk);
  add("__sendto_chk", &elixir_bionic___sendto_chk);
}

std::int64_t NextDatagramSize(SOCKET s) noexcept {
  char* scratch = Scratch();
  if (scratch == nullptr) return -1;
  const int got = recv(s, scratch, static_cast<int>(kMaxDatagram), MSG_PEEK);
  if (got >= 0) return got;
  const int error = WSAGetLastError();
  if (error == WSAEWOULDBLOCK || error == WSAEINVAL) return 0;
  if (error == WSAEMSGSIZE) return static_cast<std::int64_t>(kMaxDatagram);
  return -1;
}

}

namespace elixir::bionic::net {

abi::g_ssize FdRead(void* handle, std::uint8_t kind, void* buffer, std::size_t size, bool nonblocking,
                    int* guest_errno) noexcept {
  using namespace internal;
  try {
    if (static_cast<vfs::FdKind>(kind) == vfs::FdKind::kSocket) {
      const auto s = reinterpret_cast<SOCKET>(handle);
      SocketPtr state = LookupSocket(s);
      BufferList buffers;
      buffers.InitSingle(buffer, size);
      RecvResult result;
      const int error = ReceiveCore(-1, s, state, buffers, 0, false, true, nonblocking, nullptr, &result);
      if (error != 0) {
        *guest_errno = error;
        return -1;
      }
      return static_cast<abi::g_ssize>(result.bytes);
    }
    if (static_cast<vfs::FdKind>(kind) == vfs::FdKind::kEvent) {
      ObjectPtr object = LookupObject(handle);
      if (object == nullptr) {
        *guest_errno = guest::kEBADF;
        return -1;
      }
      return object->Read(buffer, size, nonblocking, guest_errno);
    }
    *guest_errno = guest::kEBADF;
    return -1;
  } catch (...) {
    *guest_errno = guest::kENOMEM;
    return -1;
  }
}

abi::g_ssize FdWrite(void* handle, std::uint8_t kind, const void* buffer, std::size_t size, bool nonblocking,
                     int* guest_errno) noexcept {
  using namespace internal;
  try {
    if (static_cast<vfs::FdKind>(kind) == vfs::FdKind::kSocket) {
      const auto s = reinterpret_cast<SOCKET>(handle);
      SocketPtr state = LookupSocket(s);
      BufferList buffers;
      buffers.InitSingle(const_cast<void*>(buffer), size);
      SendPlan plan;
      if (state != nullptr) plan.gso = state->gso_size.load();
      std::size_t sent = 0;
      const int error = SendCore(-1, s, state, buffers, 0, plan, true, nonblocking, &sent);
      if (error != 0) {
        *guest_errno = error;
        return -1;
      }
      return static_cast<abi::g_ssize>(sent);
    }
    if (static_cast<vfs::FdKind>(kind) == vfs::FdKind::kEvent) {
      ObjectPtr object = LookupObject(handle);
      if (object == nullptr) {
        *guest_errno = guest::kEBADF;
        return -1;
      }
      return object->Write(buffer, size, nonblocking, guest_errno);
    }
    *guest_errno = guest::kEBADF;
    return -1;
  } catch (...) {
    *guest_errno = guest::kENOMEM;
    return -1;
  }
}

int FdBytesAvailable(void* handle, std::uint8_t kind, int* available, int* guest_errno) noexcept {
  using namespace internal;
  if (static_cast<vfs::FdKind>(kind) != vfs::FdKind::kSocket) {
    *guest_errno = guest::kENOTTY;
    return -1;
  }
  const auto s = reinterpret_cast<SOCKET>(handle);
  SocketPtr state = LookupSocket(s);
  std::int64_t bytes = 0;
  if (state != nullptr && state->type != guest::kSOCK_STREAM) {
    bytes = NextDatagramSize(s);
  } else {
    if (state != nullptr && state->phase.load() == SockPhase::kListening) {
      *guest_errno = guest::kEINVAL;
      return -1;
    }
    u_long pending = 0;
    if (ioctlsocket(s, FIONREAD, &pending) != 0) {
      *guest_errno = SocketErrno(WSAGetLastError(), state.get(), false);
      return -1;
    }
    bytes = static_cast<std::int64_t>(pending);
  }
  if (bytes < 0) {
    *guest_errno = guest::kEINVAL;
    return -1;
  }
  *available = bytes > 0x7fffffff ? 0x7fffffff : static_cast<int>(bytes);
  return 0;
}

}
