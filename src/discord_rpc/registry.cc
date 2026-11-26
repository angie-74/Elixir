// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/registry.h"

#include <windows.h>

#include "discord_rpc/exceptions.h"

namespace discord_rpc::registry {
namespace {

using helper::Args;

HKEY CreateSubKey(HKEY parent, const std::string& path) {
  HKEY key = nullptr;
  const LSTATUS status = RegCreateKeyExW(parent, helper::ToWide(path).c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                                         KEY_READ | KEY_WRITE, nullptr, &key, nullptr);
  if (status != ERROR_SUCCESS) throw IOException("Win32 error " + std::to_string(status) + ".");
  return key;
}

void SetValue(HKEY key, const wchar_t* name, const std::string& value) {
  const std::wstring wide = helper::ToWide(value);
  const LSTATUS status = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(wide.c_str()),
                                        static_cast<DWORD>((wide.size() + 1) * sizeof(wchar_t)));
  if (status != ERROR_SUCCESS) throw IOException("Win32 error " + std::to_string(status) + ".");
}

struct KeyCloser {
  HKEY key;
  ~KeyCloser() {
    if (key != nullptr) RegCloseKey(key);
  }
};

}

bool WindowsUriScheme::Register(const SchemeInfo& info) {
  const String location = info.ExecutablePath;
  if (!location.has_value()) {
    if (logger_ == nullptr) throw NullReferenceException();
    logger_->Error("Failed to register application because the location was null.");
    return false;
  }

  const std::string schemePath = "discord-" + info.ApplicationID.value_or("");
  const std::string friendlyName = "Run game " + info.ApplicationID.value_or("") + " protocol";
  const std::string defaultIcon = *location;
  std::string command = *location;

  if (info.UsingSteamApp()) {
    const String steam = GetSteamLocation();
    if (steam.has_value()) command = "\"" + *steam + "\" steam://rungameid/" + *info.SteamAppID;
  }

  CreateUriScheme(schemePath, friendlyName, defaultIcon, command);
  return true;
}

void WindowsUriScheme::CreateUriScheme(const std::string& scheme, const std::string& friendly_name,
                                       const std::string& default_icon, const std::string& command) {
  const HKEY root = root_ != nullptr ? static_cast<HKEY>(root_) : HKEY_CURRENT_USER;
  {
    const KeyCloser key{CreateSubKey(root, "SOFTWARE\\Classes\\" + scheme)};
    SetValue(key.key, L"", "URL:" + friendly_name);
    SetValue(key.key, L"URL Protocol", "");

    {
      const KeyCloser iconKey{CreateSubKey(key.key, "DefaultIcon")};
      SetValue(iconKey.key, L"", default_icon);
    }

    {
      const KeyCloser commandKey{CreateSubKey(key.key, "shell\\open\\command")};
      SetValue(commandKey.key, L"", command);
    }
  }

  if (logger_ == nullptr) throw NullReferenceException();
  logger_->Trace("Registered {0}, {1}, {2}", Args(scheme, friendly_name, command));
}

String WindowsUriScheme::GetSteamLocation() {
  const HKEY root = root_ != nullptr ? static_cast<HKEY>(root_) : HKEY_CURRENT_USER;
  HKEY opened = nullptr;
  if (RegOpenKeyExW(root, L"Software\\Valve\\Steam", 0, KEY_READ, &opened) != ERROR_SUCCESS) return std::nullopt;
  const KeyCloser key{opened};
  DWORD type = 0;
  DWORD bytes = 0;
  if (RegQueryValueExW(key.key, L"SteamExe", nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
      (type != REG_SZ && type != REG_EXPAND_SZ)) {
    return std::nullopt;
  }
  std::wstring value(bytes / sizeof(wchar_t) + 1, L'\0');
  if (RegQueryValueExW(key.key, L"SteamExe", nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &bytes) !=
      ERROR_SUCCESS) {
    return std::nullopt;
  }
  value.resize(bytes / sizeof(wchar_t));
  while (!value.empty() && value.back() == L'\0') value.pop_back();
  return helper::FromWide(value);
}

bool UriScheme::Register(const SchemeInfo& info, std::shared_ptr<logging::ILogger> logger) {
  return WindowsUriScheme(std::move(logger)).Register(info);
}

}
