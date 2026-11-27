// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_SHORTCUTS_H_
#define ELIXIR_LAUNCHER_CORE_SHORTCUTS_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace elixir::launcher {

inline constexpr wchar_t kElixirShortcutName[] = L"Elixir.lnk";

struct ShortcutSpec {
  std::wstring target;
  std::wstring arguments;
  std::wstring description;
  std::wstring icon;
  std::wstring app_id;
};

struct ShortcutInfo {
  std::wstring target;
  std::wstring arguments;
  std::wstring description;
  std::wstring icon;
  std::wstring app_id;
};

std::wstring DesktopDirectory();
std::wstring StartMenuProgramsDirectory();

bool CreateShortcut(const std::wstring& link_path, const ShortcutSpec& spec, std::wstring* error);
bool ReadShortcut(const std::wstring& link_path, ShortcutInfo* info);
bool SetShortcutAppearance(const std::wstring& link_path, const std::wstring& icon, const std::wstring& description);
bool ShortcutRuns(const std::wstring& link_path, const std::wstring& target);
bool RemoveShortcut(const std::wstring& link_path, const std::wstring& target);

std::wstring GameShortcutArguments(std::uint64_t place_id);
std::uint64_t PlaceOfGameShortcut(std::wstring_view arguments);

std::wstring ShortcutFileName(std::wstring_view name);

bool ParsePlaceId(std::wstring_view text, std::uint64_t* place_id);

struct GameShortcut {
  std::wstring path;
  std::wstring name;
  std::uint64_t place_id = 0;
};

std::vector<GameShortcut> ListGameShortcuts(const std::wstring& directory, const std::wstring& target);

}

#endif
