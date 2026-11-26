// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The install, upgrade and uninstall steps follow Fishstrap's Installer.cs (MIT).
#ifndef ELIXIR_LAUNCHER_CORE_INSTALLER_H_
#define ELIXIR_LAUNCHER_CORE_INSTALLER_H_

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

#include "core/roblox_links.h"

namespace elixir::launcher {

inline constexpr wchar_t kInstalledExecutableName[] = L"Elixir.exe";
inline constexpr wchar_t kUninstallKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Elixir";
inline constexpr wchar_t kSettingsFlag[] = L"-settings";
inline constexpr wchar_t kPlayerFlag[] = L"-player";
inline constexpr wchar_t kUninstallFlag[] = L"-uninstall";
inline constexpr wchar_t kQuietFlag[] = L"-quiet";
inline constexpr wchar_t kUpgradeFlag[] = L"-upgrade";
inline constexpr wchar_t kWaitFlag[] = L"--wait-pid";

struct InstallEnvironment {
  HKEY registry_root = HKEY_CURRENT_USER;
  std::wstring uninstall_key = kUninstallKey;
  std::wstring local_app_data;
  std::wstring user_profile;
  std::wstring temp_dir;
  std::wstring desktop_dir;
  std::wstring start_menu_dir;
  LinkRegistry links;
};

InstallEnvironment SystemInstallEnvironment();

std::wstring DefaultInstallLocation(const InstallEnvironment& environment);
std::wstring RegisteredInstallLocation(const InstallEnvironment& environment);
std::wstring InstalledExecutable(const std::wstring& location);
bool ExistingDataPresent(const std::wstring& location);

std::wstring NormalizeInstallLocation(std::wstring_view text);
bool InstallLocationAllowed(const std::wstring& location, const InstallEnvironment& environment);
std::wstring SuggestedOwnFolder(const std::wstring& location);
bool CheckInstallLocation(const std::wstring& location, const InstallEnvironment& environment, std::wstring* error);

struct InstallRequest {
  std::wstring source;
  std::wstring location;
  std::wstring version;
  bool desktop_shortcut = true;
  bool start_menu_shortcut = true;
};

bool InstallElixir(const InstallRequest& request, const InstallEnvironment& environment, std::wstring* error);

std::wstring FileProductVersion(const std::wstring& path);
bool SameFileContents(const std::wstring& a, const std::wstring& b);

struct UpgradeNeed {
  bool differs = false;
  bool older = false;
  std::wstring installed_version;
};

UpgradeNeed CheckUpgrade(const std::wstring& running, const std::wstring& location, std::wstring_view running_version);
bool IsAutomaticUpgradeSource(const std::wstring& running, const std::wstring& location,
                              const InstallEnvironment& environment);
bool UpgradeInstalledCopy(const std::wstring& running, const std::wstring& location, std::wstring_view version,
                          const InstallEnvironment& environment, std::wstring* error);

std::vector<DWORD> ProcessesRunning(const std::wstring& executable);
void StopProcesses(const std::vector<DWORD>& processes);

struct UninstallRequest {
  std::wstring location;
  bool keep_data = true;
};

void UninstallElixir(const UninstallRequest& request, const InstallEnvironment& environment);
std::wstring DelayedDeleteCommand(const std::wstring& location);
bool StartDelayedDelete(const std::wstring& location);

}

#endif
