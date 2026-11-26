// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_NET_DNS_NATIVE_H_
#define ELIXIR_BIONIC_NET_DNS_NATIVE_H_

#include <cstdint>
#include <vector>

namespace elixir::bionic {
class ExportTable;
}

namespace elixir::bionic::net {

bool NativeDnsAvailable() noexcept;

struct NativeDnsStatus {
  bool available = false;
  std::uint64_t queries = 0;
  std::uint64_t answers = 0;
  std::uint64_t tcp_retries = 0;
  std::uint64_t timeouts = 0;
  std::uint64_t failures = 0;
  std::uint64_t contexts_alive = 0;
  std::uint64_t results_received = 0;
  std::uint64_t results_freed = 0;
  unsigned long last_protocol = 0;
  long last_status = 0;
};
NativeDnsStatus GetNativeDnsStatus() noexcept;
const char* NativeDnsProtocolName(unsigned long protocol) noexcept;

int SendRawDnsQuery(const std::uint8_t* query, int query_len, std::uint8_t* answer, int answer_size,
                    int timeout_ms, int* guest_errno) noexcept;

void SetNativeDnsTestServers(const std::vector<std::uint32_t>& ipv4_servers);

namespace internal {
void RegisterDnsNativeExports(ExportTable& table);
}

}

#endif
