// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/dns_native.h"

#include "bionic/net/net_internal.h"

#include <windns.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

namespace elixir::bionic::net {
namespace {

using QueryRawFn = DNS_STATUS(WINAPI*)(DNS_QUERY_RAW_REQUEST*, DNS_QUERY_RAW_CANCEL*);
using CancelQueryRawFn = DNS_STATUS(WINAPI*)(DNS_QUERY_RAW_CANCEL*);
using QueryRawResultFreeFn = VOID(WINAPI*)(DNS_QUERY_RAW_RESULT*);

struct RawApi {
  QueryRawFn query = nullptr;
  CancelQueryRawFn cancel = nullptr;
  QueryRawResultFreeFn free_result = nullptr;
};

const RawApi* Api() noexcept {
  static const RawApi* const api = []() -> const RawApi* {
    HMODULE dnsapi = LoadLibraryExW(L"dnsapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dnsapi == nullptr) return nullptr;
    static RawApi resolved;
    resolved.query = reinterpret_cast<QueryRawFn>(GetProcAddress(dnsapi, "DnsQueryRaw"));
    resolved.cancel = reinterpret_cast<CancelQueryRawFn>(GetProcAddress(dnsapi, "DnsCancelQueryRaw"));
    resolved.free_result = reinterpret_cast<QueryRawResultFreeFn>(GetProcAddress(dnsapi, "DnsQueryRawResultFree"));
    if (resolved.query == nullptr || resolved.cancel == nullptr || resolved.free_result == nullptr) return nullptr;
    return &resolved;
  }();
  return api;
}

constexpr int kHeaderSize = 12;
constexpr int kMaxMessage = 65535;
constexpr int kUdpFramingLimit = 512;
constexpr DWORD kCancelGraceMs = 2000;

std::atomic<std::uint64_t> g_queries{0};
std::atomic<std::uint64_t> g_answers{0};
std::atomic<std::uint64_t> g_tcp_retries{0};
std::atomic<std::uint64_t> g_timeouts{0};
std::atomic<std::uint64_t> g_failures{0};
std::atomic<std::uint64_t> g_contexts_alive{0};
std::atomic<std::uint64_t> g_results_received{0};
std::atomic<std::uint64_t> g_results_freed{0};
std::atomic<unsigned long> g_last_protocol{0};
std::atomic<long> g_last_status{0};

SRWLOCK g_test_servers_lock = SRWLOCK_INIT;
std::vector<std::uint32_t>* g_test_servers = nullptr;

struct QueryContext {
  SRWLOCK lock = SRWLOCK_INIT;
  HANDLE done = nullptr;
  DNS_QUERY_RAW_CANCEL cancel{};
  DNS_QUERY_RAW_RESULT* result = nullptr;
  bool completed = false;
  bool abandoned = false;
  QueryRawResultFreeFn free_result = nullptr;
  std::atomic<int> references{2};

  QueryContext() noexcept { g_contexts_alive.fetch_add(1, std::memory_order_relaxed); }
  ~QueryContext() { g_contexts_alive.fetch_sub(1, std::memory_order_relaxed); }

  void Release() noexcept {
    if (references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      if (done != nullptr) CloseHandle(done);
      delete this;
    }
  }
};

void FreeResult(QueryRawResultFreeFn free_result, DNS_QUERY_RAW_RESULT* result) noexcept {
  free_result(result);
  g_results_freed.fetch_add(1, std::memory_order_relaxed);
}

VOID CALLBACK OnQueryComplete(VOID* context, DNS_QUERY_RAW_RESULT* result) noexcept {
  auto* query = static_cast<QueryContext*>(context);
  if (result != nullptr) g_results_received.fetch_add(1, std::memory_order_relaxed);
  bool keep = false;
  AcquireSRWLockExclusive(&query->lock);
  query->completed = true;
  if (!query->abandoned) {
    query->result = result;
    keep = true;
  }
  ReleaseSRWLockExclusive(&query->lock);
  if (!keep && result != nullptr) FreeResult(query->free_result, result);
  SetEvent(query->done);
  query->Release();
}

int ErrnoFromDnsStatus(DNS_STATUS status) noexcept {
  switch (status) {
    case ERROR_TIMEOUT:
    case WSAETIMEDOUT:
    case ERROR_CANCELLED:
      return guest::kETIMEDOUT;
    case WSAECONNRESET:
    case WSAECONNREFUSED:
    case ERROR_CONNECTION_REFUSED:
      return guest::kECONNREFUSED;
    case WSAENETUNREACH:
    case WSAEHOSTUNREACH:
    case WSAENETDOWN:
    case ERROR_NETWORK_UNREACHABLE:
    case ERROR_HOST_UNREACHABLE:
    case DNS_ERROR_NO_DNS_SERVERS:
      return guest::kENETUNREACH;
    case ERROR_INVALID_PARAMETER:
      return guest::kEINVAL;
    case ERROR_OUTOFMEMORY:
    case ERROR_NOT_ENOUGH_MEMORY:
      return guest::kENOMEM;
    default:
      return guest::kECONNREFUSED;
  }
}

struct Outcome {
  int error = 0;
  std::vector<std::uint8_t> response;
};

std::vector<DNS_CUSTOM_SERVER> TestServers() {
  std::vector<std::uint32_t> addresses;
  AcquireSRWLockShared(&g_test_servers_lock);
  try {
    if (g_test_servers != nullptr) addresses = *g_test_servers;
  } catch (...) {
    ReleaseSRWLockShared(&g_test_servers_lock);
    throw;
  }
  ReleaseSRWLockShared(&g_test_servers_lock);
  std::vector<DNS_CUSTOM_SERVER> servers;
  servers.reserve(addresses.size());
  for (const std::uint32_t address : addresses) {
    DNS_CUSTOM_SERVER server{};
    server.dwServerType = DNS_CUSTOM_SERVER_TYPE_UDP;
    server.ServerAddr.Ipv4.sin_family = AF_INET;
    server.ServerAddr.Ipv4.sin_port = 0;
    server.ServerAddr.Ipv4.sin_addr.S_un.S_addr = address;
    servers.push_back(server);
  }
  return servers;
}

Outcome QueryOnce(const RawApi& api, const std::uint8_t* query, int query_len, bool tcp, DWORD wait_ms) {
  Outcome outcome;
  std::vector<std::uint8_t> framed;
  if (tcp) {
    framed.reserve(static_cast<std::size_t>(query_len) + 2);
    framed.push_back(static_cast<std::uint8_t>(query_len >> 8));
    framed.push_back(static_cast<std::uint8_t>(query_len & 0xff));
    framed.insert(framed.end(), query, query + query_len);
  }
  std::vector<DNS_CUSTOM_SERVER> servers = TestServers();

  auto* context = new (std::nothrow) QueryContext();
  if (context == nullptr) {
    outcome.error = guest::kENOMEM;
    return outcome;
  }
  context->free_result = api.free_result;
  context->done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (context->done == nullptr) {
    delete context;
    outcome.error = guest::kENOMEM;
    return outcome;
  }

  DNS_QUERY_RAW_REQUEST request{};
  request.version = DNS_QUERY_RAW_REQUEST_VERSION1;
  request.resultsVersion = DNS_QUERY_RAW_RESULTS_VERSION1;
  request.dnsQueryRawSize = tcp ? static_cast<ULONG>(framed.size()) : static_cast<ULONG>(query_len);
  request.dnsQueryRaw = tcp ? framed.data() : const_cast<BYTE*>(query);
  request.queryOptions = DNS_QUERY_STANDARD;
  request.interfaceIndex = 0;
  request.queryCompletionCallback = &OnQueryComplete;
  request.queryContext = context;
  request.queryRawOptions = 0;
  request.customServersSize = static_cast<ULONG>(servers.size());
  request.customServers = servers.empty() ? nullptr : servers.data();
  request.protocol = tcp ? DNS_PROTOCOL_TCP : DNS_PROTOCOL_UDP;

  g_queries.fetch_add(1, std::memory_order_relaxed);
  const DNS_STATUS started = api.query(&request, &context->cancel);
  if (started != DNS_REQUEST_PENDING) {
    context->Release();
    context->Release();
    g_last_status.store(started, std::memory_order_relaxed);
    outcome.error = ErrnoFromDnsStatus(started);
    return outcome;
  }

  bool timed_out = false;
  if (WaitForSingleObject(context->done, wait_ms) != WAIT_OBJECT_0) {
    timed_out = true;
    api.cancel(&context->cancel);
    WaitForSingleObject(context->done, kCancelGraceMs);
  }
  DNS_QUERY_RAW_RESULT* result = nullptr;
  AcquireSRWLockExclusive(&context->lock);
  result = context->result;
  context->result = nullptr;
  if (!context->completed) context->abandoned = true;
  ReleaseSRWLockExclusive(&context->lock);

  if (result != nullptr) {
    g_last_status.store(result->queryStatus, std::memory_order_relaxed);
    const BYTE* bytes = result->queryRawResponse;
    std::size_t size = bytes != nullptr ? result->queryRawResponseSize : 0;
    if (tcp && size >= 2) {
      const std::size_t length = (static_cast<std::size_t>(bytes[0]) << 8) | bytes[1];
      bytes += 2;
      size = std::min(size - 2, length);
    }
    if (size >= static_cast<std::size_t>(kHeaderSize)) {
      try {
        outcome.response.assign(bytes, bytes + size);
      } catch (...) {
        FreeResult(api.free_result, result);
        context->Release();
        throw;
      }
      g_last_protocol.store(result->protocol, std::memory_order_relaxed);
    } else {
      outcome.error = timed_out ? guest::kETIMEDOUT : ErrnoFromDnsStatus(result->queryStatus);
    }
    FreeResult(api.free_result, result);
  } else {
    outcome.error = guest::kETIMEDOUT;
  }
  if (timed_out && outcome.response.empty()) g_timeouts.fetch_add(1, std::memory_order_relaxed);
  context->Release();
  return outcome;
}

int SendRawImpl(const std::uint8_t* query, int query_len, std::uint8_t* answer, int answer_size, int timeout_ms,
                int* error) {
  const RawApi* api = Api();
  if (api == nullptr) {
    *error = guest::kENOSYS;
    return -1;
  }
  if (query == nullptr || query_len < kHeaderSize || query_len > kMaxMessage || answer == nullptr ||
      answer_size < kHeaderSize) {
    *error = guest::kEINVAL;
    return -1;
  }
  if (timeout_ms <= 0) {
    *error = guest::kETIMEDOUT;
    return -1;
  }
  const std::int64_t deadline_ns = internal::MonotonicNs() + static_cast<std::int64_t>(timeout_ms) * 1000000;
  bool tcp = query_len > kUdpFramingLimit;
  for (;;) {
    const std::int64_t remaining_ns = deadline_ns - internal::MonotonicNs();
    if (remaining_ns <= 0) {
      *error = guest::kETIMEDOUT;
      g_failures.fetch_add(1, std::memory_order_relaxed);
      return -1;
    }
    const DWORD wait_ms = static_cast<DWORD>(std::min<std::int64_t>((remaining_ns + 999999) / 1000000, 0x7ffffffe));
    Outcome outcome = QueryOnce(*api, query, query_len, tcp, wait_ms);
    if (outcome.response.empty()) {
      *error = outcome.error != 0 ? outcome.error : guest::kECONNREFUSED;
      g_failures.fetch_add(1, std::memory_order_relaxed);
      return -1;
    }
    std::vector<std::uint8_t>& response = outcome.response;
    const bool truncated = (response[2] & 0x02) != 0;
    if (truncated && !tcp) {
      tcp = true;
      g_tcp_retries.fetch_add(1, std::memory_order_relaxed);
      continue;
    }
    response[0] = query[0];
    response[1] = query[1];
    g_answers.fetch_add(1, std::memory_order_relaxed);
    const std::size_t size = response.size();
    if (size > static_cast<std::size_t>(answer_size)) {
      std::memcpy(answer, response.data(), static_cast<std::size_t>(answer_size));
      *error = guest::kEMSGSIZE;
      return -1;
    }
    std::memcpy(answer, response.data(), size);
    return static_cast<int>(size);
  }
}

}

bool NativeDnsAvailable() noexcept { return Api() != nullptr; }

NativeDnsStatus GetNativeDnsStatus() noexcept {
  NativeDnsStatus status;
  status.available = NativeDnsAvailable();
  status.queries = g_queries.load(std::memory_order_relaxed);
  status.answers = g_answers.load(std::memory_order_relaxed);
  status.tcp_retries = g_tcp_retries.load(std::memory_order_relaxed);
  status.timeouts = g_timeouts.load(std::memory_order_relaxed);
  status.failures = g_failures.load(std::memory_order_relaxed);
  status.contexts_alive = g_contexts_alive.load(std::memory_order_relaxed);
  status.results_received = g_results_received.load(std::memory_order_relaxed);
  status.results_freed = g_results_freed.load(std::memory_order_relaxed);
  status.last_protocol = g_last_protocol.load(std::memory_order_relaxed);
  status.last_status = g_last_status.load(std::memory_order_relaxed);
  return status;
}

const char* NativeDnsProtocolName(unsigned long protocol) noexcept {
  switch (protocol) {
    case DNS_PROTOCOL_UDP: return "UDP";
    case DNS_PROTOCOL_TCP: return "TCP";
    case DNS_PROTOCOL_DOH: return "DNS over HTTPS";
    case DNS_PROTOCOL_DOT: return "DNS over TLS";
    case DNS_PROTOCOL_NO_WIRE: return "no network traffic";
    default: return "none";
  }
}

int SendRawDnsQuery(const std::uint8_t* query, int query_len, std::uint8_t* answer, int answer_size, int timeout_ms,
                    int* guest_errno) noexcept {
  int error = 0;
  int result = -1;
  try {
    result = SendRawImpl(query, query_len, answer, answer_size, timeout_ms, &error);
  } catch (const std::bad_alloc&) {
    error = guest::kENOMEM;
    result = -1;
  } catch (...) {
    error = guest::kEIO;
    result = -1;
  }
  if (guest_errno != nullptr) *guest_errno = result < 0 ? error : 0;
  return result;
}

void SetNativeDnsTestServers(const std::vector<std::uint32_t>& ipv4_servers) {
  auto* replacement = ipv4_servers.empty() ? nullptr : new std::vector<std::uint32_t>(ipv4_servers);
  AcquireSRWLockExclusive(&g_test_servers_lock);
  std::vector<std::uint32_t>* previous = g_test_servers;
  g_test_servers = replacement;
  ReleaseSRWLockExclusive(&g_test_servers_lock);
  delete previous;
}

}

namespace elixir::bionic::net::internal {

ELIXIR_BIONIC_FN(int, __elixir_dns_send_raw)(const std::uint8_t* query, int query_len, std::uint8_t* answer,
                                             int answer_size, int timeout_ms) noexcept {
  int error = 0;
  const int result = SendRawDnsQuery(query, query_len, answer, answer_size, timeout_ms, &error);
  if (result < 0) SetGuestErrno(error);
  return result;
}

void RegisterDnsNativeExports(ExportTable& table) {
  table.Add(kLibc, "__elixir_dns_send_raw", reinterpret_cast<void*>(&elixir_bionic___elixir_dns_send_raw));
  HostLog(4, "elixir",
          NativeDnsAvailable()
              ? "dns: DnsQueryRaw is available: raw DNS queries (res_query, res_send) use Windows' resolver"
              : "dns: DnsQueryRaw is not available on this Windows build: raw DNS queries use bionic's socket "
                "transport");
}

}
