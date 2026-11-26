// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The structure of the explore loop is adapted from bionic's
// libc/dns/net/getaddrinfo.c:
//
//   Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//   3. Neither the name of the project nor the names of its contributors
//      may be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//   ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
//   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
//   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
//
// The gai_strerror/hstrerror strings are bionic's (getaddrinfo.c, NetBSD
// herror.c; BSD licence, see references/platform_bionic/libc/NOTICE).
#include "bionic/net/net_internal.h"

#include <array>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "bionic/memory.h"

namespace elixir::bionic::net::internal {
namespace {

constexpr int kAny = 0;
constexpr int kAiMask = guest::kAI_PASSIVE | guest::kAI_CANONNAME | guest::kAI_NUMERICHOST |
                        guest::kAI_NUMERICSERV | guest::kAI_ADDRCONFIG;

thread_local int t_h_errno = 0;

struct Explore {
  int family;
  int socktype;
  int protocol;
  int wild;
};

constexpr Explore kExplore[] = {
    {guest::kAF_INET6, guest::kSOCK_DGRAM, guest::kIPPROTO_UDP, 0x07},
    {guest::kAF_INET6, guest::kSOCK_STREAM, guest::kIPPROTO_TCP, 0x07},
    {guest::kAF_INET6, guest::kSOCK_RAW, kAny, 0x05},
    {guest::kAF_INET, guest::kSOCK_DGRAM, guest::kIPPROTO_UDP, 0x07},
    {guest::kAF_INET, guest::kSOCK_STREAM, guest::kIPPROTO_TCP, 0x07},
    {guest::kAF_INET, guest::kSOCK_RAW, kAny, 0x05},
    {guest::kAF_UNSPEC, guest::kSOCK_DGRAM, guest::kIPPROTO_UDP, 0x07},
    {guest::kAF_UNSPEC, guest::kSOCK_STREAM, guest::kIPPROTO_TCP, 0x07},
    {guest::kAF_UNSPEC, guest::kSOCK_RAW, kAny, 0x05},
};

bool MatchFamily(int x, int y, bool wild) {
  return x == y || (wild && (x == guest::kAF_UNSPEC || y == guest::kAF_UNSPEC));
}
bool Match(int x, int y, bool wild) { return x == y || (wild && (x == kAny || y == kAny)); }

struct Hints {
  int flags = 0;
  int family = guest::kAF_UNSPEC;
  int socktype = kAny;
  int protocol = kAny;
};

using Address = std::array<std::uint8_t, 16>;

struct ResolvedAddress {
  int family;
  Address bytes;
};

struct ResolvedName {
  std::vector<ResolvedAddress> addresses;
  std::string canonical;
};

bool EqualsIgnoreCase(const char* a, const char* b) {
  while (*a != '\0' && *b != '\0') {
    if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b))) return false;
    ++a;
    ++b;
  }
  return *a == *b;
}

std::string Undotted(const char* name) {
  std::string text(name);
  if (!text.empty() && text.back() == '.') text.pop_back();
  return text;
}

bool IsInvalidTld(const std::string& name) {
  static const char kSuffix[] = ".invalid";
  if (EqualsIgnoreCase(name.c_str(), "invalid")) return true;
  if (name.size() < sizeof(kSuffix) - 1) return false;
  return EqualsIgnoreCase(name.c_str() + name.size() - (sizeof(kSuffix) - 1), kSuffix);
}

int EaiFromWinsock(int error) {
  switch (error) {
    case WSAHOST_NOT_FOUND:
    case WSANO_DATA:
    case WSAEAFNOSUPPORT:
      return guest::kEAI_NODATA;
    case WSATRY_AGAIN:
      return guest::kEAI_AGAIN;
    case WSA_NOT_ENOUGH_MEMORY:
      return guest::kEAI_MEMORY;
    default:
      return guest::kEAI_FAIL;
  }
}

int ResolveName(const char* host, int family, bool addrconfig, bool want_canonical, ResolvedName* out) {
  const std::string name = Undotted(host);
  if (EqualsIgnoreCase(name.c_str(), "localhost")) {
    ResolvedAddress address{};
    if (family == guest::kAF_INET6) {
      address.family = guest::kAF_INET6;
      address.bytes[15] = 1;
    } else {
      address.family = guest::kAF_INET;
      address.bytes = {127, 0, 0, 1};
    }
    out->addresses.push_back(address);
    out->canonical = "localhost";
    return 0;
  }
  if (EqualsIgnoreCase(name.c_str(), "ip6-localhost") || EqualsIgnoreCase(name.c_str(), "ip6-loopback")) {
    if (family == guest::kAF_INET) return guest::kEAI_NODATA;
    ResolvedAddress address{};
    address.family = guest::kAF_INET6;
    address.bytes[15] = 1;
    out->addresses.push_back(address);
    out->canonical = "ip6-localhost";
    return 0;
  }
  if (IsInvalidTld(name)) return guest::kEAI_NODATA;
  if (!EnsureWinsock()) return guest::kEAI_FAIL;

  const std::wstring wide = Utf8ToWide(host);
  if (wide.empty()) return guest::kEAI_NONAME;
  ADDRINFOW hints{};
  hints.ai_family = family == guest::kAF_INET ? AF_INET : family == guest::kAF_INET6 ? AF_INET6 : AF_UNSPEC;
  hints.ai_flags = (addrconfig ? AI_ADDRCONFIG : 0) | (want_canonical ? AI_CANONNAME : 0);
  PADDRINFOW result = nullptr;
  const int rc = GetAddrInfoW(wide.c_str(), nullptr, &hints, &result);
  if (rc != 0) return EaiFromWinsock(rc);
  for (PADDRINFOW it = result; it != nullptr; it = it->ai_next) {
    ResolvedAddress address{};
    if (it->ai_family == AF_INET && it->ai_addrlen >= sizeof(sockaddr_in)) {
      address.family = guest::kAF_INET;
      std::memcpy(address.bytes.data(), &reinterpret_cast<const sockaddr_in*>(it->ai_addr)->sin_addr, 4);
    } else if (it->ai_family == AF_INET6 && it->ai_addrlen >= sizeof(sockaddr_in6)) {
      address.family = guest::kAF_INET6;
      std::memcpy(address.bytes.data(), &reinterpret_cast<const sockaddr_in6*>(it->ai_addr)->sin6_addr, 16);
    } else {
      continue;
    }
    bool duplicate = false;
    for (const auto& known : out->addresses) {
      if (known.family == address.family && known.bytes == address.bytes) duplicate = true;
    }
    if (!duplicate) out->addresses.push_back(address);
    if (out->canonical.empty() && it->ai_canonname != nullptr) out->canonical = WideToUtf8(it->ai_canonname);
  }
  FreeAddrInfoW(result);
  return out->addresses.empty() ? guest::kEAI_NODATA : 0;
}

int ReverseName(int family, const std::uint8_t* address, std::string* name) {
  static const std::uint8_t kLoopback6[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  if (family == guest::kAF_INET && address[0] == 127 && address[1] == 0 && address[2] == 0 && address[3] == 1) {
    *name = "localhost";
    return 0;
  }
  if (family == guest::kAF_INET6 && std::memcmp(address, kLoopback6, 16) == 0) {
    *name = "ip6-localhost";
    return 0;
  }
  if (!EnsureWinsock()) return kNO_RECOVERY;
  SOCKADDR_STORAGE storage{};
  int length = 0;
  if (family == guest::kAF_INET) {
    auto* in = reinterpret_cast<sockaddr_in*>(&storage);
    in->sin_family = AF_INET;
    std::memcpy(&in->sin_addr, address, 4);
    length = sizeof(sockaddr_in);
  } else {
    auto* in6 = reinterpret_cast<sockaddr_in6*>(&storage);
    in6->sin6_family = AF_INET6;
    std::memcpy(&in6->sin6_addr, address, 16);
    length = sizeof(sockaddr_in6);
  }
  wchar_t host[NI_MAXHOST];
  const int rc = GetNameInfoW(reinterpret_cast<const sockaddr*>(&storage), length, host, NI_MAXHOST, nullptr, 0,
                              NI_NAMEREQD);
  if (rc != 0) {
    switch (rc) {
      case WSATRY_AGAIN: return kTRY_AGAIN;
      case WSAHOST_NOT_FOUND:
      case WSANO_DATA: return kHOST_NOT_FOUND;
      default: return kNO_RECOVERY;
    }
  }
  *name = WideToUtf8(host);
  return name->empty() ? kHOST_NOT_FOUND : 0;
}

std::uint32_t SockaddrLength(int family) {
  return family == guest::kAF_INET6 ? sizeof(guest::SockaddrIn6) : sizeof(guest::SockaddrIn);
}

int AddressLength(int family) { return family == guest::kAF_INET6 ? 16 : 4; }

void FreeChain(guest::Addrinfo* node) {
  while (node != nullptr) {
    guest::Addrinfo* next = node->ai_next;
    if (node->ai_canonname != nullptr) GuestFree(node->ai_canonname);
    GuestFree(node);
    node = next;
  }
}

class Chain {
 public:
  ~Chain() { FreeChain(head_); }
  guest::Addrinfo* Release() {
    guest::Addrinfo* head = head_;
    head_ = nullptr;
    tail_ = nullptr;
    return head;
  }
  bool empty() const { return head_ == nullptr; }

  guest::Addrinfo* Append(const Hints& pai, int family, const std::uint8_t* address, std::uint16_t port,
                          std::uint32_t scope) {
    const std::uint32_t socklen = SockaddrLength(family);
    void* block = GuestCalloc(1, sizeof(guest::Addrinfo) + socklen);
    if (block == nullptr) return nullptr;
    auto* node = static_cast<guest::Addrinfo*>(block);
    node->ai_flags = pai.flags;
    node->ai_family = family;
    node->ai_socktype = pai.socktype;
    node->ai_protocol = pai.protocol;
    node->ai_addrlen = socklen;
    node->ai_addr = reinterpret_cast<guest::Sockaddr*>(node + 1);
    if (family == guest::kAF_INET6) {
      guest::SockaddrIn6 in6{};
      in6.sin6_family = static_cast<std::uint16_t>(guest::kAF_INET6);
      in6.sin6_port = port;
      std::memcpy(in6.sin6_addr.s6_addr, address, 16);
      in6.sin6_scope_id = scope;
      std::memcpy(node->ai_addr, &in6, sizeof(in6));
    } else {
      guest::SockaddrIn in{};
      in.sin_family = static_cast<std::uint16_t>(guest::kAF_INET);
      in.sin_port = port;
      std::memcpy(&in.sin_addr, address, 4);
      std::memcpy(node->ai_addr, &in, sizeof(in));
    }
    if (tail_ == nullptr) {
      head_ = node;
    } else {
      tail_->ai_next = node;
    }
    tail_ = node;
    return node;
  }

 private:
  guest::Addrinfo* head_ = nullptr;
  guest::Addrinfo* tail_ = nullptr;
};

int GetPort(const Hints& ai, const char* servname, bool match_only, std::uint16_t* port) {
  *port = 0;
  if (servname == nullptr) return 0;
  if (ai.family != guest::kAF_INET && ai.family != guest::kAF_INET6) return 0;
  switch (ai.socktype) {
    case guest::kSOCK_RAW:
      return guest::kEAI_SERVICE;
    case guest::kSOCK_DGRAM:
    case guest::kSOCK_STREAM:
    case kAny:
      break;
    default:
      return guest::kEAI_SOCKTYPE;
  }
  long long number = -1;
  if (*servname != '\0') {
    char* end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(servname, &end, 10);
    if (end != servname && *end == '\0' && errno == 0) number = value > 0x7fffffff ? 0x7fffffff : static_cast<long long>(value);
  }
  if (number >= 0) {
    if (number > 65535) return guest::kEAI_SERVICE;
    *port = htons(static_cast<u_short>(number));
    return 0;
  }
  if ((ai.flags & guest::kAI_NUMERICSERV) != 0) return guest::kEAI_NONAME;
  const char* proto = ai.socktype == guest::kSOCK_DGRAM ? "udp" : ai.socktype == guest::kSOCK_STREAM ? "tcp" : nullptr;
  int found = 0;
  if (!LookupServiceByName(servname, proto, &found, nullptr)) return guest::kEAI_SERVICE;
  (void)match_only;
  *port = static_cast<std::uint16_t>(found);
  return 0;
}

int SetCanonical(guest::Addrinfo* node, const Hints& pai, const char* name) {
  if ((pai.flags & guest::kAI_CANONNAME) == 0 || node == nullptr) return 0;
  node->ai_canonname = GuestStrdup(name);
  return node->ai_canonname == nullptr ? guest::kEAI_MEMORY : 0;
}

int ExploreNull(const Hints& pai, const char* servname, Chain* chain) {
  std::uint16_t port = 0;
  if (GetPort(pai, servname, true, &port) != 0) return 0;
  Address address{};
  if ((pai.flags & guest::kAI_PASSIVE) == 0) {
    if (pai.family == guest::kAF_INET6) {
      address[15] = 1;
    } else {
      address = {127, 0, 0, 1};
    }
  }
  if (chain->Append(pai, pai.family, address.data(), port, 0) == nullptr) return guest::kEAI_MEMORY;
  return 0;
}

int ExploreNumeric(const Hints& pai, const char* hostname, const char* servname, Chain* chain) {
  std::uint16_t port = 0;
  if (GetPort(pai, servname, true, &port) != 0) return 0;
  Address address{};
  std::uint32_t scope = 0;
  bool parsed = false;
  if (pai.family == guest::kAF_INET6) {
    const char* percent = std::strchr(hostname, '%');
    if (percent != nullptr) {
      const std::string text(hostname, static_cast<std::size_t>(percent - hostname));
      if (InetPton(guest::kAF_INET6, text.c_str(), address.data()) != 1) return 0;
      const char* zone = percent + 1;
      char* end = nullptr;
      const unsigned long long number = std::strtoull(zone, &end, 10);
      if (*zone != '\0' && end != nullptr && *end == '\0') {
        scope = static_cast<std::uint32_t>(number);
      } else {
        scope = InterfaceIndexFromName(zone);
        if (scope == 0) return guest::kEAI_NODATA;
      }
      parsed = true;
    } else {
      parsed = InetPton(guest::kAF_INET6, hostname, address.data()) == 1;
    }
  } else {
    parsed = InetPton(guest::kAF_INET, hostname, address.data()) == 1;
  }
  if (!parsed) return 0;
  guest::Addrinfo* node = chain->Append(pai, pai.family, address.data(), port, scope);
  if (node == nullptr) return guest::kEAI_MEMORY;
  return SetCanonical(node, pai, hostname);
}

int GetAddrInfoImpl(const char* hostname, const char* servname, const guest::Addrinfo* hints,
                    guest::Addrinfo** result) {
  if (result == nullptr) return guest::kEAI_FAIL;
  *result = nullptr;
  if (hostname == nullptr && servname == nullptr) return guest::kEAI_NONAME;
  Hints ai0;
  if (hints != nullptr) {
    if (hints->ai_addrlen != 0 || hints->ai_canonname != nullptr || hints->ai_addr != nullptr ||
        hints->ai_next != nullptr) {
      return guest::kEAI_BADHINTS;
    }
    if ((hints->ai_flags & ~kAiMask) != 0) return guest::kEAI_BADFLAGS;
    if (hints->ai_family != guest::kAF_UNSPEC && hints->ai_family != guest::kAF_INET &&
        hints->ai_family != guest::kAF_INET6) {
      return guest::kEAI_FAMILY;
    }
    ai0.flags = hints->ai_flags;
    ai0.family = hints->ai_family;
    ai0.socktype = hints->ai_socktype;
    ai0.protocol = hints->ai_protocol;
    if (ai0.socktype != kAny && ai0.protocol != kAny) {
      for (const Explore& ex : kExplore) {
        if (ai0.family != ex.family || ex.socktype == kAny || ex.protocol == kAny) continue;
        if (ai0.socktype == ex.socktype && ai0.protocol != ex.protocol) return guest::kEAI_BADHINTS;
      }
    }
  }
  if (MatchFamily(ai0.family, guest::kAF_INET, true) || MatchFamily(ai0.family, guest::kAF_INET6, true)) {
    Hints probe = ai0;
    if (probe.family == guest::kAF_UNSPEC) probe.family = guest::kAF_INET6;
    std::uint16_t port = 0;
    const int error = GetPort(probe, servname, true, &port);
    if (error != 0) return error;
  }

  Chain chain;
  for (const Explore& ex : kExplore) {
    if (ex.family == guest::kAF_UNSPEC) continue;
    Hints pai = ai0;
    if (!MatchFamily(pai.family, ex.family, (ex.wild & 1) != 0)) continue;
    if (!Match(pai.socktype, ex.socktype, (ex.wild & 2) != 0)) continue;
    if (!Match(pai.protocol, ex.protocol, (ex.wild & 4) != 0)) continue;
    if (pai.family == guest::kAF_UNSPEC) pai.family = ex.family;
    if (pai.socktype == kAny && ex.socktype != kAny) pai.socktype = ex.socktype;
    if (pai.protocol == kAny && ex.protocol != kAny) pai.protocol = ex.protocol;
    const int error = hostname == nullptr ? ExploreNull(pai, servname, &chain)
                                          : ExploreNumeric(pai, hostname, servname, &chain);
    if (error != 0) return error;
  }
  if (!chain.empty()) {
    *result = chain.Release();
    return 0;
  }
  if (hostname == nullptr) return guest::kEAI_NODATA;
  if ((ai0.flags & guest::kAI_NUMERICHOST) != 0) return guest::kEAI_NONAME;
  if (*hostname == '\0') return guest::kEAI_NONAME;

  ResolvedName resolved;
  bool looked_up = false;
  int lookup_error = 0;
  for (const Explore& ex : kExplore) {
    Hints pai = ai0;
    if (pai.family != ex.family) continue;
    if (!Match(pai.socktype, ex.socktype, (ex.wild & 2) != 0)) continue;
    if (!Match(pai.protocol, ex.protocol, (ex.wild & 4) != 0)) continue;
    if (pai.socktype == kAny && ex.socktype != kAny) pai.socktype = ex.socktype;
    if (pai.protocol == kAny && ex.protocol != kAny) pai.protocol = ex.protocol;
    std::uint16_t port = 0;
    if (GetPort(pai, servname, true, &port) != 0) continue;
    if (!looked_up) {
      lookup_error = ResolveName(hostname, ai0.family, (ai0.flags & guest::kAI_ADDRCONFIG) != 0,
                                 (ai0.flags & guest::kAI_CANONNAME) != 0, &resolved);
      looked_up = true;
    }
    if (lookup_error != 0) break;
    bool first = true;
    for (const ResolvedAddress& address : resolved.addresses) {
      if (pai.family != guest::kAF_UNSPEC && address.family != pai.family) continue;
      Hints node_hints = pai;
      node_hints.family = address.family;
      GetPort(node_hints, servname, false, &port);
      guest::Addrinfo* node = chain.Append(node_hints, address.family, address.bytes.data(), port, 0);
      if (node == nullptr) return guest::kEAI_MEMORY;
      if (first) {
        const int canon = SetCanonical(node, pai, resolved.canonical.empty() ? hostname : resolved.canonical.c_str());
        if (canon != 0) return canon;
        first = false;
      }
    }
  }
  if (!chain.empty()) {
    *result = chain.Release();
    return 0;
  }
  if (lookup_error != 0) return lookup_error;
  return guest::kEAI_FAIL;
}

int CopyOut(const std::string& text, char* out, std::size_t size, int overflow_error) {
  if (text.size() + 1 > size) return overflow_error;
  std::memcpy(out, text.c_str(), text.size() + 1);
  return 0;
}

int NumericHost(int family, const std::uint8_t* address, std::uint32_t scope, char* host, std::size_t host_length) {
  char text[INET6_ADDRSTRLEN + 1];
  if (InetNtop(family, address, text, sizeof(text)) == nullptr) return guest::kEAI_SYSTEM;
  std::string value = text;
  if (family == guest::kAF_INET6 && scope != 0) {
    std::string zone;
    const bool link_local = address[0] == 0xfe && (address[1] & 0xc0) == 0x80;
    const bool mc_link_local = address[0] == 0xff && (address[1] & 0x0f) == 0x02;
    if (!((link_local || mc_link_local) && InterfaceNameFromIndex(scope, &zone))) zone = std::to_string(scope);
    value += "%" + zone;
  }
  return CopyOut(value, host, host_length, family == guest::kAF_INET6 ? guest::kEAI_OVERFLOW : guest::kEAI_MEMORY);
}

int GetNameInfoImpl(const void* address, g_socklen address_length, char* host, std::size_t host_length, char* serv,
                    std::size_t serv_length, int flags) {
  if (address == nullptr) return guest::kEAI_FAIL;
  std::uint16_t family;
  std::memcpy(&family, address, sizeof(family));
  if (family == guest::kAF_UNIX) {
    if (address_length < offsetof(guest::SockaddrUn, sun_path)) return guest::kEAI_FAMILY;
    if (serv != nullptr && serv_length > 0) serv[0] = '\0';
    if (host != nullptr && host_length > 0) {
      const char* path = static_cast<const char*>(address) + offsetof(guest::SockaddrUn, sun_path);
      const std::size_t room = host_length < sizeof(guest::SockaddrUn::sun_path) + 1 ? host_length
                                                                                       : sizeof(guest::SockaddrUn::sun_path) + 1;
      std::size_t n = 0;
      while (n + 1 < room && path[n] != '\0') ++n;
      std::memcpy(host, path, n);
      host[n] = '\0';
    }
    return 0;
  }
  if (family != guest::kAF_INET && family != guest::kAF_INET6) return guest::kEAI_FAMILY;
  if (address_length < SockaddrLength(family)) return guest::kEAI_FAMILY;

  std::uint16_t port = 0;
  std::uint8_t bytes[16];
  std::uint32_t scope = 0;
  if (family == guest::kAF_INET) {
    guest::SockaddrIn in;
    std::memcpy(&in, address, sizeof(in));
    port = in.sin_port;
    std::memcpy(bytes, &in.sin_addr, 4);
  } else {
    guest::SockaddrIn6 in6;
    std::memcpy(&in6, address, sizeof(in6));
    port = in6.sin6_port;
    std::memcpy(bytes, in6.sin6_addr.s6_addr, 16);
    scope = in6.sin6_scope_id;
  }

  if (serv != nullptr && serv_length > 0) {
    std::string name;
    if ((flags & guest::kNI_NUMERICSERV) == 0 &&
        LookupServiceByPort(port, (flags & guest::kNI_DGRAM) != 0 ? "udp" : "tcp", &name)) {
      if (name.size() + 1 > serv_length) return guest::kEAI_MEMORY;
      std::memcpy(serv, name.c_str(), name.size() + 1);
    } else {
      const std::string number = std::to_string(ntohs(port));
      if (number.size() + 1 > serv_length) return guest::kEAI_MEMORY;
      std::memcpy(serv, number.c_str(), number.size() + 1);
    }
  }

  if (family == guest::kAF_INET) {
    const std::uint32_t v4 = (static_cast<std::uint32_t>(bytes[0]) << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    if ((v4 & 0xf0000000u) == 0xe0000000u || (v4 & 0xf0000000u) == 0xf0000000u) flags |= guest::kNI_NUMERICHOST;
    if ((v4 >> 24) == 0) flags |= guest::kNI_NUMERICHOST;
  } else {
    static const std::uint8_t kMapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    static const std::uint8_t kLoopback[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    static const std::uint8_t kNat64[12] = {0x00, 0x64, 0xff, 0x9b, 0, 0, 0, 0, 0, 0, 0, 0};
    if (bytes[0] == 0x00) {
      const bool mapped = std::memcmp(bytes, kMapped, 12) == 0;
      const bool loopback = std::memcmp(bytes, kLoopback, 16) == 0;
      const bool nat64 = std::memcmp(bytes, kNat64, 12) == 0;
      if (!mapped && !loopback && !nat64) flags |= guest::kNI_NUMERICHOST;
    } else if ((bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80) || bytes[0] == 0xff) {
      flags |= guest::kNI_NUMERICHOST;
    }
  }

  if (host == nullptr || host_length == 0) return 0;
  if ((flags & guest::kNI_NUMERICHOST) != 0) {
    if ((flags & guest::kNI_NAMEREQD) != 0) return guest::kEAI_NONAME;
    return NumericHost(family, bytes, scope, host, host_length);
  }
  int lookup_family = family;
  const std::uint8_t* lookup = bytes;
  static const std::uint8_t kMapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
  if (family == guest::kAF_INET6 && std::memcmp(bytes, kMapped, 12) == 0) {
    lookup_family = guest::kAF_INET;
    lookup = bytes + 12;
  }
  std::string name;
  if (ReverseName(lookup_family, lookup, &name) == 0) {
    if (name.size() + 1 > host_length) return guest::kEAI_MEMORY;
    std::memcpy(host, name.c_str(), name.size() + 1);
    return 0;
  }
  if ((flags & guest::kNI_NAMEREQD) != 0) return guest::kEAI_NONAME;
  if (family == guest::kAF_INET6) return NumericHost(family, bytes, scope, host, host_length);
  char text[INET_ADDRSTRLEN + 1];
  if (InetNtop(family, bytes, text, sizeof(text)) == nullptr) return guest::kEAI_SYSTEM;
  if (std::strlen(text) + 1 > host_length) {
    SetGuestErrno(guest::kENOSPC);
    return guest::kEAI_SYSTEM;
  }
  std::memcpy(host, text, std::strlen(text) + 1);
  return 0;
}

const char* GaiStrerror(int code) {
  static const char* const kMessages[] = {
      "Success",
      "Address family for hostname not supported",
      "Temporary failure in name resolution",
      "Invalid value for ai_flags",
      "Non-recoverable failure in name resolution",
      "ai_family not supported",
      "Memory allocation failure",
      "No address associated with hostname",
      "hostname nor servname provided, or not known",
      "servname not supported for ai_socktype",
      "ai_socktype not supported",
      "System error returned in errno",
      "Invalid value for hints",
      "Resolved protocol is unknown",
      "Argument buffer overflow",
      "Unknown error",
  };
  if (code < 0 || code > guest::kEAI_MAX) code = guest::kEAI_MAX;
  return kMessages[code];
}

const char* HostStrerror(int error) {
  static const char* const kMessages[] = {
      "Resolver Error 0 (no error)",
      "Unknown host",
      "Host name lookup failure",
      "Unknown server error",
      "No address associated with name",
  };
  if (error < 0) return "Resolver internal error";
  if (error < 5) return kMessages[error];
  return "Unknown resolver error";
}

struct HostData {
  std::string name;
  int family = guest::kAF_INET;
  std::vector<Address> addresses;
};

bool PackHostent(const HostData& data, guest::Hostent* out, char* buffer, std::size_t length) {
  const std::size_t address_length = static_cast<std::size_t>(AddressLength(data.family));
  auto base = reinterpret_cast<std::uintptr_t>(buffer);
  const std::uintptr_t aligned = (base + 7) & ~static_cast<std::uintptr_t>(7);
  const std::size_t skew = aligned - base;
  const std::size_t n = data.addresses.size();
  const std::size_t needed = skew + sizeof(char*) * (1 + n + 1) + n * address_length + data.name.size() + 1;
  if (buffer == nullptr || needed > length) return false;
  char** aliases = reinterpret_cast<char**>(aligned);
  char** list = aliases + 1;
  char* cursor = reinterpret_cast<char*>(list + n + 1);
  aliases[0] = nullptr;
  for (std::size_t i = 0; i < n; ++i) {
    std::memcpy(cursor, data.addresses[i].data(), address_length);
    list[i] = cursor;
    cursor += address_length;
  }
  list[n] = nullptr;
  std::memcpy(cursor, data.name.c_str(), data.name.size() + 1);
  out->h_name = cursor;
  out->h_aliases = aliases;
  out->h_addrtype = data.family;
  out->h_length = static_cast<std::int32_t>(address_length);
  out->h_addr_list = list;
  return true;
}

int LookupHost(const char* name, int family, HostData* data) {
  if (name == nullptr) return kHOST_NOT_FOUND;
  if (family != guest::kAF_INET && family != guest::kAF_INET6) {
    SetGuestErrno(guest::kEAFNOSUPPORT);
    return kNETDB_INTERNAL;
  }
  data->family = family;
  const std::size_t length = std::strlen(name);
  bool numeric_v4 = length > 0 && std::isdigit(static_cast<unsigned char>(name[0]));
  for (std::size_t i = 0; numeric_v4 && i < length; ++i) {
    numeric_v4 = std::isdigit(static_cast<unsigned char>(name[i])) || name[i] == '.';
  }
  bool numeric_v6 = length > 0 && (name[0] == ':' || (std::isxdigit(static_cast<unsigned char>(name[0])) &&
                                                      std::strchr(name, ':') != nullptr));
  for (std::size_t i = 0; numeric_v6 && i < length; ++i) {
    numeric_v6 = std::isxdigit(static_cast<unsigned char>(name[i])) || name[i] == ':' || name[i] == '.';
  }
  if ((numeric_v4 || numeric_v6) && name[length - 1] != '.') {
    Address address{};
    if (InetPton(family, name, address.data()) != 1) return kHOST_NOT_FOUND;
    data->name = name;
    data->addresses.push_back(address);
    return 0;
  }
  ResolvedName resolved;
  const int error = ResolveName(name, family, false, true, &resolved);
  if (error != 0) {
    if (error == guest::kEAI_NODATA || error == guest::kEAI_NONAME) return kHOST_NOT_FOUND;
    if (error == guest::kEAI_AGAIN) return kTRY_AGAIN;
    if (error == guest::kEAI_MEMORY) {
      SetGuestErrno(guest::kENOMEM);
      return kNETDB_INTERNAL;
    }
    return kNO_RECOVERY;
  }
  data->name = resolved.canonical.empty() ? Undotted(name) : resolved.canonical;
  for (const auto& address : resolved.addresses) {
    if (address.family == family) data->addresses.push_back(address.bytes);
  }
  return data->addresses.empty() ? kNO_DATA : 0;
}

int ReverseHost(const void* address, g_socklen length, int family, HostData* data) {
  if (address == nullptr) {
    SetGuestErrno(guest::kEINVAL);
    return kNETDB_INTERNAL;
  }
  const auto* bytes = static_cast<const std::uint8_t*>(address);
  static const std::uint8_t kMapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
  int lookup_family = family;
  const std::uint8_t* lookup = bytes;
  if (family == guest::kAF_INET6 && length == 16 && std::memcmp(bytes, kMapped, 12) == 0) {
    lookup_family = guest::kAF_INET;
    lookup = bytes + 12;
  }
  if (family != guest::kAF_INET && family != guest::kAF_INET6) {
    SetGuestErrno(guest::kEAFNOSUPPORT);
    return kNETDB_INTERNAL;
  }
  if (static_cast<int>(length) != AddressLength(family)) {
    SetGuestErrno(guest::kEINVAL);
    return kNETDB_INTERNAL;
  }
  std::string name;
  const int error = ReverseName(lookup_family, lookup, &name);
  if (error != 0) return error;
  data->name = name;
  data->family = family;
  Address copy{};
  std::memcpy(copy.data(), bytes, length);
  data->addresses.push_back(copy);
  return 0;
}

int FinishReentrant(int h_error, const HostData& data, guest::Hostent* ret, char* buffer, g_size length,
                    guest::Hostent** result, int* h_errno_out) {
  *result = nullptr;
  if (h_error == 0) {
    if (!PackHostent(data, ret, buffer, static_cast<std::size_t>(length))) {
      if (h_errno_out != nullptr) *h_errno_out = kNETDB_INTERNAL;
      SetGuestErrno(guest::kERANGE);
      return guest::kERANGE;
    }
    *result = ret;
    if (h_errno_out != nullptr) *h_errno_out = kNETDB_SUCCESS;
    return 0;
  }
  if (h_errno_out != nullptr) *h_errno_out = h_error;
  return h_error == kHOST_NOT_FOUND ? 0 : h_error;
}

struct StaticHost {
  guest::Hostent host;
  alignas(8) char buffer[8 * 1024];
};

StaticHost& ThreadHost() {
  thread_local StaticHost storage;
  return storage;
}

guest::Hostent* NonReentrant(int h_error, const HostData& data) {
  StaticHost& storage = ThreadHost();
  guest::Hostent* result = nullptr;
  FinishReentrant(h_error, data, &storage.host, storage.buffer, sizeof(storage.buffer), &result, &t_h_errno);
  return result;
}

thread_local int t_hosts_index = 0;

}

ELIXIR_BIONIC_FN(int, getaddrinfo)(const char* hostname, const char* servname, const guest::Addrinfo* hints,
                                   guest::Addrinfo** result) noexcept {
  try {
    return GetAddrInfoImpl(hostname, servname, hints, result);
  } catch (const std::bad_alloc&) {
    return guest::kEAI_MEMORY;
  } catch (...) {
    return guest::kEAI_FAIL;
  }
}
ELIXIR_BIONIC_FN(void, freeaddrinfo)(guest::Addrinfo* info) noexcept { FreeChain(info); }
ELIXIR_BIONIC_FN(const char*, gai_strerror)(int code) noexcept { return GaiStrerror(code); }
ELIXIR_BIONIC_FN(int, getnameinfo)(const void* address, g_socklen address_length, char* host, g_size host_length,
                                   char* serv, g_size serv_length, int flags) noexcept {
  try {
    return GetNameInfoImpl(address, address_length, host, static_cast<std::size_t>(host_length), serv,
                           static_cast<std::size_t>(serv_length), flags);
  } catch (const std::bad_alloc&) {
    return guest::kEAI_MEMORY;
  } catch (...) {
    return guest::kEAI_FAIL;
  }
}

ELIXIR_BIONIC_FN(int*, __get_h_errno)() noexcept { return &t_h_errno; }
ELIXIR_BIONIC_FN(const char*, hstrerror)(int error) noexcept { return HostStrerror(error); }
ELIXIR_BIONIC_FN(void, herror)(const char* prefix) noexcept {
  try {
    std::string line;
    if (prefix != nullptr && *prefix != '\0') {
      line = prefix;
      line += ": ";
    }
    line += HostStrerror(t_h_errno);
    line += "\n";
    GuestWriteStderr(line.data(), line.size());
  } catch (...) {
  }
}

ELIXIR_BIONIC_FN(int, gethostbyname_r)(const char* name, guest::Hostent* ret, char* buffer, g_size length,
                                       guest::Hostent** result, int* h_errno_out) noexcept {
  return Guarded(guest::kENOMEM, [&] {
    HostData data;
    return FinishReentrant(LookupHost(name, guest::kAF_INET, &data), data, ret, buffer, length, result, h_errno_out);
  });
}
ELIXIR_BIONIC_FN(int, gethostbyname2_r)(const char* name, int family, guest::Hostent* ret, char* buffer,
                                        g_size length, guest::Hostent** result, int* h_errno_out) noexcept {
  return Guarded(guest::kENOMEM, [&] {
    HostData data;
    return FinishReentrant(LookupHost(name, family, &data), data, ret, buffer, length, result, h_errno_out);
  });
}
ELIXIR_BIONIC_FN(int, gethostbyaddr_r)(const void* address, g_socklen length, int family, guest::Hostent* ret,
                                       char* buffer, g_size buffer_length, guest::Hostent** result,
                                       int* h_errno_out) noexcept {
  return Guarded(guest::kENOMEM, [&] {
    HostData data;
    return FinishReentrant(ReverseHost(address, length, family, &data), data, ret, buffer, buffer_length, result,
                           h_errno_out);
  });
}
ELIXIR_BIONIC_FN(guest::Hostent*, gethostbyname)(const char* name) noexcept {
  return Guarded(static_cast<guest::Hostent*>(nullptr), [&] {
    HostData data;
    return NonReentrant(LookupHost(name, guest::kAF_INET, &data), data);
  });
}
ELIXIR_BIONIC_FN(guest::Hostent*, gethostbyname2)(const char* name, int family) noexcept {
  return Guarded(static_cast<guest::Hostent*>(nullptr), [&] {
    HostData data;
    return NonReentrant(LookupHost(name, family, &data), data);
  });
}
ELIXIR_BIONIC_FN(guest::Hostent*, gethostbyaddr)(const void* address, g_socklen length, int family) noexcept {
  return Guarded(static_cast<guest::Hostent*>(nullptr), [&] {
    HostData data;
    return NonReentrant(ReverseHost(address, length, family, &data), data);
  });
}
ELIXIR_BIONIC_FN(guest::Hostent*, gethostent)() noexcept {
  return Guarded(static_cast<guest::Hostent*>(nullptr), [&]() -> guest::Hostent* {
    HostData data;
    if (t_hosts_index == 0) {
      data.name = "localhost";
      data.family = guest::kAF_INET;
      data.addresses.push_back(Address{127, 0, 0, 1});
    } else if (t_hosts_index == 1) {
      data.name = "ip6-localhost";
      data.family = guest::kAF_INET6;
      Address loopback{};
      loopback[15] = 1;
      data.addresses.push_back(loopback);
    } else {
      t_h_errno = kHOST_NOT_FOUND;
      return nullptr;
    }
    ++t_hosts_index;
    return NonReentrant(0, data);
  });
}
ELIXIR_BIONIC_FN(void, sethostent)(int) noexcept { t_hosts_index = 0; }
ELIXIR_BIONIC_FN(void, endhostent)() noexcept { t_hosts_index = 0; }

ELIXIR_BIONIC_FN(void*, getprotobyname)(const char*) noexcept { return nullptr; }
ELIXIR_BIONIC_FN(void*, getprotobynumber)(int) noexcept { return nullptr; }
ELIXIR_BIONIC_FN(void*, getprotoent)() noexcept { return nullptr; }
ELIXIR_BIONIC_FN(void, setprotoent)(int) noexcept {}
ELIXIR_BIONIC_FN(void, endprotoent)() noexcept {}
ELIXIR_BIONIC_FN(void*, getnetbyname)(const char*) noexcept { return nullptr; }
ELIXIR_BIONIC_FN(void*, getnetbyaddr)(std::uint32_t, int) noexcept { return nullptr; }
ELIXIR_BIONIC_FN(void*, getnetent)() noexcept { return nullptr; }
ELIXIR_BIONIC_FN(void, setnetent)(int) noexcept {}
ELIXIR_BIONIC_FN(void, endnetent)() noexcept {}

void RegisterNetdbExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("getaddrinfo", &elixir_bionic_getaddrinfo);
  add("freeaddrinfo", &elixir_bionic_freeaddrinfo);
  add("gai_strerror", &elixir_bionic_gai_strerror);
  add("getnameinfo", &elixir_bionic_getnameinfo);
  add("__get_h_errno", &elixir_bionic___get_h_errno);
  add("hstrerror", &elixir_bionic_hstrerror);
  add("herror", &elixir_bionic_herror);
  add("gethostbyname", &elixir_bionic_gethostbyname);
  add("gethostbyname2", &elixir_bionic_gethostbyname2);
  add("gethostbyname_r", &elixir_bionic_gethostbyname_r);
  add("gethostbyname2_r", &elixir_bionic_gethostbyname2_r);
  add("gethostbyaddr", &elixir_bionic_gethostbyaddr);
  add("gethostbyaddr_r", &elixir_bionic_gethostbyaddr_r);
  add("gethostent", &elixir_bionic_gethostent);
  add("sethostent", &elixir_bionic_sethostent);
  add("endhostent", &elixir_bionic_endhostent);
  add("getprotobyname", &elixir_bionic_getprotobyname);
  add("getprotobynumber", &elixir_bionic_getprotobynumber);
  add("getprotoent", &elixir_bionic_getprotoent);
  add("setprotoent", &elixir_bionic_setprotoent);
  add("endprotoent", &elixir_bionic_endprotoent);
  add("getnetbyname", &elixir_bionic_getnetbyname);
  add("getnetbyaddr", &elixir_bionic_getnetbyaddr);
  add("getnetent", &elixir_bionic_getnetent);
  add("setnetent", &elixir_bionic_setnetent);
  add("endnetent", &elixir_bionic_endnetent);
}

}
