// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_ACCOUNTS_H_
#define ELIXIR_LAUNCHER_CORE_ACCOUNTS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/game_shortcuts.h"

namespace elixir::launcher {

inline constexpr std::size_t kMaxSavedAccounts = 16;
inline constexpr wchar_t kAccountSessionFile[] = L"roblox-session.dpapi";
inline constexpr wchar_t kAccountDetailsFile[] = L"account.json";
inline constexpr wchar_t kAccountPictureDetailsFile[] = L"picture.json";
inline constexpr wchar_t kAccountPicturePrefix[] = L"picture-";

enum class AccountUse {
  kNotOpen,
  kOpen,
  kInGame,
};

struct SavedAccount {
  std::wstring id;
  std::wstring username;
  std::wstring display_name;
  std::int64_t last_used = 0;
  std::wstring session_file;
  std::wstring picture_file;
};

std::vector<SavedAccount> ListSavedAccounts(const std::wstring& accounts_dir);

std::wstring SavedAccountLabel(const SavedAccount& account);
std::wstring SavedAccountName(const SavedAccount& account);

bool IsSavedAccountSession(const std::wstring& accounts_dir, const std::wstring& session_file);

AccountUse QueryAccountUse(const std::wstring& account_id);

bool RemoveSavedAccount(const std::wstring& accounts_dir, const std::wstring& account_id, std::wstring* error);

int RefreshAccountPictures(const std::wstring& accounts_dir, const std::vector<SavedAccount>& accounts,
                           const HttpGetFunction& get, bool* pending = nullptr);

}

#endif
