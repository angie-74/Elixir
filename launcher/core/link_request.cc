// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/link_request.h"

#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iterator>

#include "runtime/roblox_launch_uri.h"

namespace elixir::launcher {
namespace {

constexpr char kOpenLinkFlagNarrow[] = "--open-link";

bool IsLinkCharacter(wchar_t c) { return c >= 0x21 && c <= 0x7e && c != L'"'; }

bool EqualsIgnoringCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    if (lower(a[i]) != lower(b[i])) return false;
  }
  return true;
}

bool OpensAppOnly(std::string_view link) {
  const std::size_t colon = link.find(':');
  if (colon == std::string_view::npos) return false;
  const std::string_view scheme = link.substr(0, colon);
  const std::string_view payload = link.substr(colon + 1);
  if (EqualsIgnoringCase(scheme, "roblox")) return payload.empty() || payload == "/" || payload == "//";
  if (!EqualsIgnoringCase(scheme, "roblox-player")) return false;
  for (std::size_t begin = 0; begin <= payload.size();) {
    const std::size_t end = payload.find('+', begin);
    const std::string_view field =
        payload.substr(begin, end == std::string_view::npos ? std::string_view::npos : end - begin);
    if (EqualsIgnoringCase(field, "launchmode:app")) return true;
    if (end == std::string_view::npos) break;
    begin = end + 1;
  }
  return false;
}

void WipeRequest(mocktail::runtime::RobloxExperienceLaunchRequest* request) {
  for (std::string* text :
       {&request->game_instance_id, &request->reserved_server_access_code, &request->call_id, &request->referral_page,
        &request->access_code, &request->link_code, &request->launch_data, &request->event_id,
        &request->game_join_context, &request->join_attempt_id, &request->join_attempt_origin, &request->iso_context,
        &request->canonical_json}) {
    Wipe(text);
  }
  *request = {};
}

template <typename Char>
void WipeAfterFlag(Char* line, std::size_t length, const Char* flag, std::size_t flag_length) {
  if (line == nullptr || length < flag_length) return;
  for (std::size_t i = 0; i + flag_length <= length; ++i) {
    if (std::memcmp(line + i, flag, flag_length * sizeof(Char)) != 0) continue;
    for (std::size_t j = i + flag_length; j < length; ++j) line[j] = static_cast<Char>(' ');
    return;
  }
}

}

void Wipe(std::string* text) {
  if (text == nullptr) return;
  SecureZeroMemory(text->data(), text->capacity());
  text->clear();
}

void Wipe(std::wstring* text) {
  if (text == nullptr) return;
  SecureZeroMemory(text->data(), text->capacity() * sizeof(wchar_t));
  text->clear();
}

LinkRequest ParseLink(std::wstring_view link) {
  LinkRequest out;
  if (link.empty() || link.size() > mocktail::runtime::kMaximumRobloxLaunchUriBytes) return out;
  std::string narrow;
  narrow.reserve(link.size());
  for (const wchar_t c : link) {
    if (!IsLinkCharacter(c)) {
      Wipe(&narrow);
      return out;
    }
    narrow.push_back(static_cast<char>(c));
  }
  if (OpensAppOnly(narrow)) {
    Wipe(&narrow);
    out.kind = LinkRequestKind::kOpenApp;
    return out;
  }
  mocktail::runtime::RobloxExperienceLaunchRequest request;
  const bool parsed = mocktail::runtime::ParseRobloxLaunchUri(narrow, &request).ok();
  Wipe(&narrow);
  if (parsed) {
    out.kind = LinkRequestKind::kJoin;
    out.join = request.canonical_json;
    out.place_id = request.place_id;
  }
  WipeRequest(&request);
  return out;
}

bool ValidateJoinRequest(std::string* join, std::int64_t* place_id) {
  if (join == nullptr) return false;
  mocktail::runtime::RobloxExperienceLaunchRequest request;
  const bool ok = !join->empty() && join->size() <= mocktail::runtime::kMaximumExperienceLaunchJsonBytes &&
                  join->find('\n') == std::string::npos &&
                  mocktail::runtime::ParseRobloxExperienceLaunchJson(*join, &request).ok();
  Wipe(join);
  if (ok) {
    *join = request.canonical_json;
    if (place_id != nullptr) *place_id = request.place_id;
  }
  WipeRequest(&request);
  return ok;
}

void WipeLinkFromCommandLine() {
  if (wchar_t* line = GetCommandLineW(); line != nullptr) {
    WipeAfterFlag(line, std::wcslen(line), kOpenLinkFlag, std::size(kOpenLinkFlag) - 1);
  }
  if (char* line = GetCommandLineA(); line != nullptr) {
    WipeAfterFlag(line, std::strlen(line), kOpenLinkFlagNarrow, std::size(kOpenLinkFlagNarrow) - 1);
  }
  if (__wargv != nullptr) {
    for (int i = 1; i < __argc; ++i) {
      if (__wargv[i - 1] != nullptr && __wargv[i] != nullptr && std::wcscmp(__wargv[i - 1], kOpenLinkFlag) == 0) {
        SecureZeroMemory(__wargv[i], std::wcslen(__wargv[i]) * sizeof(wchar_t));
      }
    }
  }
  if (__argv != nullptr) {
    for (int i = 1; i < __argc; ++i) {
      if (__argv[i - 1] != nullptr && __argv[i] != nullptr && std::strcmp(__argv[i - 1], kOpenLinkFlagNarrow) == 0) {
        SecureZeroMemory(__argv[i], std::strlen(__argv[i]));
      }
    }
  }
}

}
