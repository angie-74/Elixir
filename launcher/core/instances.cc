// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/instances.h"

#include <windows.h>
#include <sddl.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <system_error>
#include <vector>

#include "core/accounts.h"
#include "core/util.h"

namespace elixir::launcher {
namespace {

constexpr wchar_t kSessionFile[] = L"roblox-session.dpapi";
constexpr wchar_t kCookiesFile[] = L"roblox-cookies.dpapi";

std::wstring Lower(std::wstring text) {
  std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
  return text;
}

bool CopyTree(const std::wstring& from, const std::wstring& to, std::wstring* error) {
  if (!CreateDirectoryW(to.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
    if (error != nullptr) *error = L"could not create " + to;
    return false;
  }
  WIN32_FIND_DATAW entry{};
  const HANDLE find = FindFirstFileExW(JoinPath(from, L"*").c_str(), FindExInfoBasic, &entry, FindExSearchNameMatch,
                                       nullptr, FIND_FIRST_EX_LARGE_FETCH);
  if (find == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
  bool ok = true;
  do {
    const std::wstring name = entry.cFileName;
    if (name == L"." || name == L".." || IsInstanceCopySkipped(name)) continue;
    if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) continue;
    const std::wstring source = JoinPath(from, name);
    const std::wstring target = JoinPath(to, name);
    if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      ok = CopyTree(source, target, error);
    } else if (!CopyFileW(source.c_str(), target.c_str(), FALSE)) {
      if (error != nullptr) *error = L"could not copy " + source;
      ok = false;
    }
  } while (ok && FindNextFileW(find, &entry));
  FindClose(find);
  return ok;
}

bool RemoveTree(const std::wstring& path) {
  WIN32_FIND_DATAW entry{};
  const HANDLE find = FindFirstFileExW(JoinPath(path, L"*").c_str(), FindExInfoBasic, &entry, FindExSearchNameMatch,
                                       nullptr, 0);
  if (find != INVALID_HANDLE_VALUE) {
    do {
      const std::wstring name = entry.cFileName;
      if (name == L"." || name == L"..") continue;
      const std::wstring child = JoinPath(path, name);
      if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
          (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
        RemoveTree(child);
      } else if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        RemoveDirectoryW(child.c_str());
      } else {
        SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(child.c_str());
      }
    } while (FindNextFileW(find, &entry));
    FindClose(find);
  }
  return RemoveDirectoryW(path.c_str()) != FALSE;
}

bool CreatePrivateFolder(const std::wstring& folder) {
  std::wstring sddl = L"D:P";
  HANDLE token = nullptr;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buffer(size);
    wchar_t* sid = nullptr;
    if (size > 0 && GetTokenInformation(token, TokenUser, buffer.data(), size, &size) &&
        ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid)) {
      sddl += L"(A;OICI;FA;;;" + std::wstring(sid) + L")";
      LocalFree(sid);
    }
    CloseHandle(token);
  }
  sddl += L"(A;OICI;FA;;;SY)";
  SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, FALSE};
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                            &attributes.lpSecurityDescriptor, nullptr)) {
    return false;
  }
  const BOOL created = CreateDirectoryW(folder.c_str(), &attributes);
  LocalFree(attributes.lpSecurityDescriptor);
  return created || GetLastError() == ERROR_ALREADY_EXISTS;
}

}

bool IsInstanceCopySkipped(const std::wstring& name) {
  const std::wstring lower = Lower(name);
  static const wchar_t* const kSkipped[] = {L"cache", L"code_cache", L"rbx-storage", L"ota_rbxm_decompressed_cache",
                                            L"logs", L"http-wob", L"wob", L"raknet", L"sounds", L"videos",
                                            L"temporarycagemeshprovider"};
  for (const wchar_t* skipped : kSkipped) {
    if (lower == skipped) return true;
  }
  return lower.rfind(L"contentprovider_", 0) == 0;
}

static std::wstring DataFolderMutexName(const std::wstring& data_root) {
  std::error_code error;
  std::wstring key = std::filesystem::weakly_canonical(std::filesystem::path(data_root), error).wstring();
  if (error || key.empty()) key = data_root;
  key = Lower(key);
  std::uint64_t hash = 1469598103934665603ull;
  for (const wchar_t c : key) {
    hash ^= static_cast<std::uint16_t>(c);
    hash *= 1099511628211ull;
  }
  wchar_t name[64];
  std::swprintf(name, 64, L"Local\\Elixir.Roblox.%016llx", static_cast<unsigned long long>(hash));
  return name;
}

bool DataFolderInUse(const std::wstring& data_root) {
  const HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, DataFolderMutexName(data_root).c_str());
  if (mutex == nullptr) return false;
  CloseHandle(mutex);
  return true;
}

HANDLE TryReserveDataFolder(const std::wstring& data_root) {
  const std::wstring name = DataFolderMutexName(data_root);
  const HANDLE mutex = CreateMutexW(nullptr, FALSE, name.c_str());
  if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(mutex);
    return nullptr;
  }
  return mutex;
}

LauncherPaths InstancePaths(const LauncherPaths& paths, int instance) {
  LauncherPaths result = LauncherPathsForDataRoot(
      paths.exe_dir, JoinPath(JoinPath(paths.data_root, L"instances"), std::to_wstring(instance)), paths.portable);
  result.config_file = paths.config_file;
  result.fflags_file = paths.fflags_file;
  result.crashes_dir = paths.crashes_dir;
  result.apps_dir = paths.apps_dir;
  result.runtime_exe = paths.runtime_exe;
  result.runtime_flag = paths.runtime_flag;
  result.guest_runtime_dir = paths.guest_runtime_dir;
  result.bundle_dir = paths.bundle_dir;
  result.accounts_dir = paths.accounts_dir;
  return result;
}

bool PrepareInstance(const LauncherConfig& config, const LauncherPaths& paths, int instance, std::wstring* error,
                     const InstanceAccount& account) {
  if (account.sign_in == InstanceSignIn::kSavedAccount &&
      !IsSavedAccountSession(paths.accounts_dir, account.session_file)) {
    if (error != nullptr) *error = L"that saved account is no longer there";
    return false;
  }
  const LauncherPaths target = InstancePaths(paths, instance);
  if (!EnsureDirectory(target.data_root, error)) return false;
  const std::string& package = config.installed.package.empty() ? std::string("com.roblox.client") : config.installed.package;
  const std::wstring source_app = JoinPath(paths.android_data_dir, Utf8ToWide(package));
  const std::wstring target_app = JoinPath(target.android_data_dir, Utf8ToWide(package));
  if (!PathIsDirectory(target_app) && PathIsDirectory(source_app)) {
    if (!EnsureDirectory(target.android_data_dir, error)) return false;
    const std::wstring partial = target_app + L".part";
    if (PathIsDirectory(partial)) RemoveTree(partial);
    if (!CopyTree(source_app, partial, error)) {
      RemoveTree(partial);
      return false;
    }
    if (!MoveFileExW(partial.c_str(), target_app.c_str(), 0)) {
      RemoveTree(partial);
      if (error != nullptr) *error = L"could not finish " + target_app;
      return false;
    }
  }
  const std::wstring source_auth_folder = JoinPath(paths.data_root, L"auth");
  const std::wstring target_auth_folder = JoinPath(target.data_root, L"auth");
  const std::wstring target_auth = JoinPath(target_auth_folder, kSessionFile);
  const auto copy_failed = [error] {
    if (error != nullptr) *error = L"could not copy the saved sign-in";
    return false;
  };
  const std::wstring source_cookies = JoinPath(source_auth_folder, kCookiesFile);
  const std::wstring target_cookies = JoinPath(target_auth_folder, kCookiesFile);
  if (account.sign_in != InstanceSignIn::kKeep) {
    if (PathIsFile(target_cookies) && !DeleteFileW(target_cookies.c_str())) {
      if (error != nullptr) *error = L"could not remove the cookies of the account the window had before";
      return false;
    }
  } else if (!PathIsFile(target_cookies) && PathIsFile(source_cookies) && !PathIsFile(target_auth)) {
    if (!CreatePrivateFolder(target_auth_folder) || !CopyFileW(source_cookies.c_str(), target_cookies.c_str(), TRUE)) {
      return copy_failed();
    }
  }
  switch (account.sign_in) {
    case InstanceSignIn::kKeep: {
      const std::wstring source_auth = JoinPath(source_auth_folder, kSessionFile);
      if (!PathIsFile(target_auth) && PathIsFile(source_auth)) {
        if (!CreatePrivateFolder(target_auth_folder) || !CopyFileW(source_auth.c_str(), target_auth.c_str(), TRUE)) {
          return copy_failed();
        }
      }
      break;
    }
    case InstanceSignIn::kSavedAccount: {
      const std::wstring partial = target_auth + L".part";
      if (!CreatePrivateFolder(target_auth_folder) ||
          !CopyFileW(account.session_file.c_str(), partial.c_str(), FALSE) ||
          !MoveFileExW(partial.c_str(), target_auth.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(partial.c_str());
        return copy_failed();
      }
      break;
    }
    case InstanceSignIn::kSignedOut:
      if (PathIsFile(target_auth) && !DeleteFileW(target_auth.c_str())) {
        if (error != nullptr) *error = L"could not remove the saved sign-in";
        return false;
      }
      break;
  }
  return true;
}

}
