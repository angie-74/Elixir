// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_RELEASES_H_
#define ELIXIR_LAUNCHER_CORE_RELEASES_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/game_shortcuts.h"
#include "core/update.h"

namespace elixir::launcher {

struct ElixirRelease {
  std::string tag;
  std::string version;
  std::string name;
  std::string notes;
  std::string published;
  std::string page_url;
  std::string download_url;
  std::string sha256;
  std::uint64_t size = 0;
  bool prerelease = false;
};

std::wstring ReleasesApiUrl();

bool ParseReleases(std::string_view json_text, std::vector<ElixirRelease>* releases, std::string* error);

std::string PlainReleaseNotes(std::string_view markdown);

const ElixirRelease* NewestRelease(const std::vector<ElixirRelease>& releases);

bool ReleaseCanBeInstalled(const ElixirRelease& release);

UpdateManifest ManifestForRelease(const ElixirRelease& release);

struct ReleaseList {
  bool ok = false;
  std::vector<ElixirRelease> releases;
  std::wstring error;
};

ReleaseList FetchReleases(const HttpGetFunction& get);

inline constexpr std::int64_t kReleaseListMaxAgeSeconds = 15 * 60;

ReleaseList FetchReleasesCached(const HttpGetFunction& get, const std::wstring& cache_file, std::int64_t now,
                                bool fresh);

}

#endif
