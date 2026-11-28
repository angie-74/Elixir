// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_ACCOUNT_LIST_H_
#define ELIXIR_LAUNCHER_UI_ACCOUNT_LIST_H_

#include <d2d1.h>

#include <cstddef>
#include <string_view>
#include <vector>

#include "core/accounts.h"
#include "ui/ui.h"

namespace elixir::launcher::ui {

struct AccountRow {
  SavedAccount account;
  AccountUse use = AccountUse::kNotOpen;
};

struct AccountListChoice {
  enum class Kind { kNone, kOpen, kRemove };
  Kind kind = Kind::kNone;
  int row = -1;
};

inline constexpr float kAccountListGap = 4.0f;
inline constexpr float kAccountListLeastWidth = 260.0f;
inline constexpr float kAccountListMostWidth = 420.0f;

float AccountListWidth(Ui& ui, const std::vector<AccountRow>& rows, float least = kAccountListLeastWidth);
float AccountListHeight(std::size_t rows);
float AccountListReveal(int row, std::size_t rows, float scroll, float visible_height);
AccountListChoice BuildAccountList(Ui& ui, std::wstring_view control, const D2D1_RECT_F& panel,
                                   const std::vector<AccountRow>& rows, float scroll);

}

#endif
