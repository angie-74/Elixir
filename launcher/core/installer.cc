// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The install, upgrade and uninstall steps follow Fishstrap's Installer.cs (MIT).

#include "core/installer.h"

#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>

#include "core/mods.h"
#include "core/paths.h"
#include "core/process.h"
#include "core/project.h"
#include "core/sha256.h"
#include "core/shortcuts.h"
#include "core/update.h"
#include "core/util.h"
#include "platform/windows/app_user_model_id.h"

namespace elixir::launcher {

namespace {

constexpr wchar_t kCannotInstallHere[] =
    L"Elixir cannot be installed here. Please choose a different location, or resort to using the default location "
    L"by clicking the reset button.";
constexpr wchar_t kNoWriteAccess[] =
    L"Elixir does not have write access to the install location you've selected. Please choose another location.";
constexpr wchar_t kCannotOverwrite[] =
    L"Elixir has been installed to this location before and is still present, however the installer cannot overwrite "
    L"the old executable.\n\nPlease manually delete Elixir.exe from the install location or try restarting your "
    L"system, and then retry installation afterwards.";

std::wstring Lower(std::wstring_view text) {
  std::wstring out(text);
  for (wchar_t& c : out) c = static_cast<wchar_t>(std::towlower(c));
  return out;
}

bool ContainsI(std::wstring_view text, std::wstring_view part) {
  return Lower(text).find(Lower(part)) != std::wstring::npos;
}

bool EndsWithI(std::wstring_view text, std::wstring_view suffix) {
  return text.size() >= suffix.size() && PathsEqual(text.substr(text.size() - suffix.size()), suffix);
}

std::wstring WithoutTrailingSlash(std::wstring text) {
  while (text.size() > 3 && (text.back() == L'\\' || text.back() == L'/')) text.pop_back();
  return text;
}

bool IsBelow(const std::wstring& path, const std::wstring& root) {
  const std::wstring base = WithoutTrailingSlash(root);
  if (base.empty()) return false;
  return StartsWithI(path, base + L"\\");
}

bool DirectoryHasEntries(const std::wstring& path) {
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileW(JoinPath(path, L"*").c_str(), &data);
  if (find == INVALID_HANDLE_VALUE) return false;
  bool any = false;
  do {
    const std::wstring_view name = data.cFileName;
    if (name != L"." && name != L"..") any = true;
  } while (!any && FindNextFileW(find, &data));
  FindClose(find);
  return any;
}

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
  PWSTR path = nullptr;
  std::wstring result;
  if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &path)) && path != nullptr) result = path;
  if (path != nullptr) CoTaskMemFree(path);
  return result;
}

struct RegistryKey {
  HKEY key = nullptr;
  ~RegistryKey() {
    if (key != nullptr) RegCloseKey(key);
  }
};

void SetText(HKEY key, const wchar_t* name, const std::wstring& value) {
  RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                 static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

void SetNumber(HKEY key, const wchar_t* name, DWORD value) {
  RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

bool HasValue(HKEY key, const wchar_t* name) {
  return RegQueryValueExW(key, name, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

void WriteVersionValues(HKEY key, std::wstring_view version) {
  SetText(key, L"DisplayVersion", std::wstring(version));
  SetText(key, L"Publisher", kProjectPublisher);
  SetText(key, L"HelpLink", kProjectHelp);
  SetText(key, L"URLInfoAbout", kProjectWebsite);
  SetText(key, L"URLUpdateInfo", kProjectReleases);
}

void ClearReadOnly(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY)) {
    SetFileAttributesW(path.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
  }
}

bool SafeToRemoveFrom(const std::wstring& location, const InstallEnvironment& environment) {
  return InstallLocationAllowed(location, environment) && PathIsFile(InstalledExecutable(location));
}

void RemoveMatchingFiles(const std::wstring& location, const wchar_t* prefix) {
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileW(JoinPath(location, std::wstring(prefix) + L"*").c_str(), &data);
  if (find == INVALID_HANDLE_VALUE) return;
  do {
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    if (!StartsWithI(data.cFileName, prefix)) continue;
    DeleteFileEntry(JoinPath(location, data.cFileName), data.dwFileAttributes);
  } while (FindNextFileW(find, &data));
  FindClose(find);
}

}

InstallEnvironment SystemInstallEnvironment() {
  InstallEnvironment environment;
  environment.local_app_data = LocalAppDataDirectory();
  environment.user_profile = KnownFolder(FOLDERID_Profile);
  if (environment.user_profile.empty()) environment.user_profile = EnvironmentVariable(L"USERPROFILE");
  wchar_t temp[MAX_PATH + 2] = {};
  const DWORD length = GetTempPathW(MAX_PATH + 1, temp);
  if (length > 0 && length <= MAX_PATH) environment.temp_dir = WithoutTrailingSlash(temp);
  environment.desktop_dir = DesktopDirectory();
  environment.start_menu_dir = StartMenuProgramsDirectory();
  environment.register_links = LinkRegistrationBlocker().empty();
  return environment;
}

std::wstring DefaultInstallLocation(const InstallEnvironment& environment) {
  return JoinPath(environment.local_app_data, kProjectName);
}

std::wstring RegisteredInstallLocation(const InstallEnvironment& environment) {
  wchar_t buffer[1024] = {};
  DWORD size = sizeof(buffer) - sizeof(wchar_t);
  if (RegGetValueW(environment.registry_root, environment.uninstall_key.c_str(), L"InstallLocation", RRF_RT_REG_SZ,
                   nullptr, buffer, &size) != ERROR_SUCCESS) {
    return {};
  }
  const std::wstring location = NormalizeInstallLocation(buffer);
  if (location.empty() || !PathIsDirectory(location)) return {};
  return location;
}

std::wstring InstalledExecutable(const std::wstring& location) { return JoinPath(location, kInstalledExecutableName); }

bool ExistingDataPresent(const std::wstring& location) { return PathIsFile(JoinPath(location, L"config.json")); }

std::wstring NormalizeInstallLocation(std::wstring_view text) {
  std::wstring location = TrimWide(text);
  while (!location.empty() && location.front() == L'"') location.erase(location.begin());
  while (!location.empty() && location.back() == L'"') location.pop_back();
  location = TrimWide(location);
  std::replace(location.begin(), location.end(), L'/', L'\\');
  if (location.size() >= 3 && location[1] == L':' && location[2] == L'\\') {
    wchar_t full[1024] = {};
    const DWORD length = GetFullPathNameW(location.c_str(), 1024, full, nullptr);
    if (length > 0 && length < 1024) location = full;
  }
  return WithoutTrailingSlash(location);
}

bool InstallLocationAllowed(const std::wstring& location, const InstallEnvironment& environment) {
  if (location.size() <= 3) return false;
  if (location.rfind(L"\\\\", 0) == 0) return false;
  const bool drive_path = std::iswalpha(location[0]) && location[1] == L':' && location[2] == L'\\';
  if (!drive_path) return false;
  if (!environment.temp_dir.empty() &&
      (PathsEqual(location, environment.temp_dir) || IsBelow(location, environment.temp_dir))) {
    return false;
  }
  if (ContainsI(location, L"\\Temp\\") || EndsWithI(location, L"\\Temp")) return false;
  if (ContainsI(location, L"OneDrive")) return false;
  if (!environment.user_profile.empty()) {
    if (PathsEqual(location, environment.user_profile)) return false;
    if (PathsEqual(ParentDirectory(location), environment.user_profile)) return false;
  }
  if (!environment.local_app_data.empty()) {
    if (PathsEqual(location, environment.local_app_data)) return false;
    if (PathsEqual(location, ParentDirectory(environment.local_app_data))) return false;
    if (PathsEqual(ParentDirectory(location), ParentDirectory(environment.local_app_data))) return false;
  }
  if (ContainsI(location, L"Program Files")) return false;
  if (ContainsI(location, L"WindowsApps")) return false;
  if (EndsWithI(location, L"\\Windows") || ContainsI(location, L"\\Windows\\")) return false;
  return true;
}

std::wstring SuggestedOwnFolder(const std::wstring& location) {
  if (EndsWithI(location, kProjectName) || ExistingDataPresent(location)) return {};
  if (!PathIsDirectory(location) || !DirectoryHasEntries(location)) return {};
  return JoinPath(location, kProjectName);
}

bool CheckInstallLocation(const std::wstring& location, const InstallEnvironment& environment, std::wstring* error) {
  if (location.empty()) {
    if (error != nullptr) *error = L"You must set an install location";
    return false;
  }
  if (!InstallLocationAllowed(location, environment)) {
    if (error != nullptr) *error = kCannotInstallHere;
    return false;
  }
  std::wstring directory_error;
  if (!EnsureDirectory(location, &directory_error)) {
    if (error != nullptr) *error = GetLastError() == ERROR_ACCESS_DENIED ? std::wstring(kNoWriteAccess) : directory_error;
    return false;
  }
  const std::wstring test_file = JoinPath(location, L"ElixirWriteTest.txt");
  HANDLE file = CreateFileW(test_file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    const DWORD code = GetLastError();
    if (error != nullptr) {
      *error = code == ERROR_ACCESS_DENIED ? std::wstring(kNoWriteAccess)
                                           : WithErrorCause(L"Elixir can’t write to this folder.", code);
    }
    return false;
  }
  CloseHandle(file);
  DeleteFileW(test_file.c_str());
  return true;
}

bool InstallElixir(const InstallRequest& request, const InstallEnvironment& environment, std::wstring* error) {
  if (!CheckInstallLocation(request.location, environment, error)) return false;
  const std::wstring target = InstalledExecutable(request.location);
  if (!PathsEqual(request.source, target)) {
    ClearReadOnly(target);
    if (!CopyFileW(request.source.c_str(), target.c_str(), FALSE)) {
      if (error != nullptr) *error = kCannotOverwrite;
      return false;
    }
  }

  RegistryKey uninstall;
  if (RegCreateKeyExW(environment.registry_root, environment.uninstall_key.c_str(), 0, nullptr, 0,
                      KEY_READ | KEY_WRITE, nullptr, &uninstall.key, nullptr) != ERROR_SUCCESS) {
    if (error != nullptr) *error = WithErrorCause(L"Elixir couldn’t register itself with Windows.", GetLastError());
    return false;
  }
  const std::wstring quoted = L"\"" + target + L"\"";
  SetText(uninstall.key, L"DisplayIcon", target + L",0");
  SetText(uninstall.key, L"DisplayName", kProjectName);
  if (!HasValue(uninstall.key, L"InstallDate")) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t date[16] = {};
    swprintf(date, 16, L"%04u%02u%02u", now.wYear, now.wMonth, now.wDay);
    SetText(uninstall.key, L"InstallDate", date);
  }
  SetText(uninstall.key, L"InstallLocation", request.location);
  SetNumber(uninstall.key, L"NoRepair", 1);
  SetText(uninstall.key, L"ModifyPath", quoted + L" " + kSettingsFlag);
  SetText(uninstall.key, L"QuietUninstallString", quoted + L" " + kUninstallFlag + L" " + kQuietFlag);
  SetText(uninstall.key, L"UninstallString", quoted + L" " + kUninstallFlag);
  WriteVersionValues(uninstall.key, request.version);

  ShortcutSpec spec;
  spec.target = target;
  spec.description = kProjectName;
  spec.app_id = elixir::kAppUserModelId;
  if (request.desktop_shortcut && !environment.desktop_dir.empty()) {
    EnsureDirectory(environment.desktop_dir, nullptr);
    CreateShortcut(JoinPath(environment.desktop_dir, kElixirShortcutName), spec, nullptr);
  }
  if (request.start_menu_shortcut && !environment.start_menu_dir.empty()) {
    EnsureDirectory(environment.start_menu_dir, nullptr);
    CreateShortcut(JoinPath(environment.start_menu_dir, kElixirShortcutName), spec, nullptr);
  }
  EnsureInstalledLinks(request.location, environment);
  return true;
}

void EnsureInstalledLinks(const std::wstring& location, const InstallEnvironment& environment) {
  if (!environment.register_links || location.empty()) return;
  const std::wstring program = InstalledExecutable(location);
  if (!PathIsFile(program)) return;
  EnsureLinkHandlers(environment.links, program, LinkBackupFile(location), nullptr);
}

std::wstring FileProductVersion(const std::wstring& path) {
  DWORD handle = 0;
  const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
  if (size == 0) return {};
  std::string block(size, '\0');
  if (!GetFileVersionInfoW(path.c_str(), 0, size, block.data())) return {};
  VS_FIXEDFILEINFO* info = nullptr;
  UINT info_size = 0;
  if (!VerQueryValueW(block.data(), L"\\", reinterpret_cast<void**>(&info), &info_size) || info == nullptr ||
      info_size < sizeof(VS_FIXEDFILEINFO)) {
    return {};
  }
  return std::to_wstring(HIWORD(info->dwProductVersionMS)) + L"." + std::to_wstring(LOWORD(info->dwProductVersionMS)) +
         L"." + std::to_wstring(HIWORD(info->dwProductVersionLS));
}

bool SameFileContents(const std::wstring& a, const std::wstring& b) {
  WIN32_FILE_ATTRIBUTE_DATA data_a{};
  WIN32_FILE_ATTRIBUTE_DATA data_b{};
  if (!GetFileAttributesExW(a.c_str(), GetFileExInfoStandard, &data_a) ||
      !GetFileAttributesExW(b.c_str(), GetFileExInfoStandard, &data_b)) {
    return false;
  }
  if (data_a.nFileSizeHigh != data_b.nFileSizeHigh || data_a.nFileSizeLow != data_b.nFileSizeLow) return false;
  std::string hash_a;
  std::string hash_b;
  return Sha256File(a, &hash_a, nullptr) && Sha256File(b, &hash_b, nullptr) && Sha256Equal(hash_a, hash_b);
}

UpgradeNeed CheckUpgrade(const std::wstring& running, const std::wstring& location,
                         std::wstring_view running_version) {
  UpgradeNeed need;
  const std::wstring installed = InstalledExecutable(location);
  if (!PathIsFile(installed) || PathsEqual(running, installed)) return need;
  if (SameFileContents(running, installed)) return need;
  need.differs = true;
  need.installed_version = FileProductVersion(installed);
  need.older = !need.installed_version.empty() &&
               CompareVersions(WideToUtf8(running_version), WideToUtf8(need.installed_version)) < 0;
  return need;
}

bool IsAutomaticUpgradeSource(const std::wstring& running, const std::wstring& location,
                              const InstallEnvironment& environment) {
  if (IsBelow(running, JoinPath(location, L"updates"))) return true;
  if (!environment.temp_dir.empty() && IsBelow(running, environment.temp_dir)) return true;
  if (!environment.local_app_data.empty() && IsBelow(running, JoinPath(environment.local_app_data, L"Temp"))) {
    return true;
  }
  return false;
}

bool UpgradeInstalledCopy(const std::wstring& running, const std::wstring& location, std::wstring_view version,
                          const InstallEnvironment& environment, std::wstring* error) {
  const std::wstring installed = InstalledExecutable(location);
  ClearReadOnly(installed);
  bool copied = false;
  DWORD code = 0;
  for (int attempt = 1; attempt <= 10 && !copied; ++attempt) {
    copied = CopyFileW(running.c_str(), installed.c_str(), FALSE) != 0;
    if (copied) break;
    code = GetLastError();
    const std::wstring aside = installed + L"." + std::to_wstring(GetTickCount64()) + L".old-update";
    if (MoveFileExW(installed.c_str(), aside.c_str(), MOVEFILE_REPLACE_EXISTING)) {
      copied = CopyFileW(running.c_str(), installed.c_str(), FALSE) != 0;
      if (copied) {
        DeleteFileW(aside.c_str());
      } else {
        code = GetLastError();
        MoveFileExW(aside.c_str(), installed.c_str(), MOVEFILE_REPLACE_EXISTING);
      }
    }
    if (!copied) Sleep(500);
  }
  if (!copied) {
    if (error != nullptr) *error = WithErrorCause(L"Elixir couldn’t replace the installed version.", code);
    return false;
  }
  RegistryKey uninstall;
  if (RegCreateKeyExW(environment.registry_root, environment.uninstall_key.c_str(), 0, nullptr, 0,
                      KEY_READ | KEY_WRITE, nullptr, &uninstall.key, nullptr) == ERROR_SUCCESS) {
    WriteVersionValues(uninstall.key, version);
  }
  return true;
}

std::vector<DWORD> ProcessesRunning(const std::wstring& executable) {
  std::vector<DWORD> found;
  const std::wstring name = FileNameOf(executable);
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return found;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (entry.th32ProcessID == GetCurrentProcessId() || !PathsEqual(entry.szExeFile, name)) continue;
      HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
      if (process == nullptr) continue;
      wchar_t path[1024] = {};
      DWORD size = 1024;
      if (QueryFullProcessImageNameW(process, 0, path, &size) && PathsEqual(path, executable)) {
        found.push_back(entry.th32ProcessID);
      }
      CloseHandle(process);
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return found;
}

void StopProcesses(const std::vector<DWORD>& processes) {
  std::vector<HANDLE> handles;
  for (const DWORD pid : processes) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (process == nullptr) continue;
    TerminateProcess(process, 1);
    handles.push_back(process);
  }
  for (HANDLE process : handles) {
    WaitForSingleObject(process, 5000);
    CloseHandle(process);
  }
}

void UninstallElixir(const UninstallRequest& request, const InstallEnvironment& environment) {
  const std::wstring& location = request.location;
  const std::wstring program = InstalledExecutable(location);
  const bool remove_files = SafeToRemoveFrom(location, environment);

  std::wstring ignored;
  UnregisterLinkHandlers(environment.links, program, LinkBackupFile(location), &ignored);

  if (!environment.desktop_dir.empty()) {
    for (const GameShortcut& game : ListGameShortcuts(environment.desktop_dir, program)) {
      DeleteFileW(game.path.c_str());
    }
    RemoveShortcut(JoinPath(environment.desktop_dir, kElixirShortcutName), program);
  }
  if (!environment.start_menu_dir.empty()) {
    RemoveShortcut(JoinPath(environment.start_menu_dir, kElixirShortcutName), program);
  }

  if (remove_files) {
    for (const wchar_t* name : {L"apps", L"downloads", L"tmp", L"bundled", L"updates", L"cache", L"backups",
                                L"crashes", L"links", L"shortcut-icons", L"guest_patches"}) {
      const std::wstring path = JoinPath(location, name);
      if (IsRealDirectoryBelow(location, path)) DeleteDirectoryTree(path);
    }
    for (const wchar_t* name : {L"shortcuts.json", L"window_state.json"}) {
      DeleteFileW(JoinPath(location, name).c_str());
    }
    if (!request.keep_data) {
      const std::wstring mods = ModsDirectory(LauncherPathsForDataRoot(location, location, false));
      for (const std::wstring& path : {mods, JoinPath(location, L"logs"), JoinPath(location, L"data"),
                                       JoinPath(location, L"sdcard"), JoinPath(location, L"auth"),
                                       JoinPath(location, L"instances")}) {
        if (IsRealDirectoryBelow(location, path)) DeleteDirectoryTree(path);
      }
      RemoveMatchingFiles(location, L"config.json");
      RemoveMatchingFiles(location, L"fflags.json");
    }
  }

  RegDeleteTreeW(environment.registry_root, environment.uninstall_key.c_str());
  RegDeleteKeyW(environment.registry_root, environment.uninstall_key.c_str());
}

std::wstring DelayedDeleteCommand(const std::wstring& location) {
  return L"cmd.exe /d /c ping -n 6 127.0.0.1 >nul & del /f /q \"" + InstalledExecutable(location) + L"\" & rmdir \"" +
         location + L"\"";
}

bool StartDelayedDelete(const std::wstring& location) {
  if (location.find(L'"') != std::wstring::npos || location.find(L'%') != std::wstring::npos) return false;
  wchar_t system[MAX_PATH] = {};
  GetSystemDirectoryW(system, MAX_PATH);
  const std::wstring shell = JoinPath(system, L"cmd.exe");
  std::wstring command = DelayedDeleteCommand(location);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(shell.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, system,
                      &startup, &process)) {
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

}
