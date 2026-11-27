// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/accounts.h"

#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <optional>
#include <string_view>

#include "core/json.h"
#include "core/util.h"
#include "elixir_runtime/account_use.h"

namespace elixir::launcher {
namespace {

constexpr std::size_t kMaxNameLength = 128;
constexpr std::uint64_t kMaxDetailsBytes = 16 * 1024;
constexpr std::uint64_t kMaxSessionBytes = 128 * 1024;
constexpr std::uint64_t kMaxPictureBytes = 2u << 20;
constexpr std::uint64_t kMaxAnswerBytes = 256 * 1024;
constexpr std::string_view kPngSignature = "\x89PNG\r\n\x1a\n";

bool IsAccountId(const std::wstring& name) {
  return !name.empty() && name.size() <= 18 && name.front() != L'0' &&
         std::all_of(name.begin(), name.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

bool IsPlainName(const std::wstring& name) {
  return name.size() <= kMaxNameLength &&
         std::none_of(name.begin(), name.end(), [](wchar_t c) { return c < 0x20 || c == 0x7f; });
}

bool IsPlainFile(const std::wstring& path, std::uint64_t min_bytes, std::uint64_t max_bytes) {
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return false;
  if ((data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) return false;
  const std::uint64_t size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  return size > min_bytes && size <= max_bytes;
}

bool IsRealFolder(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
         (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

std::vector<std::wstring> PictureFiles(const std::wstring& folder) {
  std::vector<std::wstring> files;
  WIN32_FIND_DATAW entry{};
  const std::wstring pattern = JoinPath(folder, std::wstring(kAccountPicturePrefix) + L"*.png");
  const HANDLE find = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &entry, FindExSearchNameMatch, nullptr, 0);
  if (find == INVALID_HANDLE_VALUE) return files;
  do {
    if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) continue;
    files.push_back(JoinPath(folder, entry.cFileName));
  } while (FindNextFileW(find, &entry));
  FindClose(find);
  return files;
}

std::wstring PictureName(std::string_view url) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char c : url) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
  wchar_t name[48];
  std::swprintf(name, 48, L"%ls%016llx.png", kAccountPicturePrefix, static_cast<unsigned long long>(hash));
  return name;
}

std::string SavedPictureUrl(const std::wstring& folder) {
  std::string text;
  if (!ReadFileBytes(JoinPath(folder, kAccountPictureDetailsFile), &text, nullptr, kMaxDetailsBytes)) return {};
  const std::optional<Json> details = Json::Parse(text, nullptr);
  return details && details->is_object() ? details->GetString("url") : std::string();
}

bool OpensMutex(const std::wstring& name) {
  const HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, name.c_str());
  if (mutex == nullptr) return false;
  CloseHandle(mutex);
  return true;
}

}

std::vector<SavedAccount> ListSavedAccounts(const std::wstring& accounts_dir) {
  std::vector<SavedAccount> accounts;
  if (accounts_dir.empty() || !IsRealFolder(accounts_dir)) return accounts;
  WIN32_FIND_DATAW entry{};
  const HANDLE find = FindFirstFileExW(JoinPath(accounts_dir, L"*").c_str(), FindExInfoBasic, &entry,
                                       FindExSearchNameMatch, nullptr, 0);
  if (find == INVALID_HANDLE_VALUE) return accounts;
  do {
    const std::wstring name = entry.cFileName;
    if (!IsAccountId(name) || (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      continue;
    }
    const std::wstring folder = JoinPath(accounts_dir, name);
    SavedAccount account;
    account.id = name;
    account.session_file = JoinPath(folder, kAccountSessionFile);
    if (!IsPlainFile(account.session_file, 8, kMaxSessionBytes)) continue;
    std::string text;
    if (!ReadFileBytes(JoinPath(folder, kAccountDetailsFile), &text, nullptr, kMaxDetailsBytes)) continue;
    const std::optional<Json> details = Json::Parse(text, nullptr);
    if (!details || !details->is_object() || details->GetBool("removed", false)) continue;
    if (std::to_wstring(details->GetInt("userId", 0)) != name) continue;
    account.username = Utf8ToWide(details->GetString("username"));
    account.display_name = Utf8ToWide(details->GetString("displayName"));
    account.last_used = details->GetInt("lastUsed", 0);
    if (account.username.empty() || !IsPlainName(account.username) || !IsPlainName(account.display_name)) continue;
    for (const std::wstring& picture : PictureFiles(folder)) {
      if (IsPlainFile(picture, kPngSignature.size(), kMaxPictureBytes)) {
        account.picture_file = picture;
        break;
      }
    }
    accounts.push_back(std::move(account));
  } while (FindNextFileW(find, &entry));
  FindClose(find);
  std::sort(accounts.begin(), accounts.end(), [](const SavedAccount& a, const SavedAccount& b) {
    return a.last_used != b.last_used ? a.last_used > b.last_used : a.id < b.id;
  });
  if (accounts.size() > kMaxSavedAccounts) accounts.resize(kMaxSavedAccounts);
  return accounts;
}

std::wstring SavedAccountName(const SavedAccount& account) {
  return account.display_name.empty() ? account.username : account.display_name;
}

std::wstring SavedAccountLabel(const SavedAccount& account) {
  if (account.display_name.empty() || account.display_name == account.username) return L"@" + account.username;
  return account.display_name + L" (@" + account.username + L")";
}

bool IsSavedAccountSession(const std::wstring& accounts_dir, const std::wstring& session_file) {
  if (accounts_dir.empty() || FileNameOf(session_file) != kAccountSessionFile) return false;
  const std::wstring folder = ParentDirectory(session_file);
  return IsAccountId(FileNameOf(folder)) && PathsEqual(ParentDirectory(folder), accounts_dir) &&
         IsRealFolder(folder) && IsPlainFile(session_file, 8, kMaxSessionBytes);
}

AccountUse QueryAccountUse(const std::wstring& account_id) {
  if (!IsAccountId(account_id)) return AccountUse::kNotOpen;
  const long long user_id = std::wcstoll(account_id.c_str(), nullptr, 10);
  if (user_id <= 0) return AccountUse::kNotOpen;
  if (OpensMutex(runtime::AccountInGameName(user_id))) return AccountUse::kInGame;
  return OpensMutex(runtime::AccountOpenName(user_id)) ? AccountUse::kOpen : AccountUse::kNotOpen;
}

bool RemoveSavedAccount(const std::wstring& accounts_dir, const std::wstring& account_id, std::wstring* error) {
  const auto fail = [error](const wchar_t* text) {
    if (error != nullptr) *error = text;
    return false;
  };
  if (accounts_dir.empty() || !IsAccountId(account_id)) return fail(L"That account isn’t in the list.");
  const std::wstring folder = JoinPath(accounts_dir, account_id);
  if (!IsRealFolder(accounts_dir) || !IsRealFolder(folder)) return fail(L"That account isn’t in the list.");
  const std::wstring session = JoinPath(folder, kAccountSessionFile);
  if (GetFileAttributesW(session.c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(session.c_str())) {
    return fail(L"The saved sign-in couldn’t be removed.");
  }
  for (const std::wstring& picture : PictureFiles(folder)) DeleteFileW(picture.c_str());
  DeleteFileW(JoinPath(folder, kAccountPictureDetailsFile).c_str());
  Json details = Json::MakeObject();
  details.Set("userId", Json(static_cast<std::int64_t>(std::wcstoll(account_id.c_str(), nullptr, 10))));
  details.Set("removed", Json(true));
  AtomicWriteFile(JoinPath(folder, kAccountDetailsFile), details.Dump() + "\n", AtomicWriteBackup::kNone, nullptr);
  return true;
}

int RefreshAccountPictures(const std::wstring& accounts_dir, const std::vector<SavedAccount>& accounts,
                           const HttpGetFunction& get) {
  if (!get || accounts_dir.empty() || accounts.empty()) return 0;
  std::wstring ids;
  for (const SavedAccount& account : accounts) {
    if (!IsAccountId(account.id)) continue;
    if (!ids.empty()) ids += L",";
    ids += account.id;
  }
  if (ids.empty()) return 0;
  const HttpResult answer = get(L"https://thumbnails.roblox.com/v1/users/avatar-headshot?userIds=" + ids +
                                    L"&size=150x150&format=Png&isCircular=true",
                                kMaxAnswerBytes);
  if (!answer.ok) return 0;
  const std::optional<Json> listing = Json::Parse(answer.body, nullptr);
  const Json* data = listing && listing->is_object() ? listing->Find("data") : nullptr;
  if (data == nullptr || !data->is_array()) return 0;
  int changed = 0;
  for (const Json& item : data->items()) {
    if (!item.is_object() || item.GetString("state") != "Completed") continue;
    const std::wstring id = std::to_wstring(item.GetInt("targetId", 0));
    const auto account = std::find_if(accounts.begin(), accounts.end(),
                                      [&id](const SavedAccount& candidate) { return candidate.id == id; });
    const std::string url = item.GetString("imageUrl");
    if (account == accounts.end() || !IsRobloxImageUrl(url)) continue;
    const std::wstring folder = JoinPath(accounts_dir, id);
    const std::wstring target = JoinPath(folder, PictureName(url));
    if (!IsRealFolder(folder)) continue;
    if (SavedPictureUrl(folder) == url && IsPlainFile(target, kPngSignature.size(), kMaxPictureBytes)) continue;
    const HttpResult picture = get(Utf8ToWide(url), kMaxPictureBytes);
    if (!picture.ok || picture.body.size() <= kPngSignature.size() ||
        std::string_view(picture.body).substr(0, kPngSignature.size()) != kPngSignature) {
      continue;
    }
    if (!AtomicWriteFile(target, picture.body, AtomicWriteBackup::kNone, nullptr)) continue;
    Json details = Json::MakeObject();
    details.Set("url", Json(url));
    AtomicWriteFile(JoinPath(folder, kAccountPictureDetailsFile), details.Dump() + "\n", AtomicWriteBackup::kNone,
                    nullptr);
    for (const std::wstring& old_picture : PictureFiles(folder)) {
      if (!PathsEqual(old_picture, target)) DeleteFileW(old_picture.c_str());
    }
    ++changed;
  }
  return changed;
}

}
