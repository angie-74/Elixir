// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <algorithm>
#include <cstring>

#include "bionic/memory.h"

namespace elixir::bionic::net::internal {
namespace {

struct UnicastAddress {
  int family = 0;
  std::uint8_t bytes[16] = {};
  std::uint8_t prefix = 0;
};

struct Interface {
  std::uint32_t index = 0;
  std::string name;
  std::uint32_t flags = 0;
  std::vector<UnicastAddress> addresses;
};

bool Snapshot(std::vector<Interface>* out) {
  ULONG size = 16 * 1024;
  std::vector<std::uint8_t> buffer;
  ULONG rc = ERROR_BUFFER_OVERFLOW;
  const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER |
                      GAA_FLAG_SKIP_FRIENDLY_NAME;
  for (int attempt = 0; attempt < 4 && rc == ERROR_BUFFER_OVERFLOW; ++attempt) {
    buffer.resize(size);
    rc = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()),
                              &size);
  }
  std::vector<const IP_ADAPTER_ADDRESSES*> adapters;
  if (rc == NO_ERROR) {
    for (auto* it = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(buffer.data()); it != nullptr; it = it->Next) {
      adapters.push_back(it);
    }
  } else if (rc != ERROR_NO_DATA) {
    SetGuestErrno(rc == ERROR_NOT_ENOUGH_MEMORY ? guest::kENOMEM : guest::kEIO);
    return false;
  }
  auto index_of = [](const IP_ADAPTER_ADDRESSES* a) -> std::uint32_t {
    return a->IfIndex != 0 ? a->IfIndex : a->Ipv6IfIndex;
  };
  std::sort(adapters.begin(), adapters.end(),
            [&](const IP_ADAPTER_ADDRESSES* a, const IP_ADAPTER_ADDRESSES* b) { return index_of(a) < index_of(b); });
  int eth = 0;
  int wlan = 0;
  int ppp = 0;
  int tun = 0;
  bool have_loopback = false;
  for (const IP_ADAPTER_ADDRESSES* adapter : adapters) {
    Interface entry;
    entry.index = index_of(adapter);
    const bool up = adapter->OperStatus == IfOperStatusUp;
    const bool dormant = adapter->OperStatus == IfOperStatusDormant;
    switch (adapter->IfType) {
      case IF_TYPE_SOFTWARE_LOOPBACK:
        entry.name = have_loopback ? "lo" + std::to_string(entry.index) : "lo";
        have_loopback = true;
        entry.flags = kIFF_LOOPBACK;
        break;
      case IF_TYPE_IEEE80211:
        entry.name = "wlan" + std::to_string(wlan++);
        entry.flags = kIFF_BROADCAST | kIFF_MULTICAST;
        break;
      case IF_TYPE_PPP:
        entry.name = "ppp" + std::to_string(ppp++);
        entry.flags = kIFF_POINTOPOINT | kIFF_NOARP | kIFF_MULTICAST;
        break;
      case IF_TYPE_TUNNEL:
        entry.name = "tun" + std::to_string(tun++);
        entry.flags = kIFF_POINTOPOINT | kIFF_NOARP | kIFF_MULTICAST;
        break;
      default:
        entry.name = "eth" + std::to_string(eth++);
        entry.flags = kIFF_BROADCAST | kIFF_MULTICAST;
        break;
    }
    if ((adapter->Flags & IP_ADAPTER_NO_MULTICAST) != 0) entry.flags &= ~kIFF_MULTICAST;
    if (up) entry.flags |= kIFF_UP | kIFF_RUNNING;
    if (dormant) entry.flags |= kIFF_UP;
    for (const IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != nullptr;
         unicast = unicast->Next) {
      const sockaddr* address = unicast->Address.lpSockaddr;
      if (address == nullptr) continue;
      UnicastAddress item;
      if (address->sa_family == AF_INET) {
        item.family = guest::kAF_INET;
        std::memcpy(item.bytes, &reinterpret_cast<const sockaddr_in*>(address)->sin_addr, 4);
      } else if (address->sa_family == AF_INET6) {
        item.family = guest::kAF_INET6;
        std::memcpy(item.bytes, &reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr, 16);
      } else {
        continue;
      }
      item.prefix = unicast->OnLinkPrefixLength;
      entry.addresses.push_back(item);
    }
    out->push_back(std::move(entry));
  }
  if (!have_loopback) {
    Interface lo;
    lo.index = 1;
    lo.name = "lo";
    lo.flags = kIFF_UP | kIFF_LOOPBACK | kIFF_RUNNING;
    UnicastAddress v4;
    v4.family = guest::kAF_INET;
    v4.bytes[0] = 127;
    v4.bytes[3] = 1;
    v4.prefix = 8;
    UnicastAddress v6;
    v6.family = guest::kAF_INET6;
    v6.bytes[15] = 1;
    v6.prefix = 128;
    lo.addresses = {v4, v6};
    out->insert(out->begin(), std::move(lo));
  }
  return true;
}

void FillSockaddr(int family, const std::uint8_t* bytes, std::uint32_t scope, guest::Sockaddr* out) {
  if (family == guest::kAF_INET) {
    guest::SockaddrIn in{};
    in.sin_family = static_cast<std::uint16_t>(guest::kAF_INET);
    std::memcpy(&in.sin_addr, bytes, 4);
    std::memcpy(out, &in, sizeof(in));
  } else {
    guest::SockaddrIn6 in6{};
    in6.sin6_family = static_cast<std::uint16_t>(guest::kAF_INET6);
    std::memcpy(in6.sin6_addr.s6_addr, bytes, 16);
    in6.sin6_scope_id = scope;
    std::memcpy(out, &in6, sizeof(in6));
  }
}

void PrefixMask(int family, std::uint8_t prefix, std::uint8_t* mask) {
  const int bits = family == guest::kAF_INET ? 32 : 128;
  std::memset(mask, 0, 16);
  int remaining = prefix > bits ? bits : prefix;
  for (int i = 0; remaining > 0; ++i, remaining -= 8) {
    mask[i] = remaining >= 8 ? 0xff : static_cast<std::uint8_t>(0xff << (8 - remaining));
  }
}

struct NodeBlock {
  GuestIfaddrs node;
  alignas(8) std::uint8_t addr[32];
  alignas(8) std::uint8_t netmask[32];
  alignas(8) std::uint8_t ifu[32];
  char name[24];
};

int GetIfaddrsImpl(GuestIfaddrs** out) {
  if (out == nullptr) {
    SetGuestErrno(guest::kEFAULT);
    return -1;
  }
  *out = nullptr;
  if (!EnsureWinsock()) {
    SetGuestErrno(guest::kENETDOWN);
    return -1;
  }
  std::vector<Interface> interfaces;
  if (!Snapshot(&interfaces)) return -1;
  std::size_t count = 0;
  for (const Interface& entry : interfaces) count += entry.addresses.size();
  if (count == 0) return 0;
  auto* blocks = static_cast<NodeBlock*>(GuestCalloc(count, sizeof(NodeBlock)));
  if (blocks == nullptr) {
    SetGuestErrno(guest::kENOMEM);
    return -1;
  }
  std::size_t n = 0;
  for (const Interface& entry : interfaces) {
    for (const UnicastAddress& address : entry.addresses) {
      NodeBlock& block = blocks[n];
      const std::size_t name_length = entry.name.size() < sizeof(block.name) - 1 ? entry.name.size()
                                                                                  : sizeof(block.name) - 1;
      std::memcpy(block.name, entry.name.data(), name_length);
      block.node.ifa_name = block.name;
      block.node.ifa_flags = entry.flags;
      const bool link_local = address.family == guest::kAF_INET6 && address.bytes[0] == 0xfe &&
                              (address.bytes[1] & 0xc0) == 0x80;
      FillSockaddr(address.family, address.bytes, link_local ? entry.index : 0,
                   reinterpret_cast<guest::Sockaddr*>(block.addr));
      block.node.ifa_addr = reinterpret_cast<guest::Sockaddr*>(block.addr);
      std::uint8_t mask[16];
      PrefixMask(address.family, address.prefix, mask);
      FillSockaddr(address.family, mask, 0, reinterpret_cast<guest::Sockaddr*>(block.netmask));
      block.node.ifa_netmask = reinterpret_cast<guest::Sockaddr*>(block.netmask);
      if (address.family == guest::kAF_INET && (entry.flags & kIFF_BROADCAST) != 0) {
        std::uint8_t broadcast[4];
        for (int i = 0; i < 4; ++i) broadcast[i] = static_cast<std::uint8_t>(address.bytes[i] | ~mask[i]);
        FillSockaddr(guest::kAF_INET, broadcast, 0, reinterpret_cast<guest::Sockaddr*>(block.ifu));
        block.node.ifa_ifu = reinterpret_cast<guest::Sockaddr*>(block.ifu);
      }
      block.node.ifa_next = n + 1 < count ? &blocks[n + 1].node : nullptr;
      ++n;
    }
  }
  *out = &blocks[0].node;
  return 0;
}

}

bool InterfaceNameFromIndex(std::uint32_t index, std::string* name) {
  std::vector<Interface> interfaces;
  if (!Snapshot(&interfaces)) return false;
  for (const Interface& entry : interfaces) {
    if (entry.index == index) {
      *name = entry.name;
      return true;
    }
  }
  return false;
}

std::uint32_t InterfaceIndexFromName(const char* name) {
  if (name == nullptr) return 0;
  std::vector<Interface> interfaces;
  if (!Snapshot(&interfaces)) return 0;
  for (const Interface& entry : interfaces) {
    if (entry.name == name) return entry.index;
  }
  return 0;
}

ELIXIR_BIONIC_FN(int, getifaddrs)(GuestIfaddrs** out) noexcept {
  return Guarded(-1, [&] { return GetIfaddrsImpl(out); });
}
ELIXIR_BIONIC_FN(void, freeifaddrs)(GuestIfaddrs* list) noexcept {
  if (list != nullptr) GuestFree(list);
}
ELIXIR_BIONIC_FN(std::uint32_t, if_nametoindex)(const char* name) noexcept {
  return Guarded(0u, [&] {
    const std::uint32_t index = InterfaceIndexFromName(name);
    if (index == 0) SetGuestErrno(guest::kENODEV);
    return index;
  });
}
ELIXIR_BIONIC_FN(char*, if_indextoname)(std::uint32_t index, char* buffer) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    std::string name;
    if (buffer == nullptr || !InterfaceNameFromIndex(index, &name)) {
      SetGuestErrno(guest::kENXIO);
      return nullptr;
    }
    const std::size_t length = name.size() < static_cast<std::size_t>(guest::kIFNAMSIZ) - 1
                                   ? name.size()
                                   : static_cast<std::size_t>(guest::kIFNAMSIZ) - 1;
    std::memcpy(buffer, name.data(), length);
    buffer[length] = '\0';
    return buffer;
  });
}
ELIXIR_BIONIC_FN(GuestIfNameindex*, if_nameindex)() noexcept {
  return Guarded(static_cast<GuestIfNameindex*>(nullptr), [&]() -> GuestIfNameindex* {
    std::vector<Interface> interfaces;
    if (!Snapshot(&interfaces)) return nullptr;
    const std::size_t entries = interfaces.size() + 1;
    std::size_t names = 0;
    for (const Interface& entry : interfaces) names += entry.name.size() + 1;
    auto* block = static_cast<GuestIfNameindex*>(GuestCalloc(1, entries * sizeof(GuestIfNameindex) + names));
    if (block == nullptr) {
      SetGuestErrno(guest::kENOBUFS);
      return nullptr;
    }
    char* text = reinterpret_cast<char*>(block + entries);
    for (std::size_t i = 0; i < interfaces.size(); ++i) {
      block[i].if_index = interfaces[i].index;
      block[i].if_name = text;
      std::memcpy(text, interfaces[i].name.c_str(), interfaces[i].name.size() + 1);
      text += interfaces[i].name.size() + 1;
    }
    return block;
  });
}
ELIXIR_BIONIC_FN(void, if_freenameindex)(GuestIfNameindex* list) noexcept {
  if (list != nullptr) GuestFree(list);
}

void RegisterIfaddrsExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("getifaddrs", &elixir_bionic_getifaddrs);
  add("freeifaddrs", &elixir_bionic_freeifaddrs);
  add("if_nametoindex", &elixir_bionic_if_nametoindex);
  add("if_indextoname", &elixir_bionic_if_indextoname);
  add("if_nameindex", &elixir_bionic_if_nameindex);
  add("if_freenameindex", &elixir_bionic_if_freenameindex);
}

}
