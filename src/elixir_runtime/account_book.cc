// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/account_book.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "elixir_runtime/roblox_session_store.h"

namespace elixir::runtime {
namespace {

constexpr std::size_t kMaximumNameBytes = 128;
constexpr std::uintmax_t kMaximumDetailsBytes = 16 * 1024;
constexpr long long kLastUsedStepSeconds = 60;

void Wipe(std::string* value) {
  if (!value->empty()) SecureZeroMemory(value->data(), value->size());
  value->clear();
}

bool IsPlainName(std::string_view name, bool required) {
  if (name.empty()) return !required;
  if (name.size() > kMaximumNameBytes) return false;
  return std::none_of(name.begin(), name.end(), [](char c) {
    const auto byte = static_cast<unsigned char>(c);
    return byte < 0x20 || byte == 0x7f;
  });
}

bool IsAccountFolderName(const std::wstring& name) {
  return !name.empty() && name.size() <= 20 && name.front() != L'0' &&
         std::all_of(name.begin(), name.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

bool IsReparsePoint(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool ReadDetails(const std::filesystem::path& file, AccountBook::Account* account) {
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(file, error);
  if (error || size == 0 || size > kMaximumDetailsBytes || IsReparsePoint(file)) return false;
  std::ifstream in(file, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const nlohmann::json details = nlohmann::json::parse(text, nullptr, false);
  if (!details.is_object()) return false;
  const auto user_id = details.find("userId");
  const auto username = details.find("username");
  if (user_id == details.end() || !user_id->is_number_integer() || username == details.end() ||
      !username->is_string()) {
    return false;
  }
  account->user_id = user_id->get<long long>();
  account->username = username->get<std::string>();
  const auto display_name = details.find("displayName");
  account->display_name =
      display_name != details.end() && display_name->is_string() ? display_name->get<std::string>() : std::string();
  const auto last_used = details.find("lastUsed");
  account->last_used =
      last_used != details.end() && last_used->is_number_integer() ? last_used->get<long long>() : 0;
  return account->user_id > 0 && IsPlainName(account->username, true) && IsPlainName(account->display_name, false);
}

bool DetailsSayRemoved(const std::filesystem::path& file) {
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(file, error);
  if (error || size == 0 || size > kMaximumDetailsBytes || IsReparsePoint(file)) return false;
  std::ifstream in(file, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const nlohmann::json details = nlohmann::json::parse(text, nullptr, false);
  if (!details.is_object()) return false;
  const auto removed = details.find("removed");
  return removed != details.end() && removed->is_boolean() && removed->get<bool>();
}

bool WriteDetails(const std::filesystem::path& file, const AccountBook::Account& account) {
  nlohmann::json details = nlohmann::json::object();
  details["userId"] = account.user_id;
  details["username"] = account.username;
  details["displayName"] = account.display_name;
  details["lastUsed"] = account.last_used;
  const std::string text = details.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
  std::filesystem::path temporary = file;
  temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out.good()) {
      out.close();
      DeleteFileW(temporary.c_str());
      return false;
    }
  }
  if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(temporary.c_str());
    return false;
  }
  return true;
}

void RemoveAccountFolder(const std::filesystem::path& folder) {
  if (IsReparsePoint(folder)) return;
  DeleteFileW((folder / AccountBook::kSessionFileName).c_str());
  DeleteFileW((folder / AccountBook::kDetailsFileName).c_str());
  RemoveDirectoryW(folder.c_str());
}

std::vector<std::filesystem::path> AccountFolders(const std::filesystem::path& folder) {
  std::vector<std::filesystem::path> folders;
  if (IsReparsePoint(folder)) return folders;
  std::error_code error;
  for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
    const std::filesystem::path path = it->path();
    if (!IsAccountFolderName(path.filename().wstring())) continue;
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      continue;
    }
    folders.push_back(path);
  }
  return folders;
}

}

AccountBook::AccountBook(std::filesystem::path folder) : folder_(std::move(folder)) {}

bool AccountBook::IsRemoved(long long user_id) const {
  if (folder_.empty() || user_id <= 0) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  return DetailsSayRemoved(folder_ / std::to_wstring(user_id) / kDetailsFileName);
}

AccountBook::Remembered AccountBook::Remember(long long user_id, std::string_view username,
                                              std::string_view display_name, std::string_view credential,
                                              long long now_seconds, bool new_sign_in) {
  if (folder_.empty() || user_id <= 0 || !IsPlainName(username, true) || !IsPlainName(display_name, false) ||
      !RobloxSessionStore::IsCanonical(credential)) {
    return Remembered::kFailed;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!new_sign_in && DetailsSayRemoved(folder_ / std::to_wstring(user_id) / kDetailsFileName)) {
    return Remembered::kUnchanged;
  }
  if (!RobloxSessionStore::EnsurePrivateFolder(folder_.parent_path()) ||
      !RobloxSessionStore::EnsurePrivateFolder(folder_)) {
    return Remembered::kFailed;
  }
  const std::filesystem::path account_folder = folder_ / std::to_wstring(user_id);
  RobloxSessionStore session(account_folder / kSessionFileName);
  std::string existing;
  const bool known = session.Load(&existing) == RobloxSessionStore::LoadResult::kFound;
  const bool same_session = known && existing == credential;
  Wipe(&existing);
  if (!same_session && !session.Save(credential)) return Remembered::kFailed;

  Account details;
  const bool had_details = ReadDetails(account_folder / kDetailsFileName, &details) && details.user_id == user_id;
  const bool names_changed = !had_details || details.username != username || details.display_name != display_name;
  if (names_changed || now_seconds - details.last_used >= kLastUsedStepSeconds || now_seconds < details.last_used) {
    details.user_id = user_id;
    details.username.assign(username);
    details.display_name.assign(display_name);
    details.last_used = now_seconds;
    if (!WriteDetails(account_folder / kDetailsFileName, details)) return Remembered::kFailed;
  }

  if (!same_session) {
    std::vector<Account> accounts = ListLocked();
    std::sort(accounts.begin(), accounts.end(),
              [](const Account& a, const Account& b) { return a.last_used > b.last_used; });
    for (std::size_t index = kMaximumAccounts; index < accounts.size(); ++index) {
      if (accounts[index].user_id != user_id) RemoveAccountFolder(folder_ / std::to_wstring(accounts[index].user_id));
    }
  }
  return same_session ? Remembered::kUnchanged : Remembered::kSaved;
}

bool AccountBook::ForgetSession(std::string_view credential) {
  if (folder_.empty() || !RobloxSessionStore::IsCanonical(credential)) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  bool forgotten = false;
  for (const std::filesystem::path& account_folder : AccountFolders(folder_)) {
    RobloxSessionStore session(account_folder / kSessionFileName);
    std::string saved;
    if (session.Load(&saved) == RobloxSessionStore::LoadResult::kFound && saved == credential) {
      RemoveAccountFolder(account_folder);
      forgotten = true;
    }
    Wipe(&saved);
  }
  return forgotten;
}

std::vector<AccountBook::Account> AccountBook::List() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return ListLocked();
}

std::vector<AccountBook::Account> AccountBook::ListLocked() const {
  std::vector<Account> accounts;
  for (const std::filesystem::path& account_folder : AccountFolders(folder_)) {
    Account account;
    if (!ReadDetails(account_folder / kDetailsFileName, &account)) continue;
    if (std::to_wstring(account.user_id) != account_folder.filename().wstring()) continue;
    std::error_code error;
    if (!std::filesystem::is_regular_file(account_folder / kSessionFileName, error)) continue;
    accounts.push_back(std::move(account));
  }
  return accounts;
}

}
