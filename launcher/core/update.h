// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_UPDATE_H_
#define ELIXIR_LAUNCHER_CORE_UPDATE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace elixir::launcher {

struct UpdateManifest {
  std::string version;
  std::string url;
  std::string sha256;
  std::string notes;
};

int CompareVersions(std::string_view a, std::string_view b);

using ProgressCallback = std::function<void(std::uint64_t done, std::uint64_t total)>;

struct HttpResult {
  bool ok = false;
  unsigned long status = 0;
  std::string body;
  std::wstring error;
};

HttpResult HttpsGet(const std::wstring& url, std::uint64_t max_bytes, const std::atomic<bool>* cancel);

struct StagedUpdate {
  bool ok = false;
  std::wstring file;
  std::string sha256;
  std::uint64_t bytes = 0;
  std::wstring error;
};

StagedUpdate DownloadAndStage(const UpdateManifest& manifest, const std::wstring& updates_dir,
                              const ProgressCallback& progress, const std::atomic<bool>* cancel);

bool VerifyAndPromote(const std::wstring& source, std::string_view expected_sha256,
                      const std::wstring& destination, std::string* measured, std::wstring* error);

std::wstring StagingFileNameForUrl(std::string_view url);

void CleanupAfterUpdate(const std::wstring& install_dir);

}

#endif
