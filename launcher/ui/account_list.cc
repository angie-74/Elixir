// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "ui/account_list.h"

#include <algorithm>
#include <string>

#include "core/i18n.h"
#include "ui/theme.h"

namespace elixir::launcher::ui {

namespace {

constexpr float kRowHeight = 52.0f;
constexpr float kLastRowHeight = 40.0f;
constexpr float kPadding = 4.0f;
constexpr float kPicture = 36.0f;
constexpr float kTextLeft = 10.0f + kPicture + 12.0f;
constexpr float kRemoveSpace = 40.0f;
constexpr float kStateGap = 7.0f;
constexpr float kSmallestText = 0.72f;

std::wstring StateOf(const AccountRow& row) {
  return row.use == AccountUse::kInGame ? T(L"In game") : row.use == AccountUse::kOpen ? T(L"Open##account") : T(L"");
}

std::wstring DetailOf(const AccountRow& row) {
  std::wstring detail = L"@" + row.account.username;
  if (row.use != AccountUse::kNotOpen) detail += L"  ·  " + StateOf(row);
  return detail;
}

float DetailWidth(Ui& ui, const AccountRow& row) {
  const std::wstring handle = L"@" + row.account.username;
  if (row.use == AccountUse::kNotOpen) return ui.TextWidth(handle, TextStyle::kCaption);
  return ui.TextWidth(handle + L"  ·", TextStyle::kCaption) + kStateGap +
         ui.TextWidth(StateOf(row), TextStyle::kCaption);
}

}

float AccountListWidth(Ui& ui, const std::vector<AccountRow>& rows, float least) {
  float widest = ui.TextWidth(T(L"Add account"), TextStyle::kBody);
  for (const AccountRow& row : rows) {
    widest = std::max(widest, ui.TextWidth(SavedAccountName(row.account), TextStyle::kBody));
    widest = std::max(widest, DetailWidth(ui, row));
  }
  widest += 2.0f;
  return std::clamp(widest + kTextLeft + kRemoveSpace + 2.0f * kPadding, least,
                    std::max(least, kAccountListMostWidth));
}

float AccountListHeight(std::size_t rows) {
  return kRowHeight * static_cast<float>(rows) + kLastRowHeight + 2.0f * kPadding;
}

float AccountListReveal(int row, std::size_t rows, float scroll, float visible_height) {
  const int count = static_cast<int>(rows);
  const int shown = std::clamp(row, 0, count);
  const float max_scroll = std::max(0.0f, AccountListHeight(rows) - visible_height);
  const float row_top = static_cast<float>(shown) * kRowHeight;
  const float row_bottom = row_top + (shown < count ? kRowHeight : kLastRowHeight);
  const float inner = visible_height - 2.0f * kPadding;
  if (row_top < scroll) scroll = row_top;
  if (row_bottom > scroll + inner) scroll = row_bottom - inner;
  return std::clamp(scroll, 0.0f, max_scroll);
}

AccountListChoice BuildAccountList(Ui& ui, std::wstring_view control_id, const D2D1_RECT_F& panel,
                                   const std::vector<AccountRow>& rows, float scroll) {
  const std::wstring control(control_id);
  const int count = static_cast<int>(rows.size());
  const float needed = AccountListHeight(rows.size());
  const float visible = panel.bottom - panel.top;
  const float max_scroll = std::max(0.0f, needed - visible);
  scroll = std::clamp(scroll, 0.0f, max_scroll);
  const float top = panel.top - scroll;
  AccountListChoice choice;
  ui.Fill(panel, theme::kDialog, theme::kControlRadius);
  ui.Stroke(panel, theme::kControlBorder, theme::kControlRadius);
  if (max_scroll > 0.0f) {
    ui.PushClip(D2D1::RectF(panel.left, panel.top + kPadding, panel.right, panel.bottom - kPadding));
  }
  for (int i = 0; i < count; ++i) {
    const AccountRow& row = rows[static_cast<std::size_t>(i)];
    const float row_top = top + kPadding + static_cast<float>(i) * kRowHeight;
    const D2D1_RECT_F rect = D2D1::RectF(panel.left + kPadding, row_top, panel.right - kPadding, row_top + kRowHeight);
    const std::wstring name = SavedAccountName(row.account);
    if (ui.MenuRow(control + L"#" + std::to_wstring(i), rect, name + L", " + DetailOf(row))) {
      choice = AccountListChoice{AccountListChoice::Kind::kOpen, i};
    }
    const float center = (rect.top + rect.bottom) / 2.0f;
    const D2D1_RECT_F picture = D2D1::RectF(rect.left + 10.0f, center - kPicture / 2.0f, rect.left + 10.0f + kPicture,
                                            center + kPicture / 2.0f);
    ui.Fill(picture, theme::White(0.08f), kPicture / 2.0f);
    const bool row_is_new = ui.Animate(L"account.row:" + row.account.id, 1.0f, 1.0f, 0.0f) < 1.0f;
    const float pictured =
        row.account.picture_file.empty() ? 0.0f : ui.Image(picture, row.account.picture_file, !row_is_new);
    if (pictured < 1.0f || !ui.painting()) {
      D2D1_COLOR_F initial = theme::kTextSecondary;
      initial.a *= 1.0f - pictured;
      ui.TextLine(picture, name.substr(0, 1), TextStyle::kBodyStrong, initial, Align::kCenter);
    }
    const float text_left = rect.left + kTextLeft;
    const float text_right = rect.right - kRemoveSpace;
    ui.TextLine(D2D1::RectF(text_left, rect.top + 7.0f, text_right, rect.top + 28.0f), name, TextStyle::kBody,
                theme::kText, Align::kLeading, kSmallestText);
    const std::wstring handle = L"@" + row.account.username;
    const D2D1_RECT_F detail_rect = D2D1::RectF(text_left, rect.top + 27.0f, text_right, rect.bottom - 7.0f);
    if (row.use == AccountUse::kNotOpen) {
      ui.TextLine(detail_rect, handle, TextStyle::kCaption, theme::kTextSecondary, Align::kLeading, kSmallestText);
    } else {
      const std::wstring before = handle + L"  ·";
      const std::wstring state = StateOf(row);
      const float before_width = ui.TextWidth(before, TextStyle::kCaption);
      const float state_width = ui.TextWidth(state, TextStyle::kCaption);
      const float available = text_right - text_left;
      const float wanted = before_width + kStateGap + state_width + 1.0f;
      const float scale = wanted > available ? std::max(kSmallestText, available / wanted) : 1.0f;
      const float state_room = std::min(available, state_width * scale + 1.0f);
      const float before_room = std::max(1.0f, std::min(before_width * scale + 1.0f,
                                                        available - state_room - kStateGap * scale));
      ui.TextLine(D2D1::RectF(text_left, detail_rect.top, text_left + before_room, detail_rect.bottom), before,
                  TextStyle::kCaption, theme::kTextSecondary, Align::kLeading, kSmallestText);
      const float state_left = text_left + before_room + kStateGap * scale;
      ui.TextLine(D2D1::RectF(state_left, detail_rect.top, std::min(text_right, state_left + state_room),
                              detail_rect.bottom),
                  state, TextStyle::kCaption,
                  row.use == AccountUse::kInGame ? theme::Rgb(ui.state().control_accent_rgb) : theme::kTextSecondary,
                  Align::kLeading, kSmallestText);
    }
    const D2D1_RECT_F remove = D2D1::RectF(rect.right - 34.0f, center - 14.0f, rect.right - 6.0f, center + 14.0f);
    if (ui.IconButton(control + L"#remove." + std::to_wstring(i), remove, glyph::kCancel,
                      TF(L"Remove {0}", SavedAccountLabel(row.account)))) {
      choice = AccountListChoice{AccountListChoice::Kind::kRemove, i};
    }
  }
  const float last_top = top + kPadding + static_cast<float>(count) * kRowHeight;
  const D2D1_RECT_F last =
      D2D1::RectF(panel.left + kPadding, last_top, panel.right - kPadding, last_top + kLastRowHeight);
  if (ui.MenuRow(control + L"#" + std::to_wstring(count), last, T(L"Add account"))) {
    choice = AccountListChoice{AccountListChoice::Kind::kOpen, count};
  }
  ui.Glyph(last.left + 10.0f + kPicture / 2.0f, (last.top + last.bottom) / 2.0f, glyph::kAdd, TextStyle::kIconSmall,
           theme::kTextSecondary);
  ui.TextLine(D2D1::RectF(last.left + kTextLeft, last.top, last.right - 10.0f, last.bottom), T(L"Add account"),
              TextStyle::kBody, theme::kText);
  if (max_scroll > 0.0f) {
    ui.PopClip();
    const float track = visible - 2.0f * kPadding - 8.0f;
    const float thumb = std::max(24.0f, track * (visible - 2.0f * kPadding) / (needed - 2.0f * kPadding));
    const float offset = (track - thumb) * (scroll / max_scroll);
    ui.Fill(D2D1::RectF(panel.right - 5.0f, panel.top + kPadding + 4.0f + offset, panel.right - 2.0f,
                        panel.top + kPadding + 4.0f + offset + thumb),
            theme::White(0.22f), 1.5f);
  }
  return choice;
}

}
