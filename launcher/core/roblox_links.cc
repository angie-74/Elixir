// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/roblox_links.h"

#include <windows.h>
#include <appmodel.h>
#include <shlobj.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/json.h"
#include "core/util.h"

namespace elixir::launcher {
namespace {

constexpr int kMaxDepth = 8;
constexpr std::size_t kMaxValuesPerKey = 512;
constexpr DWORD kMaxValueBytes = 64 * 1024;
constexpr DWORD kMaxNameChars = 16384;

constexpr wchar_t kNotBackedUp[] = L"Elixir couldn’t save the current Roblox link settings, so nothing was changed.";
constexpr wchar_t kNotRestored[] =
    L"The app that opened Roblox links before couldn’t be set back. Reinstall it to fix this.";

std::wstring SchemePath(const LinkRegistry& registry, const wchar_t* scheme) {
  return registry.classes + L"\\" + scheme;
}

class Key final {
 public:
  Key() = default;
  ~Key() {
    if (key_ != nullptr) RegCloseKey(key_);
  }
  Key(const Key&) = delete;
  Key& operator=(const Key&) = delete;
  HKEY* out() { return &key_; }
  HKEY get() const { return key_; }

 private:
  HKEY key_ = nullptr;
};

std::wstring ReadDefaultString(HKEY root, const std::wstring& path, const wchar_t* name = nullptr) {
  DWORD bytes = 0;
  if (RegGetValueW(root, path.c_str(), name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes == 0) {
    return {};
  }
  std::wstring value(bytes / sizeof(wchar_t), L'\0');
  if (RegGetValueW(root, path.c_str(), name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) != ERROR_SUCCESS) return {};
  value.resize(bytes / sizeof(wchar_t));
  while (!value.empty() && value.back() == L'\0') value.pop_back();
  return value;
}

bool SameText(const std::wstring& a, const std::wstring& b) {
  return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) ==
         CSTR_EQUAL;
}

std::string Hex(const BYTE* data, DWORD size) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(static_cast<std::size_t>(size) * 2);
  for (DWORD i = 0; i < size; ++i) {
    out.push_back(kDigits[data[i] >> 4]);
    out.push_back(kDigits[data[i] & 0x0F]);
  }
  return out;
}

bool Unhex(const std::string& hex, std::vector<BYTE>* out) {
  if (hex.size() % 2 != 0) return false;
  const auto digit = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  out->clear();
  out->reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const int high = digit(hex[i]);
    const int low = digit(hex[i + 1]);
    if (high < 0 || low < 0) return false;
    out->push_back(static_cast<BYTE>(high * 16 + low));
  }
  return true;
}

bool SnapshotKey(HKEY key, int depth, Json* out, std::wstring* error) {
  if (depth > kMaxDepth) {
    *error = kNotBackedUp;
    return false;
  }
  *out = Json::MakeObject();
  Json values = Json::MakeArray();
  for (DWORD index = 0;; ++index) {
    if (index >= kMaxValuesPerKey) {
      *error = kNotBackedUp;
      return false;
    }
    std::wstring name(kMaxNameChars, L'\0');
    DWORD name_chars = kMaxNameChars;
    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = RegEnumValueW(key, index, name.data(), &name_chars, nullptr, &type, nullptr, &bytes);
    if (status == ERROR_NO_MORE_ITEMS) break;
    if (status != ERROR_SUCCESS || bytes > kMaxValueBytes) {
      *error = kNotBackedUp;
      return false;
    }
    name.resize(name_chars);
    std::vector<BYTE> data(bytes);
    name_chars = kMaxNameChars;
    std::wstring again(kMaxNameChars, L'\0');
    status = RegEnumValueW(key, index, again.data(), &name_chars, nullptr, &type, data.empty() ? nullptr : data.data(),
                           &bytes);
    if (status != ERROR_SUCCESS) {
      *error = kNotBackedUp;
      return false;
    }
    data.resize(bytes);
    Json value = Json::MakeObject();
    value.Set("name", WideToUtf8(name));
    value.Set("type", static_cast<std::int64_t>(type));
    value.Set("data", Hex(data.data(), static_cast<DWORD>(data.size())));
    values.Push(std::move(value));
  }
  out->Set("values", std::move(values));
  Json keys = Json::MakeObject();
  for (DWORD index = 0;; ++index) {
    std::wstring name(256, L'\0');
    DWORD name_chars = 256;
    const LSTATUS status = RegEnumKeyExW(key, index, name.data(), &name_chars, nullptr, nullptr, nullptr, nullptr);
    if (status == ERROR_NO_MORE_ITEMS) break;
    if (status != ERROR_SUCCESS) {
      *error = kNotBackedUp;
      return false;
    }
    name.resize(name_chars);
    Key child;
    if (RegOpenKeyExW(key, name.c_str(), 0, KEY_READ, child.out()) != ERROR_SUCCESS) {
      *error = kNotBackedUp;
      return false;
    }
    Json snapshot;
    if (!SnapshotKey(child.get(), depth + 1, &snapshot, error)) return false;
    keys.Set(WideToUtf8(name), std::move(snapshot));
  }
  out->Set("keys", std::move(keys));
  return true;
}

bool RestoreKey(HKEY parent, const std::wstring& name, const Json& snapshot, int depth, std::wstring* error) {
  if (depth > kMaxDepth || !snapshot.is_object()) {
    *error = kNotRestored;
    return false;
  }
  Key key;
  if (RegCreateKeyExW(parent, name.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, key.out(), nullptr) !=
      ERROR_SUCCESS) {
    *error = kNotRestored;
    return false;
  }
  if (const Json* values = snapshot.Find("values"); values != nullptr && values->is_array()) {
    for (const Json& value : values->items()) {
      std::vector<BYTE> data;
      if (!Unhex(value.GetString("data"), &data)) {
        *error = kNotRestored;
        return false;
      }
      const std::wstring value_name = Utf8ToWide(value.GetString("name"));
      const DWORD type = static_cast<DWORD>(value.GetInt("type", REG_SZ));
      if (RegSetValueExW(key.get(), value_name.empty() ? nullptr : value_name.c_str(), 0, type,
                         data.empty() ? nullptr : data.data(), static_cast<DWORD>(data.size())) != ERROR_SUCCESS) {
        *error = kNotRestored;
        return false;
      }
    }
  }
  if (const Json* keys = snapshot.Find("keys"); keys != nullptr && keys->is_object()) {
    for (const Json::Member& child : keys->members()) {
      if (!RestoreKey(key.get(), Utf8ToWide(child.first), child.second, depth + 1, error)) return false;
    }
  }
  return true;
}

bool SetString(HKEY root, const std::wstring& path, const wchar_t* name, const std::wstring& value) {
  Key key;
  if (RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, key.out(), nullptr) != ERROR_SUCCESS) {
    return false;
  }
  return RegSetValueExW(key.get(), name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool WriteScheme(const LinkRegistry& registry, const wchar_t* scheme, const std::wstring& icon,
                 const std::wstring& command) {
  const std::wstring path = SchemePath(registry, scheme);
  return SetString(registry.root, path, nullptr, L"URL: Roblox Protocol") &&
         SetString(registry.root, path, L"URL Protocol", L"") &&
         SetString(registry.root, path + L"\\DefaultIcon", nullptr, icon) &&
         SetString(registry.root, path + L"\\shell\\open\\command", nullptr, command);
}

void DeleteScheme(const LinkRegistry& registry, const wchar_t* scheme) {
  RegDeleteTreeW(registry.root, SchemePath(registry, scheme).c_str());
  RegDeleteKeyW(registry.root, SchemePath(registry, scheme).c_str());
}

LinkHandler HandlerFor(const LinkRegistry& registry, const wchar_t* scheme, const std::wstring& exe) {
  const std::wstring command = ReadDefaultString(registry.root, SchemePath(registry, scheme) + L"\\shell\\open\\command");
  if (command.empty()) return LinkHandler::kNone;
  return SameText(command, LinkCommand(exe)) ? LinkHandler::kElixir : LinkHandler::kOther;
}

Json LoadBackup(const std::wstring& backup_file) {
  std::string bytes;
  std::wstring ignored;
  if (PathIsFile(backup_file) && ReadFileBytes(backup_file, &bytes, &ignored, 4u << 20)) {
    std::string parse_error;
    if (std::optional<Json> parsed = Json::Parse(bytes, &parse_error); parsed && parsed->is_object()) {
      return *parsed;
    }
  }
  Json fresh = Json::MakeObject();
  fresh.Set("version", 1);
  fresh.Set("schemes", Json::MakeObject());
  return fresh;
}

void Notify() { SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr); }

}

std::wstring LinkCommand(const std::wstring& exe) { return L"\"" + exe + L"\" --open-link \"%1\""; }

LinkStatus QueryLinkHandlers(const LinkRegistry& registry, const std::wstring& exe) {
  LinkStatus status;
  status.roblox_player = HandlerFor(registry, L"roblox-player", exe);
  status.roblox = HandlerFor(registry, L"roblox", exe);
  for (const wchar_t* scheme : kLinkSchemes) {
    const std::wstring prog_id =
        ReadDefaultString(registry.root, registry.user_choice + L"\\" + scheme + L"\\UserChoice", L"ProgId");
    if (!prog_id.empty() && !SameText(prog_id, scheme)) status.overridden_by_user_choice = true;
  }
  return status;
}

std::wstring LinkRegistrationBlocker() {
  HANDLE token = nullptr;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) &&
                          elevation.TokenIsElevated != 0;
    CloseHandle(token);
    if (elevated) return L"Elixir is running as administrator. Start it normally to change this.";
  }
  UINT32 length = 0;
  if (GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE) {
    return L"Start Elixir from its own shortcut to change this.";
  }
  return {};
}

bool RegisterLinkHandlers(const LinkRegistry& registry, const std::wstring& exe, const std::wstring& backup_file,
                          std::wstring* error) {
  std::wstring local_error;
  std::wstring& message = error != nullptr ? *error : local_error;
  Json backup = LoadBackup(backup_file);
  Json schemes = backup.Find("schemes") != nullptr && backup.Find("schemes")->is_object() ? *backup.Find("schemes")
                                                                                         : Json::MakeObject();
  bool changed = false;
  for (const wchar_t* scheme : kLinkSchemes) {
    const std::string name = WideToUtf8(scheme);
    if (schemes.Find(name) != nullptr) continue;
    Json entry = Json::MakeObject();
    Key key;
    const LSTATUS opened = RegOpenKeyExW(registry.root, SchemePath(registry, scheme).c_str(), 0, KEY_READ, key.out());
    if (opened == ERROR_FILE_NOT_FOUND) {
      entry.Set("absent", true);
    } else if (opened != ERROR_SUCCESS) {
      message = L"The current Roblox link settings couldn’t be read, so nothing was changed.";
      return false;
    } else if (HandlerFor(registry, scheme, exe) == LinkHandler::kElixir) {
      entry.Set("unknown", true);
    } else {
      Json snapshot;
      if (!SnapshotKey(key.get(), 0, &snapshot, &message)) return false;
      entry.Set("key", std::move(snapshot));
    }
    schemes.Set(name, std::move(entry));
    changed = true;
  }
  backup.Set("schemes", std::move(schemes));
  if (changed) {
    std::wstring write_error;
    if (!EnsureDirectory(ParentDirectory(backup_file), &write_error) ||
        !AtomicWriteFile(backup_file, backup.Dump(), AtomicWriteBackup::kNone, &write_error)) {
      message = kNotBackedUp;
      return false;
    }
  }
  const std::wstring icon = L"\"" + exe + L"\",0";
  const std::wstring command = LinkCommand(exe);
  for (const wchar_t* scheme : kLinkSchemes) {
    DeleteScheme(registry, scheme);
    if (!WriteScheme(registry, scheme, icon, command)) {
      message = L"Windows didn’t accept the change.";
      return false;
    }
  }
  Notify();
  return true;
}

bool UnregisterLinkHandlers(const LinkRegistry& registry, const std::wstring& exe, const std::wstring& backup_file,
                            std::wstring* error) {
  std::wstring local_error;
  std::wstring& message = error != nullptr ? *error : local_error;
  const Json backup = LoadBackup(backup_file);
  const Json* schemes = backup.Find("schemes");
  bool ok = true;
  for (const wchar_t* scheme : kLinkSchemes) {
    if (HandlerFor(registry, scheme, exe) != LinkHandler::kElixir) continue;
    DeleteScheme(registry, scheme);
    const Json* entry = schemes != nullptr && schemes->is_object() ? schemes->Find(WideToUtf8(scheme)) : nullptr;
    if (entry != nullptr && entry->GetBool("absent", false)) continue;
    if (entry != nullptr && entry->Find("key") != nullptr) {
      if (!RestoreKey(registry.root, SchemePath(registry, scheme), *entry->Find("key"), 0, &message)) ok = false;
      continue;
    }
    const std::wstring location = ReadDefaultString(registry.root, registry.player_uninstall, L"InstallLocation");
    const std::wstring player = location.empty() ? std::wstring() : JoinPath(location, L"RobloxPlayerBeta.exe");
    if (!player.empty() && PathIsFile(player)) {
      if (!WriteScheme(registry, scheme, L"\"" + player + L"\",0", L"\"" + player + L"\" %1")) ok = false;
    }
  }
  if (ok) DeleteFileW(backup_file.c_str());
  Notify();
  if (!ok && message.empty()) message = L"Windows didn’t accept the change.";
  return ok;
}

}
