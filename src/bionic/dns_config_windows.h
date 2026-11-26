// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_DNS_CONFIG_WINDOWS_H_
#define ELIXIR_BIONIC_DNS_CONFIG_WINDOWS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace elixir::bionic::dns_config {

inline constexpr std::size_t kMaxServers = 4;
inline constexpr std::size_t kMaxSearchDomains = 6;
inline constexpr std::size_t kMaxSearchPath = 256;

struct ServerAddress {
  int family = 0;
  std::uint8_t bytes[16] = {};
  std::uint32_t scope = 0;
};

struct AdapterDns {
  bool operational = false;
  bool loopback = false;
  bool has_default_gateway = false;
  bool ipv4_enabled = false;
  bool ipv6_enabled = false;
  std::uint32_t ipv4_metric = 0;
  std::uint32_t ipv6_metric = 0;
  std::uint32_t ipv6_interface_index = 0;
  std::vector<ServerAddress> servers;
  std::string dns_suffix;
};

struct DnsConfiguration {
  std::vector<std::string> servers;
  std::string search_domains;
  bool operator==(const DnsConfiguration& other) const {
    return servers == other.servers && search_domains == other.search_domains;
  }
  bool operator!=(const DnsConfiguration& other) const { return !(*this == other); }
};

DnsConfiguration SelectDnsConfiguration(const std::vector<AdapterDns>& adapters, const std::string& search_list);

std::string FormatIpv6(const std::uint8_t bytes[16]);

bool ReadAdapters(std::vector<AdapterDns>* adapters, std::string* error);
std::string ReadSearchList();
bool DiscoverDnsConfiguration(DnsConfiguration* out, std::string* error);

enum class Mode {
  kNotConfigured,
  kNative,
  kFallback,
  kFailed,
};

struct Status {
  Mode mode = Mode::kNotConfigured;
  bool forced = false;
  DnsConfiguration applied;
  std::uint64_t applications = 0;
  std::uint64_t notifications = 0;
  std::uint64_t flushes = 0;
  std::string error;
};

void ConfigureGuestResolver() noexcept;

Status GetStatus();

bool ForceFallbackForTesting(bool force, std::string* error);
bool ReapplyForTesting(std::string* error);

}

#endif
