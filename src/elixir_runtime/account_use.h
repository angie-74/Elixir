// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_ACCOUNT_USE_H_
#define ELIXIR_RUNTIME_ACCOUNT_USE_H_

#include <mutex>
#include <string>

namespace elixir::runtime {

inline std::wstring AccountOpenName(long long user_id) {
  return L"Local\\Elixir.Roblox.Account." + std::to_wstring(user_id);
}

inline std::wstring AccountInGameName(long long user_id) { return AccountOpenName(user_id) + L".InGame"; }

class AccountUse final {
 public:
  AccountUse() = default;
  ~AccountUse();
  AccountUse(const AccountUse&) = delete;
  AccountUse& operator=(const AccountUse&) = delete;

  void SetAccount(long long user_id);
  void SetInGame(bool in_game);

 private:
  void UpdateLocked();

  std::mutex mutex_;
  long long user_id_ = 0;
  long long marked_user_id_ = 0;
  bool in_game_ = false;
  void* open_ = nullptr;
  void* game_ = nullptr;
};

}

#endif
