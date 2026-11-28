// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/shaders.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#include "core/crash_logs.h"
#include "core/i18n.h"
#include "core/util.h"
#include "shaders/shader_compiler.h"

namespace elixir::launcher {

namespace {

namespace fs = std::filesystem;

const wchar_t* const kCopiedExtensions[] = {L".frag", L".glsl", L".vert", L".comp", L".png", L".txt", L".md"};

bool Fail(std::wstring* error, const std::wstring& text) {
  if (error != nullptr) *error = text;
  return false;
}

bool IsLink(const fs::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool IsCopiedFile(const fs::path& path) {
  std::wstring extension = path.extension().native();
  for (wchar_t& c : extension) c = static_cast<wchar_t>(towlower(c));
  return std::find_if(std::begin(kCopiedExtensions), std::end(kCopiedExtensions),
                      [&extension](const wchar_t* allowed) { return extension == allowed; }) !=
         std::end(kCopiedExtensions);
}

bool IsFragmentFile(const fs::path& path) {
  shaders::ShaderStage stage = shaders::ShaderStage::kVertex;
  return shaders::StageOfFileName(shaders::PathToUtf8(path.filename()), &stage) &&
         stage == shaders::ShaderStage::kFragment;
}

bool WriteTextFile(const fs::path& path, const char* text) {
  std::error_code error;
  fs::create_directories(path.parent_path(), error);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output << text;
  return static_cast<bool>(output);
}

fs::path FreeName(const fs::path& folder, const fs::path& wanted) {
  std::error_code error;
  if (!fs::exists(folder / wanted, error)) return folder / wanted;
  const std::wstring stem = wanted.stem().native();
  const std::wstring extension = wanted.extension().native();
  for (int number = 2; number <= 50; ++number) {
    const fs::path candidate = folder / (stem + L" (" + std::to_wstring(number) + L")" + extension);
    if (!fs::exists(candidate, error)) return candidate;
  }
  return {};
}

bool CopyChecked(const fs::path& from, const fs::path& to, std::uint64_t* total) {
  std::error_code error;
  const std::uintmax_t size = fs::file_size(from, error);
  if (error || size > shaders::kMaxSourceBytes * 4 || IsLink(from)) return false;
  *total += size;
  if (*total > kMaxImportedBytes) return false;
  fs::create_directories(to.parent_path(), error);
  return CopyFileW(from.c_str(), to.c_str(), TRUE) != 0;
}

}

bool InstallDefaultShaders(const std::wstring& folder, bool overwrite, std::wstring* error) {
  if (folder.empty()) return Fail(error, T(L"The shaders folder couldn’t be made."));
  const fs::path root(folder);
  std::error_code ignored;
  fs::create_directories(root, ignored);
  for (std::size_t i = 0; i < kDefaultShaderFileCount; ++i) {
    const fs::path target = root / kDefaultShaderFiles[i].path;
    if (!overwrite && fs::exists(target, ignored)) continue;
    if (!WriteTextFile(target, kDefaultShaderFiles[i].text)) {
      return Fail(error, T(L"The shaders folder couldn’t be made."));
    }
  }
  if (overwrite || !fs::exists(root / shaders::kLibraryFileName, ignored)) {
    shaders::ShaderLibrary scanned = shaders::ReadShaderLibrary(root);
    shaders::ShaderLibrary ordered;
    for (std::size_t i = 0; i < kDefaultShaderOrderCount; ++i) {
      const std::string name = WideToUtf8(kDefaultShaderOrder[i]);
      for (shaders::InstalledShader& shader : scanned.shaders) {
        if (shader.name != name) continue;
        shader.enabled = false;
        shader.values.clear();
        ordered.shaders.push_back(std::move(shader));
        shader.name.clear();
      }
    }
    for (shaders::InstalledShader& shader : scanned.shaders) {
      if (shader.name.empty()) continue;
      if (overwrite) {
        shader.enabled = false;
        shader.values.clear();
      }
      ordered.shaders.push_back(std::move(shader));
    }
    ordered.enabled = overwrite ? false : scanned.enabled;
    if (!shaders::WriteShaderLibrary(root, ordered)) return Fail(error, T(L"The shaders folder couldn’t be made."));
  }
  return true;
}

bool ResetShadersToDefaults(const std::wstring& folder, std::wstring* error) {
  return InstallDefaultShaders(folder, true, error);
}

ShaderImport ImportShader(const std::wstring& folder, const std::wstring& source) {
  ShaderImport result;
  const auto fail = [&result](const std::wstring& text) {
    result.error = text;
    return result;
  };
  const fs::path from(source);
  const fs::path root(folder);
  std::error_code error;
  if (folder.empty() || source.empty() || IsLink(from)) return fail(T(L"That shader couldn’t be read."));
  fs::create_directories(root, error);
  if (!shaders::IsPlainFileName(shaders::PathToUtf8(from.filename()))) {
    return fail(T(L"That shader couldn’t be read."));
  }
  std::uint64_t total = 0;
  fs::path target;
  if (fs::is_directory(from, error)) {
    std::vector<fs::path> files;
    bool has_pass = false;
    for (fs::directory_iterator it(from, error), end; !error && it != end; it.increment(error)) {
      std::error_code ignored;
      if (!it->is_regular_file(ignored) || IsLink(it->path()) || !IsCopiedFile(it->path())) continue;
      files.push_back(it->path());
      has_pass = has_pass || IsFragmentFile(it->path());
      if (files.size() > kMaxImportedFiles) return fail(T(L"That folder has too many files for a shader."));
    }
    if (!has_pass) return fail(T(L"That folder has no .frag or .glsl file."));
    target = FreeName(root, from.filename());
    if (target.empty()) return fail(T(L"A shader with that name is already installed."));
    for (const fs::path& file : files) {
      if (!CopyChecked(file, target / file.filename(), &total)) {
        fs::remove_all(target, error);
        return fail(T(L"That shader is too large or couldn’t be copied."));
      }
    }
  } else {
    if (!IsFragmentFile(from)) return fail(T(L"Choose a .frag or .glsl file, or a folder of them."));
    target = FreeName(root, from.filename());
    if (target.empty()) return fail(T(L"A shader with that name is already installed."));
    if (!CopyChecked(from, target, &total)) {
      DeleteFileW(target.c_str());
      return fail(T(L"That shader is too large or couldn’t be copied."));
    }
  }
  result.name = shaders::PathToUtf8(target.filename());
  shaders::ShaderLibrary library = shaders::ReadShaderLibrary(root);
  shaders::WriteShaderLibrary(root, library);
  result.ok = true;
  return result;
}

bool ExportShader(const std::wstring& folder, const std::string& name, const std::wstring& destination,
                  std::wstring* error) {
  if (!shaders::IsPlainFileName(name) || destination.empty()) return Fail(error, T(L"That shader couldn’t be exported."));
  const fs::path from = fs::path(folder) / shaders::PathFromUtf8(name);
  std::error_code ignored;
  if (IsLink(from)) return Fail(error, T(L"That shader couldn’t be exported."));
  const fs::path target = FreeName(fs::path(destination), from.filename());
  if (target.empty()) return Fail(error, T(L"That shader couldn’t be exported."));
  if (fs::is_directory(from, ignored)) {
    std::error_code error_code;
    fs::create_directories(target, error_code);
    for (fs::directory_iterator it(from, error_code), end; !error_code && it != end; it.increment(error_code)) {
      if (!it->is_regular_file(ignored) || IsLink(it->path()) || !IsCopiedFile(it->path())) continue;
      if (!CopyFileW(it->path().c_str(), (target / it->path().filename()).c_str(), TRUE)) {
        return Fail(error, T(L"That shader couldn’t be exported."));
      }
    }
    return true;
  }
  if (!CopyFileW(from.c_str(), target.c_str(), TRUE)) return Fail(error, T(L"That shader couldn’t be exported."));
  return true;
}

bool RemoveShader(const std::wstring& folder, const std::string& name, std::wstring* error) {
  if (!shaders::IsPlainFileName(name)) return Fail(error, T(L"That shader couldn’t be removed."));
  const fs::path target = fs::path(folder) / shaders::PathFromUtf8(name);
  std::error_code ignored;
  if (!fs::exists(target, ignored) || IsLink(target)) return Fail(error, T(L"That shader couldn’t be removed."));
  if (!RecycleFiles({target.native()}, error)) return false;
  shaders::ShaderLibrary library = shaders::ReadShaderLibrary(fs::path(folder));
  shaders::WriteShaderLibrary(fs::path(folder), library);
  return true;
}

std::wstring CheckShader(const shaders::InstalledShader& shader) {
  if (!shader.problem.empty()) return Utf8ToWide(shader.problem);
  for (const fs::path& file : shader.passes) {
    const std::string name = shaders::PathToUtf8(file.filename());
    std::string source;
    if (!shaders::ReadSourceFile(file, &source)) return Utf8ToWide(name) + L": " + T(L"That shader couldn’t be read.");
    const shaders::CompiledShader compiled = shaders::CompileShader(source, name, shaders::CompileOptions{});
    if (!compiled.ok) return Utf8ToWide(shaders::DescribeDiagnostics(compiled.diagnostics, name));
  }
  return {};
}

std::vector<std::wstring> TakeShaderRecoveryNote(const std::wstring& folder) {
  std::vector<std::wstring> names;
  const fs::path note = fs::path(folder) / L".cache" / L"recovered.txt";
  {
    std::ifstream input(note, std::ios::binary);
    if (!input) return names;
    std::string line;
    while (std::getline(input, line) && names.size() < 16) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (shaders::IsPlainFileName(line)) names.push_back(Utf8ToWide(line));
    }
  }
  DeleteFileW(note.c_str());
  return names;
}

}
