// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_MODS_H_
#define ELIXIR_LAUNCHER_CORE_MODS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/paths.h"

namespace elixir::launcher {

inline constexpr wchar_t kCustomFontFile[] = L"CustomFont.ttf";
inline constexpr char kCustomFontAsset[] = "rbxasset://fonts/CustomFont.ttf";
inline constexpr std::uint64_t kMaxScannedModFiles = 20000;
inline constexpr std::size_t kModExamples = 5;
inline constexpr std::uint64_t kMaxFontBytes = 64ull << 20;

struct ModsSummary {
  bool folder_exists = false;
  std::uint64_t used_files = 0;
  std::uint64_t used_bytes = 0;
  std::uint64_t unused_files = 0;
  std::vector<std::wstring> unused_examples;
  bool truncated = false;
  bool custom_font = false;
};

std::wstring ModsDirectory(const LauncherPaths& paths);
ModsSummary ScanMods(const std::wstring& directory);

bool LooksLikeFontFile(const std::wstring& path);

struct CustomFontResult {
  bool ok = false;
  std::wstring error;
  std::size_t families = 0;
};

CustomFontResult ApplyCustomFont(const std::wstring& mods_directory, const std::wstring& client_assets_directory,
                                 const std::wstring& font_file);
bool RemoveCustomFont(const std::wstring& mods_directory, std::wstring* error);

}

#endif
