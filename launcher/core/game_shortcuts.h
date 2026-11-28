// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_GAME_SHORTCUTS_H_
#define ELIXIR_LAUNCHER_CORE_GAME_SHORTCUTS_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/update.h"

namespace elixir::launcher {

using HttpGetFunction = std::function<HttpResult(const std::wstring& url, std::uint64_t max_bytes)>;

struct GameDetails {
  std::uint64_t universe_id = 0;
  std::wstring name;
  std::string icon_url;
  std::uint64_t root_place_id = 0;
  std::wstring description;
  std::wstring creator;
};

bool IsRobloxImageUrl(std::string_view url);

bool FetchGameDetails(std::uint64_t place_id, const HttpGetFunction& get, GameDetails* details);

bool WriteIconFile(std::string_view image, const std::wstring& ico_path, std::wstring* error);

struct GameShortcutPlaces {
  std::wstring folder;
  std::wstring program;
  std::wstring state_file;
  std::wstring icons_dir;
};

struct GameShortcutEntry {
  std::wstring path;
  std::wstring name;
  std::wstring title;
  std::wstring icon;
  std::uint64_t place_id = 0;
  bool automatic_name = false;
};

std::vector<GameShortcutEntry> ListGameShortcutEntries(const GameShortcutPlaces& places);

struct GameShortcutChange {
  bool ok = false;
  std::wstring path;
  std::wstring error;
};

GameShortcutChange AddGameShortcut(const GameShortcutPlaces& places, std::uint64_t place_id,
                                   const HttpGetFunction& get);

GameShortcutChange RenameGameShortcut(const GameShortcutPlaces& places, const std::wstring& path,
                                      std::wstring_view name);

bool DeleteGameShortcut(const GameShortcutPlaces& places, const std::wstring& path);

int UpdateGameShortcuts(const GameShortcutPlaces& places, const HttpGetFunction& get);

}

#endif
