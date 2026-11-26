// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/dns_config_windows.h"

#include "abi/sysv_abi.h"
#include "bionic/net/dns_native.h"
#include "bionic/runtime.h"
#include "platform/windows/guest_tls.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <windows.h>

#include <mcpelauncher/linker.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace elixir::bionic::dns_config {
namespace {

constexpr int kAndroidLogInfo = 4;
constexpr int kAndroidLogWarn = 5;
constexpr DWORD kDebounceMs = 500;
constexpr unsigned kNetidUnset = 0;

bool IsPlaceholder(const ServerAddress& server) {
  static const std::uint8_t kPrefix[15] = {0xfe, 0xc0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0};
  return server.family == AF_INET6 && std::memcmp(server.bytes, kPrefix, sizeof(kPrefix)) == 0 &&
         server.bytes[15] >= 1 && server.bytes[15] <= 3;
}

bool IsUnspecified(const ServerAddress& server) {
  const std::size_t length = server.family == AF_INET ? 4 : 16;
  for (std::size_t i = 0; i < length; ++i) {
    if (server.bytes[i] != 0) return false;
  }
  return true;
}

std::uint32_t AdapterMetric(const AdapterDns& adapter) {
  std::uint32_t metric = UINT32_MAX;
  if (adapter.ipv4_enabled) metric = std::min(metric, adapter.ipv4_metric);
  if (adapter.ipv6_enabled) metric = std::min(metric, adapter.ipv6_metric);
  return metric;
}

std::string FormatServer(const ServerAddress& server, const AdapterDns& adapter) {
  if (server.family == AF_INET) {
    char text[16];
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u", server.bytes[0], server.bytes[1], server.bytes[2],
                  server.bytes[3]);
    return text;
  }
  std::string text = FormatIpv6(server.bytes);
  const bool link_local = server.bytes[0] == 0xfe && (server.bytes[1] & 0xc0) == 0x80;
  if (link_local) {
    const std::uint32_t scope = server.scope != 0 ? server.scope : adapter.ipv6_interface_index;
    if (scope != 0) text += "%" + std::to_string(scope);
  }
  return text;
}

std::string Lower(const std::string& text) {
  std::string lowered = text;
  for (char& c : lowered) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return lowered;
}

std::string Trim(const std::string& text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

using SetNameserversFn = abi::SysvFn<int(unsigned, const char**, unsigned, const char*, const void*)>;
using NetidFn = abi::SysvFn<void(unsigned)>;

struct GuestResolver {
  SetNameserversFn set_nameservers = nullptr;
  NetidFn flush = nullptr;
  NetidFn remove = nullptr;
};

bool ResolveGuest(GuestResolver* resolver) {
  void* libc = linker::dlopen("libc.so", RTLD_NOW | RTLD_NOLOAD);
  if (libc == nullptr) return false;
  resolver->set_nameservers =
      reinterpret_cast<SetNameserversFn>(linker::dlsym(libc, "_resolv_set_nameservers_for_net"));
  resolver->flush = reinterpret_cast<NetidFn>(linker::dlsym(libc, "_resolv_flush_cache_for_net"));
  resolver->remove = reinterpret_cast<NetidFn>(linker::dlsym(libc, "_resolv_delete_cache_for_net"));
  return resolver->set_nameservers != nullptr && resolver->flush != nullptr && resolver->remove != nullptr;
}

std::mutex g_lock;
bool g_configured = false;
GuestResolver g_resolver;
Status g_status;

HANDLE g_watch_handle = nullptr;
std::atomic<HANDLE> g_change_event{nullptr};
HANDLE g_stop_event = nullptr;
HANDLE g_worker_done = nullptr;
std::atomic<std::uint64_t> g_notifications{0};

void Log(int priority, const std::string& message) { HostLog(priority, "elixir", message.c_str()); }

std::string Describe(const DnsConfiguration& configuration) {
  std::string text = std::to_string(configuration.servers.size()) + " server(s)";
  for (const std::string& server : configuration.servers) text += " " + server;
  text += ", search \"" + configuration.search_domains + "\"";
  return text;
}

bool ApplyLocked(const DnsConfiguration& configuration, bool flush, std::string* error) {
  if (platform::EnsureGuestThread() == nullptr) {
    *error = "cannot adopt the thread for guest calls";
    return false;
  }
  std::vector<const char*> servers;
  servers.reserve(configuration.servers.size());
  for (const std::string& server : configuration.servers) servers.push_back(server.c_str());
  const int rc = g_resolver.set_nameservers(kNetidUnset, servers.empty() ? nullptr : servers.data(),
                                            static_cast<unsigned>(servers.size()),
                                            configuration.search_domains.c_str(), nullptr);
  if (rc != 0) {
    *error = "_resolv_set_nameservers_for_net(0) failed with " + std::to_string(rc) + " for " + Describe(configuration);
    return false;
  }
  ++g_status.applications;
  g_status.applied = configuration;
  if (flush) {
    g_resolver.flush(kNetidUnset);
    ++g_status.flushes;
  }
  return true;
}

bool DiscoverAndApplyLocked(bool flush, std::string* error) {
  DnsConfiguration configuration;
  if (!DiscoverDnsConfiguration(&configuration, error)) return false;
  return ApplyLocked(configuration, flush, error);
}

VOID NETIOAPI_API_ OnInterfaceChange(PVOID, PMIB_IPINTERFACE_ROW, MIB_NOTIFICATION_TYPE) {
  g_notifications.fetch_add(1, std::memory_order_relaxed);
  const HANDLE change = g_change_event.load(std::memory_order_acquire);
  if (change != nullptr) SetEvent(change);
}

void WatchChanges(HANDLE change, HANDLE stop, HANDLE done) noexcept {
  try {
    platform::EnsureGuestThread();
  } catch (...) {
  }
  const HANDLE waits[2] = {stop, change};
  for (;;) {
    if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) break;
    DWORD quiet = WAIT_OBJECT_0 + 1;
    while (quiet == WAIT_OBJECT_0 + 1) quiet = WaitForMultipleObjects(2, waits, FALSE, kDebounceMs);
    if (quiet != WAIT_TIMEOUT) break;
    try {
      std::lock_guard<std::mutex> lock(g_lock);
      g_status.notifications = g_notifications.load(std::memory_order_relaxed);
      std::string error;
      if (DiscoverAndApplyLocked(true, &error)) {
        Log(kAndroidLogInfo, "dns: network change: netid 0 now uses " + Describe(g_status.applied));
      } else {
        g_status.error = error;
        Log(kAndroidLogWarn, "dns: network change: " + error);
      }
    } catch (...) {
    }
  }
  SetEvent(done);
}

void StopWatcher(std::unique_lock<std::mutex>& lock) {
  HANDLE watch = g_watch_handle;
  HANDLE change = g_change_event.load(std::memory_order_acquire);
  HANDLE stop = g_stop_event;
  HANDLE done = g_worker_done;
  g_watch_handle = nullptr;
  g_stop_event = nullptr;
  g_worker_done = nullptr;
  lock.unlock();
  if (watch != nullptr) CancelMibChangeNotify2(watch);
  if (stop != nullptr) {
    SetEvent(stop);
    WaitForSingleObject(done, INFINITE);
    CloseHandle(stop);
    CloseHandle(done);
  }
  lock.lock();
  HANDLE expected = change;
  if (change != nullptr) g_change_event.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
  if (change != nullptr) CloseHandle(change);
}

bool StartWatcher(std::unique_lock<std::mutex>& lock, std::string* error) {
  const HANDLE change = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  g_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  g_worker_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  const auto undo = [&] {
    if (change != nullptr) CloseHandle(change);
    if (g_stop_event != nullptr) CloseHandle(g_stop_event);
    if (g_worker_done != nullptr) CloseHandle(g_worker_done);
    g_stop_event = g_worker_done = nullptr;
  };
  if (change == nullptr || g_stop_event == nullptr || g_worker_done == nullptr) {
    *error = "CreateEvent failed (" + std::to_string(GetLastError()) + ")";
    undo();
    return false;
  }
  try {
    std::thread(WatchChanges, change, g_stop_event, g_worker_done).detach();
  } catch (const std::exception& e) {
    *error = std::string("cannot start the interface change watcher: ") + e.what();
    undo();
    return false;
  }
  g_change_event.store(change, std::memory_order_release);
  const DWORD rc = NotifyIpInterfaceChange(AF_UNSPEC, &OnInterfaceChange, nullptr, FALSE, &g_watch_handle);
  if (rc != NO_ERROR) {
    g_watch_handle = nullptr;
    *error = "NotifyIpInterfaceChange failed (" + std::to_string(rc) + ")";
    StopWatcher(lock);
    return false;
  }
  return true;
}

bool StartFallback(std::unique_lock<std::mutex>& lock, std::string* error) {
  if (!DiscoverAndApplyLocked(false, error)) return false;
  if (g_watch_handle == nullptr && !StartWatcher(lock, error)) return false;
  g_status.mode = Mode::kFallback;
  g_status.error.clear();
  return true;
}

}

std::string FormatIpv6(const std::uint8_t bytes[16]) {
  std::uint16_t groups[8];
  for (int i = 0; i < 8; ++i) groups[i] = static_cast<std::uint16_t>((bytes[2 * i] << 8) | bytes[2 * i + 1]);
  int best_start = -1;
  int best_length = 0;
  for (int i = 0; i < 8;) {
    if (groups[i] != 0) {
      ++i;
      continue;
    }
    int j = i;
    while (j < 8 && groups[j] == 0) ++j;
    if (j - i > best_length && j - i >= 2) {
      best_start = i;
      best_length = j - i;
    }
    i = j;
  }
  const bool mapped = groups[0] == 0 && groups[1] == 0 && groups[2] == 0 && groups[3] == 0 && groups[4] == 0 &&
                      groups[5] == 0xffff;
  std::string text;
  char part[8];
  for (int i = 0; i < 8; ++i) {
    if (i == best_start) {
      text += "::";
      i += best_length - 1;
      continue;
    }
    if (mapped && i == 6) {
      char tail[20];
      std::snprintf(tail, sizeof(tail), "%u.%u.%u.%u", bytes[12], bytes[13], bytes[14], bytes[15]);
      if (!text.empty() && text.back() != ':') text += ":";
      text += tail;
      break;
    }
    if (!text.empty() && text.back() != ':') text += ":";
    std::snprintf(part, sizeof(part), "%x", groups[i]);
    text += part;
  }
  return text;
}

DnsConfiguration SelectDnsConfiguration(const std::vector<AdapterDns>& adapters, const std::string& search_list) {
  std::vector<const AdapterDns*> usable;
  for (const AdapterDns& adapter : adapters) {
    if (adapter.operational && !adapter.loopback) usable.push_back(&adapter);
  }
  std::stable_sort(usable.begin(), usable.end(), [](const AdapterDns* a, const AdapterDns* b) {
    if (a->has_default_gateway != b->has_default_gateway) return a->has_default_gateway;
    return AdapterMetric(*a) < AdapterMetric(*b);
  });

  DnsConfiguration configuration;
  for (const AdapterDns* adapter : usable) {
    for (const ServerAddress& server : adapter->servers) {
      if (configuration.servers.size() >= kMaxServers) break;
      if (server.family != AF_INET && server.family != AF_INET6) continue;
      if (IsPlaceholder(server) || IsUnspecified(server)) continue;
      const std::string text = FormatServer(server, *adapter);
      if (std::find(configuration.servers.begin(), configuration.servers.end(), text) == configuration.servers.end()) {
        configuration.servers.push_back(text);
      }
    }
  }

  std::vector<std::string> candidates;
  std::size_t start = 0;
  while (start <= search_list.size()) {
    const std::size_t comma = search_list.find(',', start);
    const std::size_t end = comma == std::string::npos ? search_list.size() : comma;
    candidates.push_back(Trim(search_list.substr(start, end - start)));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  for (const AdapterDns* adapter : usable) candidates.push_back(Trim(adapter->dns_suffix));
  std::vector<std::string> seen;
  std::size_t count = 0;
  for (std::string domain : candidates) {
    while (!domain.empty() && domain.back() == '.') domain.pop_back();
    if (domain.empty() || domain.find_first_of(" \t") != std::string::npos) continue;
    const std::string key = Lower(domain);
    if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
    if (count >= kMaxSearchDomains) break;
    const std::size_t grown =
        configuration.search_domains.size() + (configuration.search_domains.empty() ? 0 : 1) + domain.size();
    if (grown > kMaxSearchPath - 1) break;
    if (!configuration.search_domains.empty()) configuration.search_domains += " ";
    configuration.search_domains += domain;
    seen.push_back(key);
    ++count;
  }
  return configuration;
}

bool ReadAdapters(std::vector<AdapterDns>* adapters, std::string* error) {
  adapters->clear();
  constexpr ULONG kFlags =
      GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME | GAA_FLAG_INCLUDE_GATEWAYS;
  ULONG size = 32 * 1024;
  std::vector<unsigned char> buffer;
  ULONG rc = ERROR_BUFFER_OVERFLOW;
  for (int attempt = 0; attempt < 4 && rc == ERROR_BUFFER_OVERFLOW; ++attempt) {
    buffer.resize(size);
    rc = GetAdaptersAddresses(AF_UNSPEC, kFlags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()),
                              &size);
  }
  if (rc == ERROR_NO_DATA) return true;
  if (rc != NO_ERROR) {
    *error = "GetAdaptersAddresses failed (" + std::to_string(rc) + ")";
    return false;
  }
  for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter != nullptr;
       adapter = adapter->Next) {
    AdapterDns dns;
    dns.operational = adapter->OperStatus == IfOperStatusUp;
    dns.loopback = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
    dns.has_default_gateway = adapter->FirstGatewayAddress != nullptr;
    dns.ipv4_enabled = adapter->Ipv4Enabled != 0;
    dns.ipv6_enabled = adapter->Ipv6Enabled != 0;
    dns.ipv4_metric = adapter->Ipv4Metric;
    dns.ipv6_metric = adapter->Ipv6Metric;
    dns.ipv6_interface_index = adapter->Ipv6IfIndex;
    for (auto* server = adapter->FirstDnsServerAddress; server != nullptr; server = server->Next) {
      const sockaddr* address = server->Address.lpSockaddr;
      if (address == nullptr) continue;
      ServerAddress entry;
      if (address->sa_family == AF_INET && server->Address.iSockaddrLength >= static_cast<INT>(sizeof(sockaddr_in))) {
        entry.family = AF_INET;
        std::memcpy(entry.bytes, &reinterpret_cast<const sockaddr_in*>(address)->sin_addr, 4);
      } else if (address->sa_family == AF_INET6 &&
                 server->Address.iSockaddrLength >= static_cast<INT>(sizeof(sockaddr_in6))) {
        const auto* in6 = reinterpret_cast<const sockaddr_in6*>(address);
        entry.family = AF_INET6;
        std::memcpy(entry.bytes, &in6->sin6_addr, 16);
        entry.scope = in6->sin6_scope_id;
      } else {
        continue;
      }
      dns.servers.push_back(entry);
    }
    if (adapter->DnsSuffix != nullptr && adapter->DnsSuffix[0] != L'\0') {
      const int length = WideCharToMultiByte(CP_UTF8, 0, adapter->DnsSuffix, -1, nullptr, 0, nullptr, nullptr);
      if (length > 1) {
        std::string suffix(static_cast<std::size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, adapter->DnsSuffix, -1, suffix.data(), length, nullptr, nullptr);
        suffix.resize(static_cast<std::size_t>(length) - 1);
        dns.dns_suffix = suffix;
      }
    }
    adapters->push_back(std::move(dns));
  }
  return true;
}

std::string ReadSearchList() {
  const auto read = [](const wchar_t* key) -> std::string {
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, L"SearchList", RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
        bytes < sizeof(wchar_t)) {
      return {};
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, L"SearchList", RRF_RT_REG_SZ, nullptr, value.data(), &bytes) !=
        ERROR_SUCCESS) {
      return {};
    }
    value.resize(wcsnlen(value.c_str(), value.size()));
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0,
                                           nullptr, nullptr);
    std::string text(static_cast<std::size_t>(length > 0 ? length : 0), '\0');
    if (length > 0) {
      WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), text.data(), length, nullptr,
                          nullptr);
    }
    return text;
  };
  std::string policy = read(L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient");
  if (!Trim(policy).empty()) return policy;
  return read(L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters");
}

bool DiscoverDnsConfiguration(DnsConfiguration* out, std::string* error) {
  std::vector<AdapterDns> adapters;
  if (!ReadAdapters(&adapters, error)) return false;
  *out = SelectDnsConfiguration(adapters, ReadSearchList());
  return true;
}

void ConfigureGuestResolver() noexcept {
  try {
    std::unique_lock<std::mutex> lock(g_lock);
    if (g_configured) return;
    if (!ResolveGuest(&g_resolver)) {
      g_status.mode = Mode::kNotConfigured;
      g_status.error = "libc.so has no _resolv_set_nameservers_for_net (the libc extension's resolver)";
      return;
    }
    g_configured = true;
    if (net::NativeDnsAvailable() && !g_status.forced) {
      g_status.mode = Mode::kNative;
      Log(kAndroidLogInfo,
          "dns: the guest resolver sends through Windows' resolver (DnsQueryRaw); nothing is configured in-process");
      return;
    }
    std::string error;
    if (StartFallback(lock, &error)) {
      Log(kAndroidLogInfo, "dns: no DnsQueryRaw: netid 0 uses " + Describe(g_status.applied) +
                               " through bionic's socket transport");
    } else {
      g_status.mode = Mode::kFailed;
      g_status.error = error;
      Log(kAndroidLogWarn, "dns: no DnsQueryRaw and the Windows DNS configuration failed: " + error);
    }
  } catch (const std::exception& e) {
    Log(kAndroidLogWarn, std::string("dns: configuring the guest resolver failed: ") + e.what());
  } catch (...) {
  }
}

Status GetStatus() {
  std::lock_guard<std::mutex> lock(g_lock);
  Status status = g_status;
  status.notifications = g_notifications.load(std::memory_order_relaxed);
  return status;
}

bool ForceFallbackForTesting(bool force, std::string* error) {
  std::string ignored;
  if (error == nullptr) error = &ignored;
  std::unique_lock<std::mutex> lock(g_lock);
  if (!g_configured) {
    if (!ResolveGuest(&g_resolver)) {
      *error = "libc.so has no _resolv_set_nameservers_for_net (load the libc extension first)";
      return false;
    }
    g_configured = true;
  }
  if (force) {
    g_status.forced = true;
    if (!StartFallback(lock, error)) {
      g_status.mode = Mode::kFailed;
      g_status.error = *error;
      return false;
    }
    return true;
  }
  g_status.forced = false;
  if (!net::NativeDnsAvailable()) {
    return g_status.mode == Mode::kFallback || StartFallback(lock, error);
  }
  StopWatcher(lock);
  if (platform::EnsureGuestThread() == nullptr) {
    *error = "cannot adopt the thread for guest calls";
    return false;
  }
  g_resolver.remove(kNetidUnset);
  g_status.applied = DnsConfiguration();
  g_status.mode = Mode::kNative;
  return true;
}

bool ReapplyForTesting(std::string* error) {
  std::string ignored;
  if (error == nullptr) error = &ignored;
  std::lock_guard<std::mutex> lock(g_lock);
  if (g_status.mode != Mode::kFallback) {
    *error = "the fallback is not active";
    return false;
  }
  return DiscoverAndApplyLocked(true, error);
}

}
