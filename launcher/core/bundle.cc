// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/bundle.h"

#include <windows.h>

#include "core/notices.h"
#include "core/util.h"
#include "resource.h"

namespace elixir::launcher {

const std::vector<BundledFile>& BundledFiles() {
  static const std::vector<BundledFile> files = {
      {IDR_GUEST_LIBC_EXTENSION, L"guest-runtime\\libelixir-extension.so"},
      {IDR_GUEST_ZLIB, L"guest-runtime\\libz.so"},
      {IDR_CONFIG_SIGNING_CERTIFICATES, L"config\\roblox_signing_certificates.json"},
      {IDR_CONFIG_COMPATIBILITY, L"config\\roblox_compatibility.json"},
      {IDR_CONFIG_DOWNLOAD_SOURCES, L"config\\roblox_download_sources.json"},
  };
  return files;
}

bool HasBundledFiles() {
  std::string bytes;
  return LoadResourceText(IDR_GUEST_LIBC_EXTENSION, &bytes);
}

bool EnsureBundledFiles(const std::wstring& bundle_dir, std::wstring* error) {
  for (const BundledFile& file : BundledFiles()) {
    std::string wanted;
    if (!LoadResourceText(file.resource, &wanted)) {
      if (error != nullptr) *error = L"Part of Elixir is missing. Reinstall Elixir.";
      return false;
    }
    const std::wstring path = JoinPath(bundle_dir, file.relative);
    std::string present;
    if (PathIsFile(path) && ReadFileBytes(path, &present, nullptr, wanted.size() + 1) && present == wanted) continue;
    if (!EnsureDirectory(ParentDirectory(path), error)) return false;
    if (!AtomicWriteFile(path, wanted, AtomicWriteBackup::kNone, error)) return false;
  }
  return true;
}

}
