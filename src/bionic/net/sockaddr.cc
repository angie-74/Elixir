// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

namespace elixir::bionic::net::internal {
namespace {

constexpr std::size_t kSunPathOffset = offsetof(guest::SockaddrUn, sun_path);

std::uint64_t Fnv1a(const std::string& text) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char c : text) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

const std::wstring& UnixDirectory() {
  static const std::wstring* directory = [] {
    wchar_t temp[MAX_PATH + 1];
    DWORD length = GetTempPathW(MAX_PATH + 1, temp);
    std::wstring base = length > 0 && length <= MAX_PATH ? std::wstring(temp, length) : std::wstring(L"C:\\Windows\\Temp\\");
    if (base.size() > 60) {
      wchar_t shortened[MAX_PATH + 1];
      const DWORD short_length = GetShortPathNameW(base.c_str(), shortened, MAX_PATH + 1);
      if (short_length > 0 && short_length < base.size()) base.assign(shortened, short_length);
    }
    if (!base.empty() && base.back() != L'\\') base.push_back(L'\\');
    base += L"elixir-unix\\";
    CreateDirectoryW(base.c_str(), nullptr);
    return new std::wstring(base);
  }();
  return *directory;
}

void CopyBytes(void* destination, const void* source, std::size_t size) { std::memcpy(destination, source, size); }

std::uint16_t ReadFamily(const void* address) {
  std::uint16_t family;
  std::memcpy(&family, address, sizeof(family));
  return family;
}

}

int GuestFamilyToHost(int family) noexcept {
  switch (family) {
    case guest::kAF_UNSPEC: return AF_UNSPEC;
    case guest::kAF_INET: return AF_INET;
    case guest::kAF_INET6: return AF_INET6;
    case guest::kAF_UNIX: return AF_UNIX;
    default: return -1;
  }
}

int HostFamilyToGuest(int family) noexcept {
  switch (family) {
    case AF_UNSPEC: return guest::kAF_UNSPEC;
    case AF_INET: return guest::kAF_INET;
    case AF_INET6: return guest::kAF_INET6;
    case AF_UNIX: return guest::kAF_UNIX;
    default: return -1;
  }
}

std::wstring UnixHostPath(const std::string& name) {
  wchar_t file[40];
  const bool abstract = !name.empty() && name[0] == '\0';
  swprintf(file, 40, L"%c-%016llx.sock", abstract ? L'a' : L'p', static_cast<unsigned long long>(Fnv1a(name)));
  return UnixDirectory() + file;
}

std::string UnixAutobindName() {
  static std::atomic<std::uint32_t> counter{GetCurrentProcessId() * 2654435761u};
  char digits[8];
  std::snprintf(digits, sizeof(digits), "%05x", counter.fetch_add(1) & 0xfffffu);
  std::string name(1, '\0');
  name += digits;
  return name;
}

bool IsLoopbackHost(const sockaddr* host) noexcept {
  if (host->sa_family == AF_INET) {
    const auto* in = reinterpret_cast<const sockaddr_in*>(host);
    return (ntohl(in->sin_addr.S_un.S_addr) >> 24) == 127;
  }
  if (host->sa_family == AF_INET6) {
    const auto* in6 = reinterpret_cast<const sockaddr_in6*>(host);
    static const std::uint8_t kLoopback[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    if (std::memcmp(in6->sin6_addr.u.Byte, kLoopback, 16) == 0) return true;
    static const std::uint8_t kMapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    return std::memcmp(in6->sin6_addr.u.Byte, kMapped, 12) == 0 && in6->sin6_addr.u.Byte[12] == 127;
  }
  return false;
}

int GuestToHostAddr(const SocketState& state, const void* guest_addr, g_socklen guest_len, AddrUse use,
                    HostAddr* out, std::string* unix_name) noexcept {
  try {
    if (guest_addr == nullptr) return guest::kEFAULT;
    if (static_cast<std::int32_t>(guest_len) < 0 || guest_len > sizeof(guest::SockaddrStorage)) return guest::kEINVAL;
    if (guest_len < sizeof(std::uint16_t)) return guest::kEINVAL;
    std::memset(&out->storage, 0, sizeof(out->storage));
    const int family = ReadFamily(guest_addr);

    if (state.domain == guest::kAF_UNIX) {
      if (family != guest::kAF_UNIX) return guest::kEINVAL;
      if (guest_len > sizeof(guest::SockaddrUn)) return guest::kEINVAL;
      if (state.emulated_unix) return guest::kEOPNOTSUPP;
      const char* path = static_cast<const char*>(guest_addr) + kSunPathOffset;
      std::size_t length = guest_len - kSunPathOffset;
      std::string name;
      if (length == 0) {
        if (use != AddrUse::kBind) return guest::kEINVAL;
        name = UnixAutobindName();
      } else if (path[0] == '\0') {
        name.assign(path, length);
      } else {
        std::size_t end = 0;
        while (end < length && path[end] != '\0') ++end;
        name.assign(path, end);
      }
      const std::wstring host = UnixHostPath(name);
      const int bytes = WideCharToMultiByte(CP_UTF8, 0, host.c_str(), static_cast<int>(host.size()), nullptr, 0,
                                            nullptr, nullptr);
      auto* un = reinterpret_cast<SOCKADDR_UN*>(&out->storage);
      if (bytes <= 0 || static_cast<std::size_t>(bytes) >= sizeof(un->sun_path)) return guest::kENAMETOOLONG;
      un->sun_family = AF_UNIX;
      WideCharToMultiByte(CP_UTF8, 0, host.c_str(), static_cast<int>(host.size()), un->sun_path, bytes, nullptr,
                          nullptr);
      un->sun_path[bytes] = '\0';
      out->length = static_cast<int>(offsetof(SOCKADDR_UN, sun_path)) + bytes + 1;
      if (unix_name != nullptr) *unix_name = std::move(name);
      return 0;
    }

    if (state.domain == guest::kAF_INET) {
      if (family != guest::kAF_INET) {
        if (use == AddrUse::kBind && family == guest::kAF_UNSPEC && guest_len >= sizeof(guest::SockaddrIn)) {
          guest::SockaddrIn in;
          CopyBytes(&in, guest_addr, sizeof(in));
          std::uint32_t address;
          std::memcpy(&address, &in.sin_addr, sizeof(address));
          if (address != 0) return guest::kEAFNOSUPPORT;
        } else {
          return guest::kEAFNOSUPPORT;
        }
      }
      if (guest_len < sizeof(guest::SockaddrIn)) return guest::kEINVAL;
      auto* in = reinterpret_cast<sockaddr_in*>(&out->storage);
      CopyBytes(in, guest_addr, sizeof(sockaddr_in));
      in->sin_family = AF_INET;
      std::memset(in->sin_zero, 0, sizeof(in->sin_zero));
      out->length = sizeof(sockaddr_in);
      return 0;
    }

    if (state.domain == guest::kAF_INET6) {
      auto* in6 = reinterpret_cast<sockaddr_in6*>(&out->storage);
      if (family == guest::kAF_INET && use != AddrUse::kBind && state.type != guest::kSOCK_STREAM) {
        if (guest_len < sizeof(guest::SockaddrIn)) return guest::kEINVAL;
        guest::SockaddrIn in;
        CopyBytes(&in, guest_addr, sizeof(in));
        in6->sin6_family = AF_INET6;
        in6->sin6_port = in.sin_port;
        in6->sin6_addr.u.Byte[10] = 0xff;
        in6->sin6_addr.u.Byte[11] = 0xff;
        std::memcpy(&in6->sin6_addr.u.Byte[12], &in.sin_addr, 4);
        out->length = sizeof(sockaddr_in6);
        return 0;
      }
      if (family != guest::kAF_INET6) return guest::kEAFNOSUPPORT;
      if (guest_len < kSIN6_LEN_RFC2133) return guest::kEINVAL;
      CopyBytes(in6, guest_addr, guest_len >= sizeof(guest::SockaddrIn6) ? sizeof(guest::SockaddrIn6)
                                                                           : kSIN6_LEN_RFC2133);
      in6->sin6_family = AF_INET6;
      out->length = sizeof(sockaddr_in6);
      return 0;
    }
    return guest::kEAFNOSUPPORT;
  } catch (...) {
    return guest::kENOMEM;
  }
}

std::uint32_t HostToGuestSockaddr(const sockaddr* host, int host_len, guest::SockaddrStorage* out) noexcept {
  std::memset(out, 0, sizeof(*out));
  if (host == nullptr || host_len < static_cast<int>(sizeof(std::uint16_t))) return 0;
  switch (host->sa_family) {
    case AF_INET: {
      guest::SockaddrIn in{};
      CopyBytes(&in, host, sizeof(in));
      in.sin_family = static_cast<std::uint16_t>(guest::kAF_INET);
      CopyBytes(out, &in, sizeof(in));
      return sizeof(guest::SockaddrIn);
    }
    case AF_INET6: {
      guest::SockaddrIn6 in6{};
      CopyBytes(&in6, host, sizeof(in6));
      in6.sin6_family = static_cast<std::uint16_t>(guest::kAF_INET6);
      CopyBytes(out, &in6, sizeof(in6));
      return sizeof(guest::SockaddrIn6);
    }
    case AF_UNIX:
      out->ss_family = static_cast<std::uint16_t>(guest::kAF_UNIX);
      return static_cast<std::uint32_t>(kSunPathOffset);
    default:
      return 0;
  }
}

std::uint32_t UnixSockaddr(const std::string& name, guest::SockaddrStorage* out) noexcept {
  std::memset(out, 0, sizeof(*out));
  out->ss_family = static_cast<std::uint16_t>(guest::kAF_UNIX);
  auto* un = reinterpret_cast<guest::SockaddrUn*>(out);
  const std::size_t room = sizeof(un->sun_path);
  const std::size_t length = name.size() < room ? name.size() : room;
  std::memcpy(un->sun_path, name.data(), length);
  if (name.empty()) return static_cast<std::uint32_t>(kSunPathOffset);
  if (name[0] == '\0') return static_cast<std::uint32_t>(kSunPathOffset + length);
  return static_cast<std::uint32_t>(kSunPathOffset + length + (length < room ? 1 : 0));
}

int CopySockaddrOut(const guest::SockaddrStorage& value, std::uint32_t length, void* guest_addr,
                    g_socklen* guest_len) noexcept {
  if (guest_len == nullptr) return guest::kEFAULT;
  const std::int32_t room = static_cast<std::int32_t>(*guest_len);
  if (room < 0) return guest::kEINVAL;
  const std::uint32_t copy = static_cast<std::uint32_t>(room) < length ? static_cast<std::uint32_t>(room) : length;
  if (copy > 0) {
    if (guest_addr == nullptr) return guest::kEFAULT;
    std::memcpy(guest_addr, &value, copy);
  }
  *guest_len = length;
  return 0;
}

}
