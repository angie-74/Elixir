// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "shaders/shader_library.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

namespace elixir::shaders {

namespace {

using nlohmann::json;

std::string Utf8(const std::wstring& text) {
  if (text.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr,
                                       nullptr);
  std::string out(static_cast<std::size_t>(std::max(size, 0)), '\0');
  if (size > 0) {
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
  }
  return out;
}

std::wstring Wide(const std::string& text) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                       nullptr, 0);
  std::wstring out(static_cast<std::size_t>(std::max(size, 0)), L'\0');
  if (size > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
  return out;
}

bool IsFragmentFile(const std::filesystem::path& file) {
  ShaderStage stage = ShaderStage::kFragment;
  return StageOfFileName(PathToUtf8(file.filename()), &stage) && stage == ShaderStage::kFragment;
}

bool IsLink(const std::filesystem::directory_entry& entry) {
  const DWORD attributes = GetFileAttributesW(entry.path().c_str());
  return attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

void AddPass(const std::filesystem::path& file, InstalledShader* shader) {
  shader->passes.push_back(file);
  std::string source;
  if (!ReadSourceFile(file, &source)) {
    if (shader->problem.empty()) shader->problem = PathToUtf8(file.filename()) + ": the file could not be read";
    return;
  }
  ShaderPragmas pragmas;
  std::vector<Diagnostic> diagnostics;
  if (!ParseShaderPragmas(source, &pragmas, &diagnostics) && shader->problem.empty() && !diagnostics.empty()) {
    shader->problem = PathToUtf8(file.filename()) + ":" + std::to_string(diagnostics.front().line) + ": " +
                      diagnostics.front().message;
  }
  for (ShaderParameter& parameter : pragmas.parameters) {
    const bool known = std::any_of(shader->parameters.begin(), shader->parameters.end(),
                                   [&parameter](const ShaderParameter& other) { return other.name == parameter.name; });
    if (!known && shader->parameters.size() < kMaxParameters) shader->parameters.push_back(std::move(parameter));
  }
}

std::vector<InstalledShader> Scan(const std::filesystem::path& folder) {
  std::vector<InstalledShader> found;
  std::error_code error;
  for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
    if (found.size() >= kMaxInstalledShaders) break;
    const std::filesystem::directory_entry& entry = *it;
    const std::string name = PathToUtf8(entry.path().filename());
    if (!IsPlainFileName(name) || IsLink(entry)) continue;
    std::error_code ignored;
    InstalledShader shader;
    shader.name = name;
    if (entry.is_directory(ignored)) {
      std::vector<std::filesystem::path> files;
      std::error_code inner;
      for (std::filesystem::directory_iterator pass(entry.path(), inner), last; !inner && pass != last;
           pass.increment(inner)) {
        if (pass->is_regular_file(ignored) && !IsLink(*pass) && IsFragmentFile(pass->path())) {
          files.push_back(pass->path());
        }
      }
      if (files.empty()) continue;
      std::sort(files.begin(), files.end());
      if (files.size() > kMaxPassesPerShader) {
        files.resize(kMaxPassesPerShader);
        shader.problem = "a shader can have at most 8 passes; the rest are ignored";
      }
      shader.folder = true;
      for (const std::filesystem::path& file : files) AddPass(file, &shader);
    } else if (entry.is_regular_file(ignored) && IsFragmentFile(entry.path())) {
      AddPass(entry.path(), &shader);
    } else {
      continue;
    }
    found.push_back(std::move(shader));
  }
  std::sort(found.begin(), found.end(),
            [](const InstalledShader& a, const InstalledShader& b) { return a.name < b.name; });
  return found;
}

}

std::string PathToUtf8(const std::filesystem::path& path) { return Utf8(path.native()); }

std::filesystem::path PathFromUtf8(const std::string& text) { return std::filesystem::path(Wide(text)); }

bool ReadSourceFile(const std::filesystem::path& file, std::string* text) {
  text->clear();
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(file, error);
  if (error || size > kMaxSourceBytes) return false;
  std::ifstream input(file, std::ios::binary);
  if (!input) return false;
  text->resize(static_cast<std::size_t>(size));
  input.read(text->data(), static_cast<std::streamsize>(size));
  if (input.gcount() != static_cast<std::streamsize>(size)) return false;
  if (text->size() >= 3 && static_cast<unsigned char>((*text)[0]) == 0xEF &&
      static_cast<unsigned char>((*text)[1]) == 0xBB && static_cast<unsigned char>((*text)[2]) == 0xBF) {
    text->erase(0, 3);
  }
  return text->find('\0') == std::string::npos;
}

float ParameterValue(const InstalledShader& shader, const ShaderParameter& parameter) {
  const auto found = shader.values.find(parameter.name);
  return found == shader.values.end() ? parameter.initial : ClampParameter(parameter, found->second);
}

ShaderLibrary ReadShaderLibrary(const std::filesystem::path& folder) {
  ShaderLibrary library;
  std::vector<InstalledShader> found = Scan(folder);
  json saved;
  {
    std::string text;
    std::error_code error;
    const std::filesystem::path file = folder / kLibraryFileName;
    const std::uintmax_t size = std::filesystem::file_size(file, error);
    if (!error && size <= 256 * 1024) {
      std::ifstream input(file, std::ios::binary);
      text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    saved = json::parse(text, nullptr, false);
  }
  if (saved.is_object() && saved.value("version", 0) == 1) {
    if (saved.contains("enabled") && saved["enabled"].is_boolean()) library.enabled = saved["enabled"].get<bool>();
    if (saved.contains("shaders") && saved["shaders"].is_array()) {
      for (const json& item : saved["shaders"]) {
        if (!item.is_object() || !item.contains("name") || !item["name"].is_string()) continue;
        const std::string name = item["name"].get<std::string>();
        const auto match = std::find_if(found.begin(), found.end(),
                                        [&name](const InstalledShader& shader) { return shader.name == name; });
        if (match == found.end()) continue;
        InstalledShader shader = std::move(*match);
        found.erase(match);
        if (item.contains("enabled") && item["enabled"].is_boolean()) shader.enabled = item["enabled"].get<bool>();
        if (item.contains("values") && item["values"].is_object()) {
          for (const auto& value : item["values"].items()) {
            if (!value.value().is_number() || !IsShaderIdentifier(value.key())) continue;
            const double number = value.value().get<double>();
            if (std::isfinite(number)) shader.values[value.key()] = static_cast<float>(number);
          }
        }
        library.shaders.push_back(std::move(shader));
      }
    }
  }
  for (InstalledShader& shader : found) library.shaders.push_back(std::move(shader));
  return library;
}

bool WriteShaderLibrary(const std::filesystem::path& folder, const ShaderLibrary& library) {
  json root = json::object();
  root["version"] = 1;
  root["enabled"] = library.enabled;
  json shaders = json::array();
  for (const InstalledShader& shader : library.shaders) {
    json item = json::object();
    item["name"] = shader.name;
    item["enabled"] = shader.enabled;
    json values = json::object();
    for (const ShaderParameter& parameter : shader.parameters) {
      const auto found = shader.values.find(parameter.name);
      if (found != shader.values.end()) values[parameter.name] = ClampParameter(parameter, found->second);
    }
    item["values"] = std::move(values);
    shaders.push_back(std::move(item));
  }
  root["shaders"] = std::move(shaders);
  std::error_code error;
  std::filesystem::create_directories(folder, error);
  const std::filesystem::path file = folder / kLibraryFileName;
  const std::filesystem::path partial = folder / (std::wstring(kLibraryFileName) + L".part");
  {
    std::ofstream output(partial, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    const std::string text = root.dump(2) + "\n";
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) return false;
  }
  if (!MoveFileExW(partial.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(partial.c_str());
    return false;
  }
  return true;
}

std::uint64_t LibraryStamp(const std::filesystem::path& folder) {
  std::uint64_t stamp = 1469598103934665603ull;
  const auto mix = [&stamp](std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
      stamp ^= (value >> shift) & 0xFF;
      stamp *= 1099511628211ull;
    }
  };
  const auto visit = [&mix](const std::filesystem::path& where, const auto& self, int depth) -> void {
    WIN32_FIND_DATAW entry{};
    const HANDLE find = FindFirstFileExW((where / L"*").c_str(), FindExInfoBasic, &entry, FindExSearchNameMatch,
                                         nullptr, 0);
    if (find == INVALID_HANDLE_VALUE) return;
    int count = 0;
    do {
      const std::wstring name = entry.cFileName;
      if (name == L"." || name == L".." || name.front() == L'.' || ++count > 512) continue;
      if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) continue;
      for (const wchar_t c : name) mix(static_cast<std::uint64_t>(c));
      mix((static_cast<std::uint64_t>(entry.ftLastWriteTime.dwHighDateTime) << 32) |
          entry.ftLastWriteTime.dwLowDateTime);
      mix((static_cast<std::uint64_t>(entry.nFileSizeHigh) << 32) | entry.nFileSizeLow);
      if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && depth == 0) self(where / name, self, 1);
    } while (FindNextFileW(find, &entry));
    FindClose(find);
  };
  visit(folder, visit, 0);
  return stamp;
}

}
