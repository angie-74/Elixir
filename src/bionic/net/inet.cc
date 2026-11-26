// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// inet_pton/inet_ntop follow bionic's OpenBSD implementation
// (upstream-openbsd/lib/libc/net/inet_pton.c, inet_ntop.c by Paul Vixie, ISC
// licence). inet_aton is bionic's arpa_inet.cpp.
//
//   Copyright (c) 1996,1999 by Internet Software Consortium.
//
//   Permission to use, copy, modify, and distribute this software for any
//   purpose with or without fee is hereby granted, provided that the above
//   copyright notice and this permission notice appear in all copies.
//
//   THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
//   ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
//   OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
//   CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
//   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
//   PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
//   ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
//   SOFTWARE.
#include "bionic/net/net_internal.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace elixir::bionic::net::internal {
namespace {

int HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int Pton4(const char* src, std::uint8_t* dst) {
  std::uint8_t tmp[4] = {};
  int octets = 0;
  bool saw_digit = false;
  std::uint8_t* tp = tmp;
  for (char ch; (ch = *src++) != '\0';) {
    if (ch >= '0' && ch <= '9') {
      const unsigned value = *tp * 10u + static_cast<unsigned>(ch - '0');
      if (value > 255) return 0;
      if (!saw_digit) {
        if (++octets > 4) return 0;
        saw_digit = true;
      }
      *tp = static_cast<std::uint8_t>(value);
    } else if (ch == '.' && saw_digit) {
      if (octets == 4) return 0;
      *++tp = 0;
      saw_digit = false;
    } else {
      return 0;
    }
  }
  if (octets < 4) return 0;
  std::memcpy(dst, tmp, 4);
  return 1;
}

int Pton6(const char* src, std::uint8_t* dst) {
  std::uint8_t tmp[16] = {};
  std::uint8_t* tp = tmp;
  std::uint8_t* const endp = tmp + 16;
  std::uint8_t* colonp = nullptr;
  if (*src == ':' && *++src != ':') return 0;
  const char* curtok = src;
  bool saw_xdigit = false;
  int count_xdigit = 0;
  unsigned value = 0;
  for (char ch; (ch = *src++) != '\0';) {
    const int digit = HexValue(ch);
    if (digit >= 0) {
      if (count_xdigit >= 4) return 0;
      value = (value << 4) | static_cast<unsigned>(digit);
      if (value > 0xffff) return 0;
      saw_xdigit = true;
      ++count_xdigit;
      continue;
    }
    if (ch == ':') {
      curtok = src;
      if (!saw_xdigit) {
        if (colonp != nullptr) return 0;
        colonp = tp;
        continue;
      }
      if (*src == '\0') return 0;
      if (tp + 2 > endp) return 0;
      *tp++ = static_cast<std::uint8_t>(value >> 8);
      *tp++ = static_cast<std::uint8_t>(value);
      saw_xdigit = false;
      count_xdigit = 0;
      value = 0;
      continue;
    }
    if (ch == '.' && tp + 4 <= endp && Pton4(curtok, tp) > 0) {
      tp += 4;
      saw_xdigit = false;
      break;
    }
    return 0;
  }
  if (saw_xdigit) {
    if (tp + 2 > endp) return 0;
    *tp++ = static_cast<std::uint8_t>(value >> 8);
    *tp++ = static_cast<std::uint8_t>(value);
  }
  if (colonp != nullptr) {
    const auto n = static_cast<int>(tp - colonp);
    if (tp == endp) return 0;
    for (int i = 1; i <= n; ++i) {
      endp[-i] = colonp[n - i];
      colonp[n - i] = 0;
    }
    tp = endp;
  }
  if (tp != endp) return 0;
  std::memcpy(dst, tmp, 16);
  return 1;
}

const char* Ntop4(const std::uint8_t* src, char* dst, std::size_t size) {
  char tmp[sizeof("255.255.255.255")];
  const int length = std::snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", src[0], src[1], src[2], src[3]);
  if (length <= 0 || static_cast<std::size_t>(length) >= size) {
    SetGuestErrno(guest::kENOSPC);
    return nullptr;
  }
  std::memcpy(dst, tmp, static_cast<std::size_t>(length) + 1);
  return dst;
}

const char* Ntop6(const std::uint8_t* src, char* dst, std::size_t size) {
  char tmp[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
  unsigned words[8] = {};
  for (int i = 0; i < 16; ++i) words[i / 2] |= static_cast<unsigned>(src[i]) << ((1 - (i % 2)) << 3);
  struct Run {
    int base = -1;
    int len = 0;
  } best, cur;
  for (int i = 0; i < 8; ++i) {
    if (words[i] == 0) {
      if (cur.base == -1) {
        cur.base = i;
        cur.len = 1;
      } else {
        ++cur.len;
      }
    } else if (cur.base != -1) {
      if (best.base == -1 || cur.len > best.len) best = cur;
      cur.base = -1;
    }
  }
  if (cur.base != -1 && (best.base == -1 || cur.len > best.len)) best = cur;
  if (best.base != -1 && best.len < 2) best.base = -1;

  char* tp = tmp;
  for (int i = 0; i < 8; ++i) {
    if (best.base != -1 && i >= best.base && i < best.base + best.len) {
      if (i == best.base) *tp++ = ':';
      continue;
    }
    if (i != 0) *tp++ = ':';
    if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
      if (Ntop4(src + 12, tp, sizeof(tmp) - static_cast<std::size_t>(tp - tmp)) == nullptr) return nullptr;
      tp += std::strlen(tp);
      break;
    }
    tp += std::snprintf(tp, sizeof(tmp) - static_cast<std::size_t>(tp - tmp), "%x", words[i]);
  }
  if (best.base != -1 && best.base + best.len == 8) *tp++ = ':';
  *tp++ = '\0';
  if (static_cast<std::size_t>(tp - tmp) > size) {
    SetGuestErrno(guest::kENOSPC);
    return nullptr;
  }
  std::memcpy(dst, tmp, static_cast<std::size_t>(tp - tmp));
  return dst;
}

}

int InetPton(int family, const char* text, void* out) noexcept {
  if (text == nullptr || out == nullptr) return 0;
  switch (family) {
    case guest::kAF_INET: return Pton4(text, static_cast<std::uint8_t*>(out));
    case guest::kAF_INET6: return Pton6(text, static_cast<std::uint8_t*>(out));
    default:
      SetGuestErrno(guest::kEAFNOSUPPORT);
      return -1;
  }
}

const char* InetNtop(int family, const void* addr, char* out, std::size_t size) noexcept {
  switch (family) {
    case guest::kAF_INET: return Ntop4(static_cast<const std::uint8_t*>(addr), out, size);
    case guest::kAF_INET6: return Ntop6(static_cast<const std::uint8_t*>(addr), out, size);
    default:
      SetGuestErrno(guest::kEAFNOSUPPORT);
      return nullptr;
  }
}

bool InetAton(const char* text, std::uint32_t* out_network_order) noexcept {
  if (text == nullptr) return false;
  unsigned long long parts[4] = {};
  int i = 0;
  const char* cp = text;
  for (; i < 4; ++i) {
    char* end = nullptr;
    errno = 0;
    parts[i] = std::strtoull(cp, &end, 0);
    if (errno != 0 || end == cp || (*end != '.' && *end != '\0')) return false;
    if (*end == '\0') break;
    cp = end + 1;
  }
  std::uint64_t result = 0;
  switch (i) {
    case 0:
      if (parts[0] > 0xffffffffull) return false;
      result = parts[0];
      break;
    case 1:
      if (parts[0] > 0xff || parts[1] > 0xffffff) return false;
      result = (parts[0] << 24) | parts[1];
      break;
    case 2:
      if (parts[0] > 0xff || parts[1] > 0xff || parts[2] > 0xffff) return false;
      result = (parts[0] << 24) | (parts[1] << 16) | parts[2];
      break;
    case 3:
      if (parts[0] > 0xff || parts[1] > 0xff || parts[2] > 0xff || parts[3] > 0xff) return false;
      result = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
      break;
    default:
      return false;
  }
  if (out_network_order != nullptr) *out_network_order = htonl(static_cast<u_long>(result));
  return true;
}

namespace {

std::uint32_t Swap32(std::uint32_t v) { return _byteswap_ulong(v); }
std::uint16_t Swap16(std::uint16_t v) { return _byteswap_ushort(v); }

bool ClassA(std::uint32_t host) { return (host & 0x80000000u) == 0; }
bool ClassB(std::uint32_t host) { return (host & 0xc0000000u) == 0x80000000u; }

}

ELIXIR_BIONIC_FN(int, inet_pton)(int family, const char* text, void* out) noexcept {
  return InetPton(family, text, out);
}
ELIXIR_BIONIC_FN(const char*, inet_ntop)(int family, const void* addr, char* out, g_socklen size) noexcept {
  if (addr == nullptr || out == nullptr) {
    SetGuestErrno(guest::kENOSPC);
    return nullptr;
  }
  return InetNtop(family, addr, out, size);
}
ELIXIR_BIONIC_FN(int, inet_aton)(const char* text, guest::InAddr* out) noexcept {
  const int saved = GuestErrno();
  std::uint32_t value = 0;
  const bool ok = InetAton(text, &value);
  SetGuestErrno(saved);
  if (!ok) return 0;
  if (out != nullptr) std::memcpy(out, &value, sizeof(value));
  return 1;
}
ELIXIR_BIONIC_FN(std::uint32_t, inet_addr)(const char* text) noexcept {
  std::uint32_t value = 0;
  return InetAton(text, &value) ? value : 0xffffffffu;
}
ELIXIR_BIONIC_FN(std::uint32_t, inet_network)(const char* text) noexcept {
  std::uint32_t value = 0;
  return Swap32(InetAton(text, &value) ? value : 0xffffffffu);
}
ELIXIR_BIONIC_FN(char*, inet_ntoa)(guest::InAddr address) noexcept {
  thread_local char buffer[sizeof("255.255.255.255")];
  std::uint8_t bytes[4];
  std::memcpy(bytes, &address, 4);
  std::snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
  return buffer;
}
ELIXIR_BIONIC_FN(guest::InAddr, inet_makeaddr)(std::uint32_t net, std::uint32_t host) noexcept {
  std::uint32_t value;
  if (net < 128) {
    value = (net << 24) | (host & 0x00ffffffu);
  } else if (net < 65536) {
    value = (net << 16) | (host & 0x0000ffffu);
  } else if (net < 16777216u) {
    value = (net << 8) | (host & 0x000000ffu);
  } else {
    value = net | host;
  }
  guest::InAddr out;
  const std::uint32_t network = Swap32(value);
  std::memcpy(&out, &network, sizeof(network));
  return out;
}
ELIXIR_BIONIC_FN(std::uint32_t, inet_lnaof)(guest::InAddr address) noexcept {
  std::uint32_t network;
  std::memcpy(&network, &address, 4);
  const std::uint32_t host = Swap32(network);
  if (ClassA(host)) return host & 0x00ffffffu;
  if (ClassB(host)) return host & 0x0000ffffu;
  return host & 0x000000ffu;
}
ELIXIR_BIONIC_FN(std::uint32_t, inet_netof)(guest::InAddr address) noexcept {
  std::uint32_t network;
  std::memcpy(&network, &address, 4);
  const std::uint32_t host = Swap32(network);
  if (ClassA(host)) return (host & 0xff000000u) >> 24;
  if (ClassB(host)) return (host & 0xffff0000u) >> 16;
  return (host & 0xffffff00u) >> 8;
}
ELIXIR_BIONIC_FN(std::uint32_t, htonl)(std::uint32_t v) noexcept { return Swap32(v); }
ELIXIR_BIONIC_FN(std::uint32_t, ntohl)(std::uint32_t v) noexcept { return Swap32(v); }
ELIXIR_BIONIC_FN(std::uint16_t, htons)(std::uint16_t v) noexcept { return Swap16(v); }
ELIXIR_BIONIC_FN(std::uint16_t, ntohs)(std::uint16_t v) noexcept { return Swap16(v); }

ELIXIR_BIONIC_FN(guest::Cmsghdr*, __cmsg_nxthdr)(guest::Msghdr* msg, guest::Cmsghdr* cmsg) noexcept {
  if (msg == nullptr || cmsg == nullptr) return nullptr;
  const std::uint64_t aligned = (cmsg->cmsg_len + 7u) & ~std::uint64_t{7};
  auto* next = reinterpret_cast<guest::Cmsghdr*>(reinterpret_cast<char*>(cmsg) + aligned);
  const std::uint64_t used =
      static_cast<std::uint64_t>(reinterpret_cast<char*>(next + 1) - static_cast<char*>(msg->msg_control));
  return used > msg->msg_controllen ? nullptr : next;
}

namespace {
const guest::In6Addr g_in6addr_any = {};
const guest::In6Addr g_in6addr_loopback = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
}

void RegisterInetExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("inet_pton", &elixir_bionic_inet_pton);
  add("inet_ntop", &elixir_bionic_inet_ntop);
  add("inet_aton", &elixir_bionic_inet_aton);
  add("inet_addr", &elixir_bionic_inet_addr);
  add("inet_network", &elixir_bionic_inet_network);
  add("inet_ntoa", &elixir_bionic_inet_ntoa);
  add("inet_makeaddr", &elixir_bionic_inet_makeaddr);
  add("inet_lnaof", &elixir_bionic_inet_lnaof);
  add("inet_netof", &elixir_bionic_inet_netof);
  add("htonl", &elixir_bionic_htonl);
  add("ntohl", &elixir_bionic_ntohl);
  add("htons", &elixir_bionic_htons);
  add("ntohs", &elixir_bionic_ntohs);
  add("__cmsg_nxthdr", &elixir_bionic___cmsg_nxthdr);
  table.Add(kLibc, "in6addr_any", const_cast<guest::In6Addr*>(&g_in6addr_any));
  table.Add(kLibc, "in6addr_loopback", const_cast<guest::In6Addr*>(&g_in6addr_loopback));
}

}
