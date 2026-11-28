// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_SHADERS_H_
#define ELIXIR_LAUNCHER_CORE_SHADERS_H_

#include <cstddef>
#include <string>
#include <vector>

#include "shaders/shader_library.h"

namespace elixir::launcher {

struct DefaultShaderFile {
  const wchar_t* path;
  const char* text;
};

extern const DefaultShaderFile kDefaultShaderFiles[];
extern const std::size_t kDefaultShaderFileCount;
extern const wchar_t* const kDefaultShaderOrder[];
extern const std::size_t kDefaultShaderOrderCount;

inline constexpr std::size_t kMaxImportedFiles = 16;
inline constexpr std::uint64_t kMaxImportedBytes = 2ull << 20;

bool InstallDefaultShaders(const std::wstring& folder, bool overwrite, std::wstring* error);
bool ResetShadersToDefaults(const std::wstring& folder, std::wstring* error);

struct ShaderImport {
  bool ok = false;
  std::string name;
  std::wstring error;
};

ShaderImport ImportShader(const std::wstring& folder, const std::wstring& source);
bool ExportShader(const std::wstring& folder, const std::string& name, const std::wstring& destination,
                  std::wstring* error);
bool RemoveShader(const std::wstring& folder, const std::string& name, std::wstring* error);

std::wstring CheckShader(const shaders::InstalledShader& shader);

std::vector<std::wstring> TakeShaderRecoveryNote(const std::wstring& folder);

}

#endif
