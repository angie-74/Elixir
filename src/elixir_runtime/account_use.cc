// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/account_use.h"

#include <windows.h>

namespace elixir::runtime {
namespace {

void Close(void** handle) {
  if (*handle != nullptr) CloseHandle(static_cast<HANDLE>(*handle));
  *handle = nullptr;
}

}

AccountUse::~AccountUse() {
  std::lock_guard<std::mutex> lock(mutex_);
  Close(&game_);
  Close(&open_);
}

void AccountUse::SetAccount(long long user_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  user_id_ = user_id > 0 ? user_id : 0;
  UpdateLocked();
}

void AccountUse::SetInGame(bool in_game) {
  std::lock_guard<std::mutex> lock(mutex_);
  in_game_ = in_game;
  UpdateLocked();
}

void AccountUse::UpdateLocked() {
  if (marked_user_id_ != user_id_) {
    Close(&game_);
    Close(&open_);
    marked_user_id_ = user_id_;
  }
  if (user_id_ == 0) return;
  if (open_ == nullptr) open_ = CreateMutexW(nullptr, FALSE, AccountOpenName(user_id_).c_str());
  if (in_game_ && game_ == nullptr) game_ = CreateMutexW(nullptr, FALSE, AccountInGameName(user_id_).c_str());
  if (!in_game_) Close(&game_);
}

}
