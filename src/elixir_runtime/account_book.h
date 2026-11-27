// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_ACCOUNT_BOOK_H_
#define ELIXIR_RUNTIME_ACCOUNT_BOOK_H_

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace elixir::runtime {

class AccountBook final {
 public:
  static constexpr std::size_t kMaximumAccounts = 16;
  static constexpr wchar_t kSessionFileName[] = L"roblox-session.dpapi";
  static constexpr wchar_t kDetailsFileName[] = L"account.json";

  enum class Remembered {
    kUnchanged,
    kSaved,
    kFailed,
  };

  struct Account {
    long long user_id = 0;
    std::string username;
    std::string display_name;
    long long last_used = 0;
  };

  explicit AccountBook(std::filesystem::path folder);

  AccountBook(const AccountBook&) = delete;
  AccountBook& operator=(const AccountBook&) = delete;

  Remembered Remember(long long user_id, std::string_view username, std::string_view display_name,
                      std::string_view credential, long long now_seconds, bool new_sign_in = true);
  bool IsRemoved(long long user_id) const;
  bool ForgetSession(std::string_view credential);
  std::vector<Account> List() const;

  const std::filesystem::path& folder() const { return folder_; }

 private:
  std::vector<Account> ListLocked() const;

  std::filesystem::path folder_;
  mutable std::mutex mutex_;
};

}

#endif
