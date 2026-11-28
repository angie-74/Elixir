// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/host_log.h"

#include <windows.h>
#include <psapi.h>

#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>

#include "bionic/files/files.h"
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
#include "android/android.h"
#endif
#include "elixir_runtime/network_trace.h"
#include "elixir_runtime/text.h"
#include "platform/trace_clock.h"

namespace elixir::runtime {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
std::atomic<unsigned long> g_guest_lines{0};

char PriorityLetter(int priority) {
  switch (priority) {
    case 2: return 'V';
    case 3: return 'D';
    case 4: return 'I';
    case 5: return 'W';
    case 6: return 'E';
    case 7: return 'F';
    default: return '?';
  }
}

void WriteLine(const std::string& line) {
  AcquireSRWLockExclusive(&g_lock);
  std::fwrite(line.data(), 1, line.size(), stderr);
  std::fflush(stderr);
  ReleaseSRWLockExclusive(&g_lock);
}

void Emit(std::string_view prefix, std::string_view message) noexcept {
  try {
    std::string line;
    std::size_t start = 0;
    do {
      std::size_t end = message.find('\n', start);
      if (end == std::string_view::npos) end = message.size();
      line += prefix;
      line.append(message.substr(start, end - start));
      line.push_back('\n');
      start = end + 1;
    } while (start < message.size());
    WriteLine(line);
  } catch (...) {
  }
}

struct ConnectionStart {
  ULONGLONG tick;
  bionic::files::ContentCacheIoCounters cache;
  unsigned wrap_warnings = 0;
  unsigned websocket_timeouts = 0;
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  android::AssetReadFailures assets;
#endif
};

SRWLOCK g_connection_lock = SRWLOCK_INIT;
std::optional<ConnectionStart> g_connection_start;

void LogConnectionMemorySnapshot(const char* event) noexcept {
  const DWORD saved_last_error = GetLastError();
  const int saved_errno = errno;
  const long long timestamp = DiagnosticUtcMillis();
  char working_set[24] = "unavailable";
  char private_commit[24] = "unavailable";
  char host_available[24] = "unavailable";
  char host_total[24] = "unavailable";
  const auto store_mib = [](char (&field)[24], unsigned long long bytes) {
    std::snprintf(field, sizeof(field), "%llu", bytes >> 20);
  };

  PROCESS_MEMORY_COUNTERS_EX process{};
  process.cb = sizeof(process);
  if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process),
                           sizeof(process))) {
    store_mib(working_set, process.WorkingSetSize);
    store_mib(private_commit, process.PrivateUsage);
  }
  MEMORYSTATUSEX host{};
  host.dwLength = sizeof(host);
  if (GlobalMemoryStatusEx(&host)) {
    store_mib(host_available, host.ullAvailPhys);
    store_mib(host_total, host.ullTotalPhys);
  }

  char summary[256];
  std::snprintf(summary, sizeof(summary),
                "connection memory: event=%s working_set_mib=%s private_commit_mib=%s "
                "host_available_mib=%s host_total_mib=%s utc_ms=%lld",
                event, working_set, private_commit, host_available, host_total, timestamp);
  Log(summary);
  errno = saved_errno;
  SetLastError(saved_last_error);
}

void LogContentCacheSnapshot(const char* event, const ConnectionStart& start, ULONGLONG tick,
                             const bionic::files::ContentCacheIoCounters& now) noexcept {
  char summary[400];
  std::snprintf(summary, sizeof(summary),
                "content cache: %s since_accept_ms=%llu wrap_warnings=%u websocket_timeout_reports=%u "
                "open_errors=%llu read_errors=%llu write_errors=%llu rename_errors=%llu map_errors=%llu "
                "open_hits=%llu open_misses=%llu utc_ms=%lld",
                event, tick - start.tick, start.wrap_warnings, start.websocket_timeouts,
                now.open - start.cache.open, now.read - start.cache.read, now.write - start.cache.write,
                now.rename - start.cache.rename, now.map - start.cache.map,
                now.open_hits - start.cache.open_hits, now.open_misses - start.cache.open_misses, DiagnosticUtcMillis());
  Log(summary);
}

void ObserveConnectionLog(std::string_view tag, std::string_view message) noexcept {
  if (tag != "Roblox") return;

  const std::size_t category = message.substr(0, 128).find_first_of("[\r\n");
  if (category == std::string_view::npos || message[category] != '[') return;
  message.remove_prefix(category);
  while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
    message.remove_suffix(1);
  }
  if (auto http = FormatGuestHttpOutcomeTrace(message)) {
    char timed[256];
    std::snprintf(timed, sizeof(timed), "%s utc_ms=%lld", http->c_str(), DiagnosticUtcMillis());
    Log(timed);
    return;
  }
  constexpr std::string_view accepted = "[DFLog::NetworkClient] Connection accepted";
  if (message.substr(0, accepted.size()) == accepted &&
      (message.size() == accepted.size() || message[accepted.size()] == ' ')) {
    AcquireSRWLockExclusive(&g_connection_lock);
    ConnectionStart start{};
    start.tick = GetTickCount64();
    start.cache = bionic::files::GetContentCacheIoCounters();
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
    start.assets = android::GetAssetReadFailures();
#endif
    g_connection_start = start;
    ReleaseSRWLockExclusive(&g_connection_lock);
    LogConnectionMemorySnapshot("accepted");
    return;
  }

  constexpr std::string_view wrap = "[FLog::Output] Wrap-deformer fetching meshes resulted in error.";
  constexpr std::string_view ws = "[DFLog::WebSocketTraceError] ws: ";
  constexpr std::string_view timeout = " error: HttpError: Timedout";
  const bool wrap_warning = message.substr(0, wrap.size()) == wrap;
  const bool websocket_timeout = message.substr(0, ws.size()) == ws && message.size() >= timeout.size() &&
                                 message.substr(message.size() - timeout.size()) == timeout;
  if (wrap_warning || websocket_timeout) {
    std::optional<ConnectionStart> start;
    ULONGLONG tick = 0;
    bionic::files::ContentCacheIoCounters cache{};
    AcquireSRWLockExclusive(&g_connection_lock);
    if (g_connection_start) {
      if (wrap_warning) ++g_connection_start->wrap_warnings;
      if (websocket_timeout) ++g_connection_start->websocket_timeouts;
      tick = GetTickCount64();
      cache = bionic::files::GetContentCacheIoCounters();
      start = *g_connection_start;
    }
    ReleaseSRWLockExclusive(&g_connection_lock);
    if (start) LogContentCacheSnapshot(wrap_warning ? "wrap warning" : "WebSocket timeout", *start, tick, cache);
    return;
  }

  constexpr std::string_view disconnected = "[FLog::Network] Disconnect reason received: ";
  const bool lost = message == "[FLog::Network] Connection lost";
  unsigned reason = 0;
  if (!lost) {
    if (message.substr(0, disconnected.size()) != disconnected) return;
    message.remove_prefix(disconnected.size());
    if (message.empty() || message.size() > 16) return;
    const char* end = message.data() + message.size();
    const auto parsed = std::from_chars(message.data(), end, reason);
    if (parsed.ec != std::errc{} || parsed.ptr != end) return;
  }

  AcquireSRWLockExclusive(&g_connection_lock);
  const ULONGLONG tick = GetTickCount64();
  const auto cache = bionic::files::GetContentCacheIoCounters();
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  const auto assets = android::GetAssetReadFailures();
#endif
  const auto start = g_connection_start;
  g_connection_start.reset();
  ReleaseSRWLockExclusive(&g_connection_lock);

  if (lost) {
    if (start) {
      LogConnectionMemorySnapshot("lost");
      LogContentCacheSnapshot("connection lost", *start, tick, cache);
    }
    return;
  }

  char elapsed[24] = "unknown";
  char extracted_failures[24] = "unknown";
  char archive_failures[24] = "unknown";
  if (start.has_value()) {
    std::snprintf(elapsed, sizeof(elapsed), "%llu", static_cast<unsigned long long>(tick - start->tick));
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
    std::snprintf(extracted_failures, sizeof(extracted_failures), "%llu",
                  static_cast<unsigned long long>(assets.extracted - start->assets.extracted));
    std::snprintf(archive_failures, sizeof(archive_failures), "%llu",
                  static_cast<unsigned long long>(assets.archive - start->assets.archive));
#endif
  }
  char summary[192];
  std::snprintf(summary, sizeof(summary),
                "connection: disconnect reason=%u since_accept_ms=%s extracted_asset_read_failures=%s "
                "archive_asset_read_failures=%s",
                reason, elapsed, extracted_failures, archive_failures);
  LogConnectionMemorySnapshot("disconnect");
  Log(summary);
  if (start) LogContentCacheSnapshot("disconnect", *start, tick, cache);
}

}

void Log(std::string_view message) noexcept { Emit("[elixir-runtime] ", message); }

void LogError(std::string_view message) noexcept { Emit("[elixir-runtime] error: ", message); }

void ResetConnectionDiagnostics() noexcept {
  AcquireSRWLockExclusive(&g_connection_lock);
  g_connection_start.reset();
  ReleaseSRWLockExclusive(&g_connection_lock);
}

namespace {
struct LineObserver {
  GuestLineObserver observer = nullptr;
  void* context = nullptr;
};
SRWLOCK g_observer_lock = SRWLOCK_INIT;
LineObserver g_line_observers[4];
}

void AddGuestLineObserver(GuestLineObserver observer, void* context) noexcept {
  if (observer == nullptr) return;
  AcquireSRWLockExclusive(&g_observer_lock);
  for (LineObserver& slot : g_line_observers) {
    if (slot.observer == nullptr) {
      slot.observer = observer;
      slot.context = context;
      break;
    }
  }
  ReleaseSRWLockExclusive(&g_observer_lock);
}

void RemoveGuestLineObserver(GuestLineObserver observer, void* context) noexcept {
  AcquireSRWLockExclusive(&g_observer_lock);
  for (LineObserver& slot : g_line_observers) {
    if (slot.observer == observer && slot.context == context) slot = LineObserver{};
  }
  ReleaseSRWLockExclusive(&g_observer_lock);
}

void GuestLogSink(int priority, const char* tag, const char* message) noexcept {
  ++g_guest_lines;
  char prefix[160];
  std::snprintf(prefix, sizeof(prefix), "[guest %c/%.128s] ", PriorityLetter(priority), tag != nullptr ? tag : "");
  const std::string_view safe_tag = tag != nullptr ? tag : "";
  const std::string_view text = message != nullptr ? message : "";
  Emit(prefix, text);
  ObserveConnectionLog(safe_tag, text);
  AcquireSRWLockShared(&g_observer_lock);
  for (const LineObserver& slot : g_line_observers) {
    if (slot.observer != nullptr) slot.observer(slot.context, safe_tag.data(), text);
  }
  ReleaseSRWLockShared(&g_observer_lock);
}

namespace {
std::atomic<EventSink> g_event_sink{nullptr};
std::atomic<void*> g_event_context{nullptr};
}

void SetEventSink(EventSink sink, void* context) noexcept {
  g_event_context.store(context);
  g_event_sink.store(sink);
}

void EmitEvent(unsigned kind, std::string_view event, std::string_view detail) noexcept {
  const EventSink sink = g_event_sink.load();
  if (sink == nullptr) return;
  try {
    const std::string json = "{\"event\":" + JsonQuote(event) + ",\"detail\":" + JsonQuote(detail) + "}";
    sink(g_event_context.load(), kind, json.c_str());
  } catch (...) {
  }
}

void LoaderLogSink(int priority, const char* tag, const char* message) noexcept {
  char prefix[96];
  std::snprintf(prefix, sizeof(prefix), "[%.64s %c] ", tag != nullptr ? tag : "linker", PriorityLetter(priority));
  Emit(prefix, message != nullptr ? message : "");
}

unsigned long GuestLogLineCount() { return g_guest_lines.load(); }

}
