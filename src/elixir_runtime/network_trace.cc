// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "elixir_runtime/network_trace.h"

#include <charconv>
#include <cstdint>

namespace elixir::runtime {
namespace {

bool Consume(std::string_view* input, std::string_view prefix) {
  if (input->substr(0, prefix.size()) != prefix) return false;
  input->remove_prefix(prefix.size());
  return true;
}

std::string_view TakeUntil(std::string_view* input, std::string_view delimiter) {
  const auto end = input->find(delimiter);
  if (end == std::string_view::npos) return {};
  const auto result = input->substr(0, end);
  input->remove_prefix(end + delimiter.size());
  return result;
}

std::optional<std::uint64_t> ParseUnsigned(std::string_view token) {
  if (token.empty()) return std::nullopt;
  std::uint64_t result = 0;
  const auto* end = token.data() + token.size();
  const auto parsed = std::from_chars(token.data(), end, result);
  if (parsed.ec != std::errc{} || parsed.ptr != end) return std::nullopt;
  return result;
}

bool IsDecimalMilliseconds(std::string_view token) {
  if (token.size() > 24) return false;
  const auto dot = token.find('.');
  if (!ParseUnsigned(token.substr(0, dot))) return false;
  return dot == std::string_view::npos || ParseUnsigned(token.substr(dot + 1)).has_value();
}

std::optional<std::string> FormatResponse(std::string_view body) {
  const auto request = TakeUntil(&body, ") time:");
  if (request.empty()) return std::nullopt;
  const auto elapsed = TakeUntil(&body, "ms (net:");
  if (elapsed.empty() || TakeUntil(&body, ")").empty()) return std::nullopt;
  Consume(&body, " ");

  const bool has_status = Consume(&body, "status:");
  if (!has_status && !Consume(&body, "error:")) return std::nullopt;
  const auto result = ParseUnsigned(body.substr(0, body.find(' ')));
  if (!result || (has_status && (*result < 100 || *result > 599))) return std::nullopt;

  std::string out = "network event=http_response";
  if (const auto id = ParseUnsigned(request.substr(0, request.find(' ')))) {
    out += " request_id=" + std::to_string(*id);
  }
  if (IsDecimalMilliseconds(elapsed)) {
    out += " elapsed_ms=";
    out.append(elapsed);
  }
  out += has_status ? " http_status=" : " transport_error=";
  out += std::to_string(*result);
  return out;
}

std::optional<std::string> FormatConnectFailure(std::string_view body) {
  const auto destination = TakeUntil(&body, " port ");
  if (destination.empty() || destination.find_first_of(" \t\r\n") != std::string_view::npos) return std::nullopt;
  const auto port = ParseUnsigned(TakeUntil(&body, " after "));
  if (!port || *port == 0 || *port > 65535) return std::nullopt;
  const auto elapsed = ParseUnsigned(TakeUntil(&body, " ms:"));
  if (!elapsed) return std::nullopt;
  return "network event=tcp_connect_failure elapsed_ms=" + std::to_string(*elapsed);
}

}

std::optional<std::string> FormatGuestHttpOutcomeTrace(std::string_view body) noexcept {
  try {
    if (!Consume(&body, "[DFLog::HttpTraceError] ") && !Consume(&body, "[DFLog::HttpTrace] ")) return std::nullopt;
    if (Consume(&body, "HttpResponse(#")) return FormatResponse(body);
    if (Consume(&body, "CURLINFO_OS_ERRNO: ")) {
      if (const auto code = ParseUnsigned(body.substr(0, body.find(' ')))) {
        return "network event=transport_os_error os_errno=" + std::to_string(*code);
      }
    }
    if (Consume(&body, "CURLOPT_ERRORBUFFER: Failed to connect to ")) return FormatConnectFailure(body);
  } catch (...) {
  }
  return std::nullopt;
}

}
