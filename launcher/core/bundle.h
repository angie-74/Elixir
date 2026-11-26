// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_BUNDLE_H_
#define ELIXIR_LAUNCHER_CORE_BUNDLE_H_

#include <string>
#include <vector>

namespace elixir::launcher {

struct BundledFile {
  int resource = 0;
  const wchar_t* relative = L"";
};

const std::vector<BundledFile>& BundledFiles();

bool HasBundledFiles();

bool EnsureBundledFiles(const std::wstring& bundle_dir, std::wstring* error);

}

#endif
