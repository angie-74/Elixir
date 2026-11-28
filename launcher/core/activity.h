// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_ACTIVITY_H_
#define ELIXIR_LAUNCHER_CORE_ACTIVITY_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/game_shortcuts.h"

namespace elixir::launcher {

inline constexpr wchar_t kActivityJournalFile[] = L"journal.jsonl";
inline constexpr wchar_t kActivityStoreFile[] = L"activity.json";
inline constexpr std::size_t kMaxActivityGames = 100;
inline constexpr std::int64_t kActivitySessionQuietSeconds = 10 * 60;
inline constexpr std::int64_t kActivityDetailsMaxAgeSeconds = 24 * 60 * 60;

struct ActivityGame {
  std::uint64_t place_id = 0;
  std::uint64_t root_place_id = 0;
  std::uint64_t universe_id = 0;
  std::wstring name;
  std::wstring description;
  std::wstring creator;
  std::wstring icon_file;
  std::int64_t last_played = 0;
  std::int64_t seconds = 0;
  std::int64_t user_id = 0;
  std::string job_id;
  bool playing = false;
};

std::vector<ActivityGame> ReadActivity(const std::wstring& folder, std::int64_t now);

bool RemoveActivityGame(const std::wstring& folder, std::uint64_t place_id);
bool ClearActivity(const std::wstring& folder);

int UpdateActivityDetails(const std::wstring& folder, const HttpGetFunction& get, std::int64_t now,
                          std::size_t limit = 6);

std::wstring ActivityJoinLink(const ActivityGame& game, bool same_server);

std::wstring ActivityDuration(std::int64_t seconds);
std::wstring ActivityDay(std::int64_t when, std::int64_t now);

bool IsActivityServerId(std::string_view text);

}

#endif
