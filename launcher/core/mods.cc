// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/mods.h"

#include <windows.h>

#include <algorithm>
#include <utility>

#include "core/json.h"
#include "core/util.h"
#include "runtime/mod_paths.h"

namespace elixir::launcher {

namespace {

std::wstring FontsDirectory(const std::wstring& root) { return JoinPath(JoinPath(root, L"content"), L"fonts"); }
std::wstring FamiliesDirectory(const std::wstring& root) { return JoinPath(FontsDirectory(root), L"families"); }

std::vector<std::wstring> JsonFilesIn(const std::wstring& directory) {
  std::vector<std::wstring> files;
  WIN32_FIND_DATAW entry{};
  const HANDLE find = FindFirstFileExW(JoinPath(directory, L"*.json").c_str(), FindExInfoBasic, &entry,
                                       FindExSearchNameMatch, nullptr, 0);
  if (find == INVALID_HANDLE_VALUE) return files;
  do {
    if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) continue;
    files.push_back(entry.cFileName);
  } while (FindNextFileW(find, &entry));
  FindClose(find);
  std::sort(files.begin(), files.end());
  return files;
}

std::optional<Json> WithFont(const Json& family) {
  if (!family.is_object()) return std::nullopt;
  const Json* faces = family.Find("faces");
  if (faces == nullptr || !faces->is_array() || faces->items().empty()) return std::nullopt;
  Json out = Json::MakeObject();
  for (const Json::Member& member : family.members()) {
    if (member.first != "faces") {
      out.Set(member.first, member.second);
      continue;
    }
    Json replaced = Json::MakeArray();
    for (const Json& face : faces->items()) {
      if (!face.is_object()) return std::nullopt;
      Json copy = Json::MakeObject();
      for (const Json::Member& field : face.members()) copy.Set(field.first, field.second);
      copy.Set("assetId", Json(kCustomFontAsset));
      replaced.Push(std::move(copy));
    }
    out.Set("faces", std::move(replaced));
  }
  return out;
}

}

std::wstring ModsDirectory(const LauncherPaths& paths) { return JoinPath(paths.data_root, mods::kModsFolderName); }

ModsSummary ScanMods(const std::wstring& directory) {
  ModsSummary summary;
  summary.folder_exists = PathIsDirectory(directory);
  if (!summary.folder_exists) return summary;
  summary.custom_font = PathIsFile(JoinPath(FontsDirectory(directory), kCustomFontFile));
  std::vector<std::wstring> pending{std::wstring()};
  std::uint64_t seen = 0;
  while (!pending.empty() && !summary.truncated) {
    const std::wstring relative = std::move(pending.back());
    pending.pop_back();
    const std::wstring folder = relative.empty() ? directory : JoinPath(directory, relative);
    WIN32_FIND_DATAW entry{};
    const HANDLE find = FindFirstFileExW(JoinPath(folder, L"*").c_str(), FindExInfoBasic, &entry,
                                         FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (find == INVALID_HANDLE_VALUE) continue;
    do {
      const std::wstring name = entry.cFileName;
      if (name == L"." || name == L"..") continue;
      if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) continue;
      const std::wstring child = relative.empty() ? name : relative + L"\\" + name;
      if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        pending.push_back(child);
        continue;
      }
      if (++seen > kMaxScannedModFiles) {
        summary.truncated = true;
        break;
      }
      std::string path = WideToUtf8(child);
      std::replace(path.begin(), path.end(), '\\', '/');
      if (mods::IsReplaceable(path)) {
        ++summary.used_files;
        summary.used_bytes += (static_cast<std::uint64_t>(entry.nFileSizeHigh) << 32) | entry.nFileSizeLow;
      } else {
        ++summary.unused_files;
        if (summary.unused_examples.size() < kModExamples) summary.unused_examples.push_back(child);
      }
    } while (FindNextFileW(find, &entry));
    FindClose(find);
  }
  std::sort(summary.unused_examples.begin(), summary.unused_examples.end());
  return summary;
}

bool LooksLikeFontFile(const std::wstring& path) {
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  unsigned char head[4] = {};
  DWORD read = 0;
  const bool ok = ReadFile(file, head, sizeof(head), &read, nullptr) && read == sizeof(head);
  CloseHandle(file);
  if (!ok) return false;
  const auto is = [&head](unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
    return head[0] == a && head[1] == b && head[2] == c && head[3] == d;
  };
  return is(0x00, 0x01, 0x00, 0x00) || is('O', 'T', 'T', 'O') || is('t', 't', 'c', 'f') || is('t', 'r', 'u', 'e');
}

CustomFontResult ApplyCustomFont(const std::wstring& mods_directory, const std::wstring& client_assets_directory,
                                 const std::wstring& font_file) {
  CustomFontResult result;
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  if (!GetFileAttributesExW(font_file.c_str(), GetFileExInfoStandard, &attributes) ||
      (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    result.error = L"That file can’t be read.";
    return result;
  }
  const std::uint64_t size = (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
  if (size == 0 || size > kMaxFontBytes || !LooksLikeFontFile(font_file)) {
    result.error = L"That file isn’t a TrueType or OpenType font.";
    return result;
  }
  const std::wstring client_families = FamiliesDirectory(client_assets_directory);
  const std::vector<std::wstring> families = JsonFilesIn(client_families);
  if (families.empty()) {
    result.error = L"Install Roblox first: its font list wasn’t found.";
    return result;
  }
  std::wstring error;
  const std::wstring fonts = FontsDirectory(mods_directory);
  const std::wstring mod_families = FamiliesDirectory(mods_directory);
  if (!EnsureDirectory(mod_families, &error)) {
    result.error = L"The Mods folder can’t be written. " + error;
    return result;
  }
  if (!CopyFileW(font_file.c_str(), JoinPath(fonts, kCustomFontFile).c_str(), FALSE)) {
    result.error = WithErrorCause(L"The font couldn’t be copied into the Mods folder", GetLastError());
    return result;
  }
  for (const std::wstring& name : families) {
    std::string text;
    if (!ReadFileBytes(JoinPath(client_families, name), &text, &error, 1u << 20)) continue;
    std::string parse_error;
    const std::optional<Json> family = Json::Parse(text, &parse_error);
    if (!family) continue;
    const std::optional<Json> replaced = WithFont(*family);
    if (!replaced) continue;
    if (!AtomicWriteFile(JoinPath(mod_families, name), replaced->Dump(2) + "\n", AtomicWriteBackup::kNone, &error)) {
      result.error = L"The Mods folder can’t be written. " + error;
      return result;
    }
    ++result.families;
  }
  if (result.families == 0) {
    DeleteFileW(JoinPath(fonts, kCustomFontFile).c_str());
    result.error = L"Roblox’s font list couldn’t be read.";
    return result;
  }
  result.ok = true;
  return result;
}

bool RemoveCustomFont(const std::wstring& mods_directory, std::wstring* error) {
  bool ok = true;
  const std::wstring mod_families = FamiliesDirectory(mods_directory);
  for (const std::wstring& name : JsonFilesIn(mod_families)) {
    const std::wstring path = JoinPath(mod_families, name);
    std::string text;
    std::wstring read_error;
    if (!ReadFileBytes(path, &text, &read_error, 1u << 20)) continue;
    if (text.find(kCustomFontAsset) == std::string::npos) continue;
    if (!DeleteFileW(path.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
      if (ok && error != nullptr) *error = WithErrorCause(L"A font file couldn’t be removed", GetLastError());
      ok = false;
    }
  }
  const std::wstring font = JoinPath(FontsDirectory(mods_directory), kCustomFontFile);
  if (!DeleteFileW(font.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) {
    if (ok && error != nullptr) *error = WithErrorCause(L"The font couldn’t be removed", GetLastError());
    ok = false;
  }
  RemoveDirectoryW(mod_families.c_str());
  RemoveDirectoryW(FontsDirectory(mods_directory).c_str());
  RemoveDirectoryW(JoinPath(mods_directory, L"content").c_str());
  return ok;
}

}
