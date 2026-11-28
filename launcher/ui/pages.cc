// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include <windows.h>
#include <shlwapi.h>
#undef PathIsDirectory

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <set>

#include "core/process.h"
#include "core/util.h"
#include "core/i18n.h"
#include "ui/app.h"
#include "ui/theme.h"

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif

namespace elixir::launcher::ui {

namespace {

constexpr wchar_t kVersion[] = L"" ELIXIR_VERSION;

std::wstring FormatPercent(double value) {
  wchar_t text[16];
  std::swprintf(text, 16, L"%.0f%%", value * 100.0);
  return text;
}

}

float App::PageHeader(Ui& ui, float x, float y, float width, std::wstring_view title, std::wstring_view subtitle) {
  const float height = ui.Text(x, y, width, title, TextStyle::kTitle, theme::kText);
  ui.Label(D2D1::RectF(x, y, x + width, y + height), title);
  y += height + 4.0f;
  if (!subtitle.empty()) y += ui.Text(x, y, width, subtitle, TextStyle::kBody, theme::kTextSecondary);
  return y + 22.0f;
}

float App::Banner(Ui& ui, float x, float y, float width, int status, std::wstring_view title, std::wstring_view body,
                  float trailing) {
  const D2D1_COLOR_F fill = status == 2   ? theme::kFailFill
                            : status == 1 ? theme::kWarnFill
                            : status == 0 ? theme::kPassFill
                                          : theme::kInfoFill;
  const int card = ui.BeginCard(x, y, width, fill);
  const float text_x = x + theme::kCardPadding;
  const float text_width = std::max(120.0f, width - 2.0f * theme::kCardPadding - trailing);
  float cy = y + 16.0f;
  cy += ui.Text(text_x, cy, text_width, title, TextStyle::kBodyStrong, theme::kText);
  if (!body.empty()) cy += 3.0f + ui.Text(text_x, cy + 3.0f, text_width, body, TextStyle::kSecondary, theme::kTextSecondary);
  ui.Label(D2D1::RectF(x, y, x + width, cy), std::wstring(title) + L". " + std::wstring(body));
  return ui.EndCard(card, cy - 4.0f);
}

float App::SectionTitle(Ui& ui, float x, float y, float width, std::wstring_view title) {
  const float height = ui.Text(x, y, width, title, TextStyle::kSection, theme::kText);
  ui.Label(D2D1::RectF(x, y, x + width, y + height), title);
  return y + height + 10.0f;
}

float App::SettingRow(Ui& ui, float x, float y, float width, std::wstring_view title, std::wstring_view description,
                      float control_width, float control_height, D2D1_RECT_F* control, bool first) {
  const int option_card =
      option_cards_ ? ui.BeginCard(x - theme::kCardPadding, y, width + 2.0f * theme::kCardPadding) : -1;
  if (!first && !option_cards_) ui.Fill(D2D1::RectF(x, y, x + width, y + 1.0f), theme::kDivider);
  const bool stacked = control_width > width * 0.5f;
  const float text_width = stacked ? width : width - control_width - 28.0f;
  float top = y + 15.0f;
  if (description.empty() && !stacked) {
    const float alone = ui.TextHeight(title, TextStyle::kBody, text_width);
    top = y + (std::max(alone, control_height) + 30.0f - alone) / 2.0f;
  }
  const float title_height = ui.Text(x, top, text_width, title, TextStyle::kBody, theme::kText);
  float text_bottom = top + title_height;
  if (!description.empty()) {
    text_bottom += 2.0f + ui.Text(x, text_bottom + 2.0f, text_width, description, TextStyle::kCaption,
                                  theme::kTextTertiary);
  }
  float bottom = 0.0f;
  if (stacked) {
    *control = D2D1::RectF(x, text_bottom + 10.0f, x + control_width, text_bottom + 10.0f + control_height);
    bottom = control->bottom + 15.0f;
  } else {
    const float row_height = std::max(text_bottom + 15.0f - y, control_height + 30.0f);
    const float center = y + row_height / 2.0f;
    *control = D2D1::RectF(x + width - control_width, center - control_height / 2.0f, x + width,
                           center + control_height / 2.0f);
    bottom = y + row_height;
  }
  if (!option_cards_) return bottom;
  ui.EndCard(option_card, bottom - theme::kCardPadding);
  return bottom + theme::kOptionGap;
}

void App::BeginOptions() { option_cards_ = true; }

float App::EndOptions(float y) {
  option_cards_ = false;
  return y - theme::kOptionGap;
}

float App::KeyValue(Ui& ui, float x, float y, float width, std::wstring_view key, std::wstring_view value,
                    TextStyle style, D2D1_COLOR_F color, float key_width) {
  const float key_height = ui.Text(x, y, key_width - 12.0f, key, TextStyle::kSecondary, theme::kTextSecondary);
  const float value_height =
      ui.Text(x + key_width, y + (style == TextStyle::kMono ? 1.0f : 0.0f), width - key_width,
              value.empty() ? std::wstring_view(L"—") : value, style, color);
  const float height = std::max(key_height, value_height);
  ui.Label(D2D1::RectF(x, y, x + width, y + height), std::wstring(key) + L": " + std::wstring(value));
  return y + height + 8.0f;
}

void App::BuildFrame(Ui& ui) {
  float width = frame_size_override_.width;
  float height = frame_size_override_.height;
  if (width <= 0.0f || height <= 0.0f) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    width = static_cast<float>(client.right) / scale();
    height = static_cast<float>(client.bottom) / scale();
  }
  ui.SetInert(overlay_.open);
  if (ui.painting()) {
    if (page_ != shown_page_) {
      shown_page_ = page_;
      ++page_epoch_;
    }
    if (toast_ != shown_toast_) {
      leaving_toast_ = toast_.empty() ? shown_toast_ : std::wstring();
      if (toast_.empty()) ++toast_out_epoch_;
      shown_toast_ = toast_;
      ++toast_epoch_;
    }
    const std::wstring dialog = overlay_.open ? overlay_.id + L"\n" + overlay_.title : std::wstring();
    if (dialog != shown_dialog_) {
      shown_dialog_ = dialog;
      ++dialog_epoch_;
    }
  }
  if (options_.mode != AppMode::kLauncher) {
    if (options_.mode == AppMode::kInstaller) {
      BuildInstallerFrame(ui, width, height);
    } else {
      BuildPromptFrame(ui, width, height);
    }
    BuildOverlays(ui, width, height);
    return;
  }
  BuildChrome(ui, width, height);

  viewport_ = D2D1::RectF(theme::kSidebarWidth, theme::kHeaderHeight, width, height - theme::kBottomBarHeight);
  const float viewport_height = viewport_.bottom - viewport_.top;
  float& scroll = scroll_[static_cast<int>(page_)];
  float& content_height = content_height_[static_cast<int>(page_)];
  const float max_scroll = std::max(0.0f, content_height - viewport_height);
  scroll = std::clamp(scroll, 0.0f, max_scroll);
  float& scroll_target = scroll_target_[static_cast<int>(page_)];
  scroll_target = std::clamp(scroll_target, 0.0f, max_scroll);
  const float content_width =
      std::max(320.0f, std::min(width - theme::kSidebarWidth - 2.0f * theme::kPagePaddingX, theme::kContentMaxWidth));
  const float x = theme::kSidebarWidth + theme::kPagePaddingX;
  const float top = theme::kHeaderHeight + theme::kPagePaddingTop - scroll;
  ui.PushClip(D2D1::RectF(viewport_.left + 1.0f, viewport_.top + 1.0f, viewport_.right, viewport_.bottom));
  const float page_shown = ui.Animate(L"page." + std::to_wstring(page_epoch_), 1.0f, theme::kPageTransitionMs, 0.0f,
                                      Ease::kWpfDecelerate);
  ui.PushLayer(page_shown, (1.0f - page_shown) * theme::kPageSlide);
  const float bottom = BuildPage(ui, x, top, content_width);
  ui.PopLayer();
  ui.PopClip();
  BuildBottomBar(ui, width, height);
  BuildSearchResults(ui);

  if (ui.painting()) {
    content_height = bottom - top + theme::kPagePaddingTop + 36.0f;
    if (content_height > viewport_height + 1.0f) {
      const float track = viewport_height - 16.0f;
      const float thumb = std::max(36.0f, track * viewport_height / content_height);
      const float max_scroll = content_height - viewport_height;
      const float offset = (track - thumb) * (max_scroll > 0 ? scroll / max_scroll : 0.0f);
      ui.Fill(D2D1::RectF(width - 7.0f, viewport_.top + 8.0f + offset, width - 3.0f, viewport_.top + 8.0f + offset + thumb),
              theme::White(0.22f), 2.0f);
    }
    const auto draw_toast = [&](const std::wstring& text, float shown) {
      const float text_width = std::min(ui.TextWidth(text, TextStyle::kBody), 520.0f);
      const float center = (viewport_.left + viewport_.right) / 2.0f;
      const D2D1_RECT_F rect =
          D2D1::RectF(center - text_width / 2.0f - 20.0f, viewport_.bottom - 56.0f, center + text_width / 2.0f + 20.0f,
                      viewport_.bottom - 16.0f);
      ui.PushLayer(shown, (1.0f - shown) * theme::kPopupRise);
      ui.Fill(rect, theme::kToast, 6.0f);
      ui.Stroke(rect, theme::White(0.10f), 6.0f);
      ui.TextLine(D2D1::RectF(rect.left + 20.0f, rect.top, rect.right - 20.0f, rect.bottom), text, TextStyle::kBody,
                  theme::kText);
      ui.PopLayer();
      return rect;
    };
    if (!toast_.empty()) {
      ui.Label(draw_toast(toast_, ui.Animate(L"toast." + std::to_wstring(toast_epoch_), 1.0f, theme::kPopupMs, 0.0f)),
               toast_);
    } else if (!leaving_toast_.empty()) {
      const float left =
          ui.Animate(L"toast.out." + std::to_wstring(toast_out_epoch_), 0.0f, theme::kPopupMs, 1.0f);
      if (left <= 0.001f) {
        leaving_toast_.clear();
      } else {
        draw_toast(leaving_toast_, left);
      }
    }
  }
  BuildOverlays(ui, width, height);
}

void App::BuildOverlays(Ui& ui, float width, float height) {
  if (overlay_.open) {
    ui.SetInert(false);
    BuildOverlay(ui, width, height, overlay_,
                 ui.Animate(L"dialog." + std::to_wstring(dialog_epoch_), 1.0f, theme::kPopupMs, 0.0f));
    return;
  }
  if (!ui.painting() || !closing_overlay_.open) return;
  const float left = ui.Animate(L"dialog.out." + std::to_wstring(dialog_out_epoch_), 0.0f, theme::kPopupMs, 1.0f);
  if (left <= 0.001f) {
    closing_overlay_ = Overlay{};
    return;
  }
  const bool inert = ui.inert();
  ui.SetInert(true);
  BuildOverlay(ui, width, height, closing_overlay_, left);
  ui.SetInert(inert);
}

void App::BuildChrome(Ui& ui, float width, float height) {
  if (mica_) ui.Fill(D2D1::RectF(0, 0, width, height), theme::kMicaTint);

  const float logo_height = 34.0f;
  const float logo_width = std::round(logo_height * 886.0f / 320.0f);
  const D2D1_RECT_F logo = D2D1::RectF(22.0f, (theme::kHeaderHeight - logo_height) / 2.0f, 22.0f + logo_width,
                                       (theme::kHeaderHeight + logo_height) / 2.0f);
  ui.Logo(logo);
  ui.Label(logo, L"Elixir");

  if (paths_.portable) {
    const std::wstring& label = T(L"Portable");
    const float right = width - 20.0f;
    const float text_width = ui.TextWidth(label, TextStyle::kCaption);
    const D2D1_RECT_F rect = D2D1::RectF(right - text_width - 22.0f, 20.0f, right, 44.0f);
    ui.Fill(rect, theme::kInfoFill, 12.0f);
    ui.Stroke(rect, theme::kInfoBorder, 12.0f);
    ui.TextLine(rect, label, TextStyle::kCaption, theme::kText, Align::kCenter);
    ui.Label(rect, label);
  }

  struct Item {
    Page page;
    const wchar_t* shortcut;
  };
  const Item main_items[] = {{Page::kActivity, L""},
                             {Page::kInstall, L"Ctrl+1"},  {Page::kMods, L"Ctrl+9"},
                             {Page::kShaders, L""},
                             {Page::kFastFlags, L"Ctrl+2"}, {Page::kGlobalSettings, L"Ctrl+8"},
                             {Page::kIntegrations, L"Ctrl+3"}, {Page::kShortcuts, L"Ctrl+0"},
                             {Page::kSettings, L"Ctrl+4"}, {Page::kCrashLogs, L"Ctrl+5"}};
  const Item footer_items[] = {{Page::kUpdates, L"Ctrl+6"}, {Page::kAbout, L"Ctrl+7"}};
  const float left = 10.0f;
  const float right = theme::kSidebarWidth - 10.0f;

  const D2D1_RECT_F search = D2D1::RectF(left, theme::kHeaderHeight + 2.0f, right,
                                         theme::kHeaderHeight + 2.0f + theme::kSearchHeight);
  if (ui.TextField(L"nav.search", search, &search_text_, T(L"Search settings"), T(L"Search settings"))) {
    UpdateSearch(search_text_);
  }
  if (!ui.painting() && search_enter_) {
    search_enter_ = false;
    UpdateSearch(search_text_);
    if (!search_hits_.empty()) Defer([this] { GoToSearchHit(0); });
  }
  if (state_.editing != L"nav.search") {
    ui.Glyph(search.right - 18.0f, (search.top + search.bottom) / 2.0f, glyph::kSearch, TextStyle::kIconSmall,
             theme::kTextSecondary);
  }

  constexpr int kNavItems = static_cast<int>(std::size(main_items) + std::size(footer_items));
  const float nav_top = search.bottom + 10.0f;
  const float available = height - nav_top - 16.0f - 6.0f - 12.0f;
  const float pitch = std::clamp(std::floor(available / static_cast<float>(kNavItems)), 26.0f,
                                 theme::kNavItemHeight + 4.0f);
  const float item_height = pitch - 4.0f;
  const float footer_top =
      std::max(nav_top + pitch * static_cast<float>(std::size(main_items)) + 16.0f, height - 12.0f - 2.0f * pitch);
  if (ui.painting()) {
    float selected_top = -1.0f;
    for (std::size_t i = 0; i < std::size(main_items); ++i) {
      if (main_items[i].page == page_) selected_top = nav_top + pitch * static_cast<float>(i);
    }
    for (std::size_t i = 0; i < std::size(footer_items); ++i) {
      if (footer_items[i].page == page_) selected_top = footer_top + pitch * static_cast<float>(i);
    }
    if (selected_top >= 0.0f) {
      const float shown_top = ui.Animate(L"nav.selection", selected_top, theme::kNavSlideMs,
                                         std::numeric_limits<float>::quiet_NaN(), Ease::kWpfDecelerate);
      ui.Fill(D2D1::RectF(left, shown_top, right, shown_top + item_height), theme::kNavSelected,
              theme::kControlRadius);
    }
  }
  const auto nav = [&](const Item& item, float y) {
    const D2D1_RECT_F rect = D2D1::RectF(left, y, right, y + item_height);
    const std::wstring id = L"nav." + Utf8ToWide(PageKey(item.page));
    if (ui.Nav(id, rect, PageTitle(item.page), page_ == item.page, item.shortcut, false)) {
      ClearSearch();
      Navigate(item.page);
    }
    if (item.page == Page::kCrashLogs && !crash_.entries.empty() && ui.painting()) {
      std::set<std::wstring> stems;
      for (const CrashLogEntry& entry : crash_.entries) stems.insert(entry.path.substr(0, entry.path.size() - 4));
      const std::wstring count = std::to_wstring(stems.size());
      const float badge_width = std::max(22.0f, ui.TextWidth(count, TextStyle::kCaption) + 14.0f);
      const float center = (rect.top + rect.bottom) / 2.0f;
      const D2D1_RECT_F badge = D2D1::RectF(rect.right - 10.0f - badge_width, center - 10.0f, rect.right - 10.0f,
                                            center + 10.0f);
      ui.Fill(badge, theme::White(0.10f), 10.0f);
      ui.TextLine(badge, count, TextStyle::kCaption, theme::kTextSecondary, Align::kCenter);
    }
  };
  float y = nav_top;
  for (const Item& item : main_items) {
    nav(item, y);
    y += pitch;
  }
  float footer_y = footer_top;
  ui.Fill(D2D1::RectF(left + 12.0f, footer_y - 8.0f, right - 12.0f, footer_y - 7.0f), theme::kDivider);
  for (const Item& item : footer_items) {
    nav(item, footer_y);
    footer_y += pitch;
  }
}

void App::BuildBottomBar(Ui& ui, float width, float height) {
  const float top = height - theme::kBottomBarHeight;
  const float left = theme::kSidebarWidth;
  ui.Fill(D2D1::RectF(left, top, width, top + 1.0f), theme::kDivider);
  const float center = top + theme::kBottomBarHeight / 2.0f;
  const float button_width = 148.0f;
  const D2D1_RECT_F button =
      D2D1::RectF(width - 24.0f - button_width, center - 18.0f, width - 24.0f, center + 18.0f);
  if (runtime_.running) {
    if (ui.Button(L"bar.stop", button, runtime_.stopping ? T(L"Stopping…") : T(L"Stop"), ButtonStyle::kDanger,
                  !runtime_.stopping)) {
      Defer([this] { StopRuntime(); });
    }
    const float window_width = ui.ButtonWidth(T(L"New instance"), glyph::kAdd) + 8.0f;
    const D2D1_RECT_F another = D2D1::RectF(button.left - 12.0f - window_width, button.top, button.left - 12.0f,
                                            button.bottom);
    if (ui.Button(kInstanceAccountControl, another, opening_window_ ? T(L"Opening…") : T(L"New instance"),
                  ButtonStyle::kStandard, !runtime_.stopping && !opening_window_, glyph::kAdd,
                  T(L"Opens another Roblox window with the account you pick."))) {
      Defer([this] { ChooseInstanceAccount(); });
    }
    if (MenuOpen(kInstanceAccountControl)) BuildInstanceAccountMenu(ui, another);
    return;
  }
  if (ui.Button(kPlayAccountControl, button, T(L"Play"), ButtonStyle::kStandard, true)) {
    Defer([this] { ChoosePlayAccount(); });
  }
  if (MenuOpen(kPlayAccountControl)) BuildInstanceAccountMenu(ui, button);
}

void App::BuildInstanceAccountMenu(Ui& ui, const D2D1_RECT_F& button) {
  constexpr float kRowHeight = 52.0f;
  constexpr float kLastRowHeight = 40.0f;
  constexpr float kPadding = 4.0f;
  constexpr float kPicture = 36.0f;
  constexpr float kTextLeft = 10.0f + kPicture + 12.0f;
  constexpr float kRemoveSpace = 40.0f;
  const std::wstring control = menu_control_;
  const int count = static_cast<int>(instance_accounts_.size());
  constexpr float kStateGap = 7.0f;
  constexpr float kSmallestText = 0.72f;
  const auto detail_of = [](const InstanceAccountRow& row) {
    std::wstring detail = L"@" + row.account.username;
    if (row.use == AccountUse::kInGame) detail += L"  \u00B7  " + T(L"In game");
    if (row.use == AccountUse::kOpen) detail += L"  \u00B7  " + T(L"Open##account");
    return detail;
  };
  const auto state_of = [](const InstanceAccountRow& row) {
    return row.use == AccountUse::kInGame ? T(L"In game")
                                          : row.use == AccountUse::kOpen ? T(L"Open##account") : T(L"");
  };
  const auto detail_width = [&](const InstanceAccountRow& row) {
    const std::wstring handle = L"@" + row.account.username;
    if (row.use == AccountUse::kNotOpen) return ui.TextWidth(handle, TextStyle::kCaption);
    return ui.TextWidth(handle + L"  \u00B7", TextStyle::kCaption) + kStateGap +
           ui.TextWidth(state_of(row), TextStyle::kCaption);
  };
  float widest = ui.TextWidth(T(L"Add account"), TextStyle::kBody);
  for (const InstanceAccountRow& row : instance_accounts_) {
    widest = std::max(widest, ui.TextWidth(SavedAccountName(row.account), TextStyle::kBody));
    widest = std::max(widest, detail_width(row));
  }
  widest += 2.0f;
  const float menu_width = std::clamp(widest + kTextLeft + kRemoveSpace + 2.0f * kPadding, 260.0f, 420.0f);
  const float menu_height = kRowHeight * static_cast<float>(count) + kLastRowHeight + 2.0f * kPadding;
  const float bottom = std::min(button.top - kPadding, viewport_.bottom);
  const float top = std::max(viewport_.top + kPadding, bottom - menu_height);
  const D2D1_RECT_F panel = D2D1::RectF(button.right - menu_width, top, button.right, top + menu_height);
  const float menu_shown = ui.Animate(L"menu:" + control, 1.0f, theme::kPopupMs, 0.0f);
  ui.PushLayer(menu_shown, (1.0f - menu_shown) * theme::kMenuSlide);
  ui.Fill(panel, theme::kDialog, theme::kControlRadius);
  ui.Stroke(panel, theme::kControlBorder, theme::kControlRadius);
  for (int i = 0; i < count; ++i) {
    const InstanceAccountRow& row = instance_accounts_[static_cast<std::size_t>(i)];
    const float row_top = top + kPadding + static_cast<float>(i) * kRowHeight;
    const D2D1_RECT_F rect = D2D1::RectF(panel.left + kPadding, row_top, panel.right - kPadding, row_top + kRowHeight);
    const std::wstring name = SavedAccountName(row.account);
    const std::wstring detail = detail_of(row);
    if (ui.MenuRow(control + L"#" + std::to_wstring(i), rect, name + L", " + detail)) {
      Defer([this, i] { StartInstanceWith(i); });
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
      const std::wstring state = state_of(row);
      const float before_width = ui.TextWidth(before, TextStyle::kCaption);
      const float state_width = ui.TextWidth(state, TextStyle::kCaption);
      const float available = text_right - text_left;
      const float needed = before_width + kStateGap + state_width + 1.0f;
      const float scale = needed > available ? std::max(kSmallestText, available / needed) : 1.0f;
      const float state_room = std::min(available, state_width * scale + 1.0f);
      const float before_room = std::max(1.0f, std::min(before_width * scale + 1.0f,
                                                        available - state_room - kStateGap * scale));
      ui.TextLine(D2D1::RectF(text_left, detail_rect.top, text_left + before_room, detail_rect.bottom), before,
                  TextStyle::kCaption, theme::kTextSecondary, Align::kLeading, kSmallestText);
      const float state_left = text_left + before_room + kStateGap * scale;
      ui.TextLine(D2D1::RectF(state_left, detail_rect.top, std::min(text_right, state_left + state_room),
                              detail_rect.bottom),
                  state, TextStyle::kCaption,
                  row.use == AccountUse::kInGame ? theme::Rgb(state_.control_accent_rgb) : theme::kTextSecondary,
                  Align::kLeading, kSmallestText);
    }
    const D2D1_RECT_F remove = D2D1::RectF(rect.right - 34.0f, center - 14.0f, rect.right - 6.0f, center + 14.0f);
    if (ui.IconButton(control + L"#remove." + std::to_wstring(i), remove, glyph::kCancel,
                      TF(L"Remove {0}", SavedAccountLabel(row.account)))) {
      Defer([this, i] { RemoveInstanceAccount(i); });
    }
  }
  const float last_top = top + kPadding + static_cast<float>(count) * kRowHeight;
  const D2D1_RECT_F last =
      D2D1::RectF(panel.left + kPadding, last_top, panel.right - kPadding, last_top + kLastRowHeight);
  if (ui.MenuRow(control + L"#" + std::to_wstring(count), last, T(L"Add account"))) {
    Defer([this, count] { StartInstanceWith(count); });
  }
  ui.Glyph(last.left + 10.0f + kPicture / 2.0f, (last.top + last.bottom) / 2.0f, glyph::kAdd, TextStyle::kIconSmall,
           theme::kTextSecondary);
  ui.TextLine(D2D1::RectF(last.left + kTextLeft, last.top, last.right - 10.0f, last.bottom), T(L"Add account"),
              TextStyle::kBody, theme::kText);
  ui.PopLayer();
}

namespace {

struct SearchEntry {
  Page page;
  const wchar_t* title;
  const wchar_t* words;
  const wchar_t* anchor;
};

constexpr SearchEntry kSearchIndex[] = {
    {Page::kInstall, ELIXIR_TR(L"Installation"), L"install roblox download version versions latest older", L"versions.install"},
    {Page::kInstall, ELIXIR_TR(L"Install from a file"), L"apk apkm xapk package file choose drop manual", L"install.choose"},
    {Page::kInstall, ELIXIR_TR(L"Roblox version check"), L"update updates check latest version rollback previous version",
     L"roblox.update.check"},
    {Page::kFastFlags, ELIXIR_TR(L"Fast Flags"), L"fflags flags fast flag editor add rename delete", L""},
    {Page::kFastFlags, ELIXIR_TR(L"Graphics settings"),
     L"mesh detail anti-aliasing antialiasing msaa display scaling dpi graphics quality grass gray sky lighting",
     L"fflags.preset."},
    {Page::kFastFlags, ELIXIR_TR(L"Fast Flag Editor"), L"editor import export json add delete flags fflags", L"fflags.open"},
    {Page::kGlobalSettings, ELIXIR_TR(L"Global Settings"), L"roblox game preferences global basic settings", L""},
    {Page::kGlobalSettings, ELIXIR_TR(L"Read-only settings file"), L"global settings lock protect save", L"global.readonly"},
    {Page::kGlobalSettings, ELIXIR_TR(L"Graphics quality"), L"global settings automatic saved quality level", L"global.quality"},
    {Page::kGlobalSettings, ELIXIR_TR(L"Framerate cap"),
     L"global settings frame rate framerate fps limit cap unlock 60 120 240", L"global.fps"},
    {Page::kGlobalSettings, ELIXIR_TR(L"Background transparency"), L"global settings interface accessibility opacity", L"global.transparency"},
    {Page::kGlobalSettings, ELIXIR_TR(L"Reduced motion"), L"global settings interface accessibility animations", L"global.motion"},
    {Page::kGlobalSettings, ELIXIR_TR(L"Text size"), L"global settings interface accessibility font size", L"global.textsize"},
    {Page::kGlobalSettings, ELIXIR_TR(L"Mouse sensitivity"), L"global settings input camera speed", L"global.sensitivity"},
    {Page::kIntegrations, ELIXIR_TR(L"Discord Rich Presence"), L"discord rich presence activity join button",
     L"integrations.discord"},
    {Page::kSettings, ELIXIR_TR(L"Language"), L"language locale translation english", L"settings.language"},
    {Page::kSettings, ELIXIR_TR(L"Resolution scale"), L"resolution scale render sharpness graphics quality", L"settings.scale"},
    {Page::kSettings, ELIXIR_TR(L"Stretch"), L"stretch stretched aspect ratio wide resolution", L"settings.stretch"},
    {Page::kSettings, ELIXIR_TR(L"Audio output device"), L"audio sound speakers headphones output device",
     L"settings.audio.default"},
    {Page::kSettings, ELIXIR_TR(L"Audio input device"), L"audio microphone voice chat input device",
     L"settings.audio.input.default"},
    {Page::kSettings, ELIXIR_TR(L"Data folder and caches"), L"storage data folder cache caches clear", L"settings.data.open"},
    {Page::kCrashLogs, ELIXIR_TR(L"Crash logs"), L"crash crashes report logs", L""},
    {Page::kUpdates, ELIXIR_TR(L"Elixir updates"), L"update updates version elixir automatic", L"updates.check"},
    {Page::kUpdates, ELIXIR_TR(L"Version history"), L"versions releases changes history notes changelog", L"updates.history"},
    {Page::kAbout, ELIXIR_TR(L"About Elixir"), L"about version license licenses notices credits contributors thanks", L""},
    {Page::kMods, ELIXIR_TR(L"Mods"), L"mods modifications folder textures sounds fonts replace files", L"mods.open"},
    {Page::kMods, ELIXIR_TR(L"Custom font"), L"mods font custom text typeface", L"mods.font.choose"},
    {Page::kShortcuts, ELIXIR_TR(L"Shortcuts"), L"shortcut shortcuts desktop start menu icon", L"shortcuts.desktop"},
    {Page::kShortcuts, ELIXIR_TR(L"Game shortcuts"), L"shortcut game experience place id desktop", L"shortcuts.game.name"},
    {Page::kActivity, ELIXIR_TR(L"Activity"), L"activity history recent games servers rejoin played", L"activity.history"},
    {Page::kShaders, ELIXIR_TR(L"Shaders"), L"shaders glsl effects bloom vignette fxaa color import", L"shaders.enabled"},
    {Page::kShaders, ELIXIR_TR(L"Upscaling"), L"upscaling upscale sharpness sharpen dynamic target fps", L"shaders.upscaling"},
    {Page::kShaders, ELIXIR_TR(L"Render scale"), L"render size smaller picture", L"shaders.scale"},
};

std::wstring Lower(std::wstring_view text) {
  std::wstring out(text);
  if (!out.empty()) CharLowerBuffW(out.data(), static_cast<DWORD>(out.size()));
  return out;
}

}

void App::UpdateSearch(const std::wstring& query) {
  search_hits_.clear();
  std::vector<std::wstring> words;
  const std::wstring lowered = Lower(query);
  std::size_t start = 0;
  while (start < lowered.size()) {
    const std::size_t end = lowered.find(L' ', start);
    const std::wstring word = lowered.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
    if (!word.empty()) words.push_back(word);
    if (end == std::wstring::npos) break;
    start = end + 1;
  }
  if (words.empty()) return;
  std::vector<std::pair<int, int>> ranked;
  for (int i = 0; i < static_cast<int>(std::size(kSearchIndex)); ++i) {
    const std::wstring title = Lower(T(kSearchIndex[i].title));
    const std::wstring haystack = title + L" " + Lower(kSearchIndex[i].title) + L" " + kSearchIndex[i].words;
    bool all = true;
    for (const std::wstring& word : words) all &= haystack.find(word) != std::wstring::npos;
    if (!all) continue;
    const int rank = title.rfind(words.front(), 0) == 0 ? 0 : title.find(words.front()) != std::wstring::npos ? 1 : 2;
    ranked.push_back({rank, i});
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const auto& a, const auto& b) { return a.first < b.first; });
  for (const auto& [rank, index] : ranked) {
    if (search_hits_.size() == 8) break;
    search_hits_.push_back(index);
  }
}

void App::ClearSearch() {
  search_text_.clear();
  search_hits_.clear();
  search_enter_ = false;
}

void App::GoToSearchHit(int position) {
  if (position < 0 || position >= static_cast<int>(search_hits_.size())) return;
  const SearchEntry& entry = kSearchIndex[search_hits_[static_cast<std::size_t>(position)]];
  ClearSearch();
  if (!state_.editing.empty()) EndEdit(false);
  Navigate(entry.page);
  if (page_ == entry.page) search_anchor_ = entry.anchor;
  Invalidate();
}

void App::BuildSearchResults(Ui& ui) {
  const bool searching = state_.editing == L"nav.search" || !search_text_.empty();
  if (!searching || TrimWide(search_text_).empty()) return;
  const float left = 10.0f;
  const float top = theme::kHeaderHeight + 2.0f + theme::kSearchHeight + 4.0f;
  const float width = 380.0f;
  const float row = 48.0f;
  const float rows_height = search_hits_.empty() ? 44.0f : row * static_cast<float>(search_hits_.size());
  const D2D1_RECT_F panel = D2D1::RectF(left, top, left + width, top + rows_height + 8.0f);
  const float shown = ui.Animate(L"menu:search", 1.0f, theme::kPopupMs, 0.0f);
  ui.PushLayer(shown, (shown - 1.0f) * theme::kMenuSlide);
  ui.Fill(D2D1::RectF(panel.left + 2.0f, panel.top + 4.0f, panel.right + 2.0f, panel.bottom + 6.0f),
          theme::Rgb(0x000000, 0.35f), theme::kCardRadius);
  ui.Fill(panel, theme::kDialog, theme::kCardRadius);
  ui.Stroke(panel, theme::White(0.10f), theme::kCardRadius);
  if (search_hits_.empty()) {
    const D2D1_RECT_F line = D2D1::RectF(panel.left + 16.0f, panel.top + 4.0f, panel.right - 16.0f, panel.bottom - 4.0f);
    ui.TextLine(line, TF(L"No page or setting matches “{0}”.", TrimWide(search_text_)), TextStyle::kSecondary,
                theme::kTextSecondary);
    ui.Label(line, T(L"No page or setting matches"));
    ui.PopLayer();
    return;
  }
  constexpr int kSearchGroup = 9301;
  for (std::size_t i = 0; i < search_hits_.size(); ++i) {
    const SearchEntry& entry = kSearchIndex[search_hits_[i]];
    const D2D1_RECT_F rect = D2D1::RectF(panel.left + 4.0f, panel.top + 4.0f + row * static_cast<float>(i),
                                         panel.right - 4.0f, panel.top + 4.0f + row * static_cast<float>(i + 1));
    if (ui.ListItem(L"search.hit#" + std::to_wstring(i), rect, false, T(entry.title), PageTitle(entry.page), L"",
                    glyph::kChevronRight, kSearchGroup)) {
      const int position = static_cast<int>(i);
      Defer([this, position] { GoToSearchHit(position); });
    }
  }
  ui.PopLayer();
}

float App::BuildPage(Ui& ui, float x, float y, float width) {
  switch (page_) {
    case Page::kInstall: return BuildInstall(ui, x, y, width);
    case Page::kFastFlags: return BuildFastFlags(ui, x, y, width);
    case Page::kGlobalSettings: return BuildGlobalSettings(ui, x, y, width);
    case Page::kIntegrations: return BuildIntegrations(ui, x, y, width);
    case Page::kSettings: return BuildSettings(ui, x, y, width);
    case Page::kCrashLogs: return BuildCrashLogs(ui, x, y, width);
    case Page::kUpdates: return BuildUpdates(ui, x, y, width);
    case Page::kAbout: return BuildAbout(ui, x, y, width);
    case Page::kMods: return BuildMods(ui, x, y, width);
    case Page::kShortcuts: return BuildShortcuts(ui, x, y, width);
    case Page::kActivity: return BuildActivity(ui, x, y, width);
    case Page::kShaders: return BuildShaders(ui, x, y, width);
  }
  return y;
}

float App::BuildInstall(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, T(L"Installation"), L"");
  const bool available = apk_.configured && apk_.found;
  const bool working =
      install_.stage == InstallFlow::Stage::kValidating || install_.stage == InstallFlow::Stage::kInstalling;
  if (!available) {
    y = Banner(ui, x, y, width, 1, T(L"Elixir can’t install Roblox"),
               T(L"Some of Elixir’s files are missing. Reinstall Elixir to fix this.")) +
        16.0f;
  }

  y = BuildRobloxVersions(ui, x, y, width) + 24.0f;

  y = SectionTitle(ui, x, y, width, T(L"Install from a file"));
  int card = ui.BeginCard(x, y, width);
  float cx = x + theme::kCardPadding;
  float cw = width - 2.0f * theme::kCardPadding;
  float cy = y + 18.0f;
  const float choose_width = ui.ButtonWidth(T(L"Choose file…"), glyph::kOpenFile);
  const bool other_work = cache_.clearing || switching_;
  if (ui.Button(L"install.choose", D2D1::RectF(cx, cy, cx + choose_width, cy + theme::kButtonHeight),
                T(L"Choose file…"), ButtonStyle::kStandard, available && !working && !other_work, glyph::kOpenFile,
                T(L"Choose a Roblox APK, APKM or XAPK from this PC"))) {
    Defer([this] { ChooseApk(); });
  }
  {
    const bool beside = cw - choose_width - 16.0f >= 260.0f;
    const float info_x = beside ? cx + choose_width + 16.0f : cx;
    const float info_width = beside ? cw - choose_width - 16.0f : cw;
    if (!beside) cy += theme::kButtonHeight + 10.0f;
    const bool chosen = !install_.file.empty();
    const std::wstring info = chosen ? FileNameOf(install_.file) + L"  ·  " + FormatByteSize(install_.size)
                                     : std::wstring(T(L"An APK, APKM or XAPK with the x86_64 engine. You can drop it here too."));
    const TextStyle style = chosen ? TextStyle::kBodyStrong : TextStyle::kSecondary;
    const float info_height = ui.TextHeight(info, style, info_width);
    const float info_top = beside ? cy + std::max(0.0f, (theme::kButtonHeight - info_height) / 2.0f) : cy;
    ui.Text(info_x, info_top, info_width, info, style, chosen ? theme::kText : theme::kTextTertiary);
    if (!chosen) ui.Label(D2D1::RectF(info_x, info_top, info_x + info_width, info_top + info_height), info);
    cy += beside ? std::max(theme::kButtonHeight, info_height) : info_height;
  }
  y = ui.EndCard(card, cy) + 16.0f;

  if (working) {
    card = ui.BeginCard(x, y, width);
    cy = y + 18.0f;
    const std::wstring& title =
        install_.stage == InstallFlow::Stage::kValidating ? T(L"Checking the file…") : T(L"Installing Roblox…");
    cy += ui.Text(cx, cy, cw, title, TextStyle::kCardTitle, theme::kText) + 12.0f;
    ui.Progress(D2D1::RectF(cx, cy, cx + cw - 110.0f, cy + 4.0f),
                install_.progress.percent >= 0 ? static_cast<float>(install_.progress.percent) / 100.0f : -1.0f);
    if (ui.Button(L"install.cancel", D2D1::RectF(cx + cw - 92.0f, cy - 14.0f, cx + cw, cy + 18.0f), T(L"Cancel"),
                  ButtonStyle::kStandard)) {
      if (install_.cancel) *install_.cancel = true;
    }
    cy += 18.0f;
    ui.Label(D2D1::RectF(cx, y, cx + cw, cy), title);
    y = ui.EndCard(card, cy) + 16.0f;
  }

  const bool has_result = install_.stage != InstallFlow::Stage::kIdle &&
                          install_.stage != InstallFlow::Stage::kValidating;
  if (has_result) {
    const ApkResult& result = install_.validation;
    const ApkMetadata& metadata = result.metadata;
    card = ui.BeginCard(x, y, width);
    cy = y + 18.0f;
    cy += ui.Text(cx, cy, cw, T(L"File check"), TextStyle::kCardTitle, theme::kText) + 12.0f;
    if (!metadata.package.empty()) {
      cy = KeyValue(ui, cx, cy, cw, T(L"Version"), Utf8ToWide(metadata.version_name));
      if (metadata.signature_verified && !metadata.has_x86_64_engine) {
        cy += 4.0f + ui.Text(cx, cy + 4.0f, cw,
                             T(L"This file doesn’t contain the x86-64 Roblox engine. Choose a package that includes it."),
                             TextStyle::kSecondary, theme::kWarn);
      }
    }
    if (!result.ok && !result.component_missing) {
      cy += 4.0f;
      const float error_height = ui.Text(cx, cy, cw, result.error, TextStyle::kBodyStrong, theme::kFail);
      ui.Label(D2D1::RectF(cx, cy, cx + cw, cy + error_height), result.error);
      cy += error_height + 10.0f;
    }
    if (install_.stage == InstallFlow::Stage::kValidated) {
      cy += 4.0f;
      if (ui.Button(L"install.install", D2D1::RectF(cx, cy, cx + 140.0f, cy + 36.0f), T(L"Install"),
                    ButtonStyle::kStandard, metadata.has_x86_64_engine && !other_work, glyph::kDownload)) {
        Defer([this] { StartInstall(); });
      }
      cy += 36.0f;
    } else if (install_.stage == InstallFlow::Stage::kRejected) {
      const float retry_width = ui.ButtonWidth(T(L"Try again"), glyph::kRefresh);
      if (ui.Button(L"install.retry", D2D1::RectF(cx, cy, cx + retry_width, cy + theme::kButtonHeight), T(L"Try again"),
                    ButtonStyle::kStandard, available && !other_work, glyph::kRefresh)) {
        Defer([this] { StartValidation(); });
      }
      cy += theme::kButtonHeight;
    }
    y = ui.EndCard(card, cy) + 16.0f;
  }

  if (install_.stage == InstallFlow::Stage::kInstalled) {
    const ApkMetadata& installed = install_.install.metadata;
    y = Banner(ui, x, y, width, 0, TF(L"Roblox {0} is installed", Utf8ToWide(installed.version_name)),
               T(L"Press Play to start."));
  } else if (install_.stage == InstallFlow::Stage::kInstallFailed) {
    y = Banner(ui, x, y, width, 2, T(L"Roblox couldn’t be installed"), install_.install.error);
  }
  return BuildRobloxUpdates(ui, x, y + 8.0f, width);
}

float App::BuildSettings(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, T(L"Settings"), L"");
  if (load_notice_) {
    const float top = y;
    const bool repaired = load_result_.outcome == LoadOutcome::kRecoveredFromBackup ||
                          load_result_.outcome == LoadOutcome::kResetToDefaults;
    y = Banner(ui, x, y, width, 1, repaired ? T(L"Your settings were repaired") : T(L"Your settings couldn’t be saved"),
               load_result_.message, 100.0f);
    if (ui.Button(L"settings.dismiss", D2D1::RectF(x + width - 104.0f, top + 12.0f, x + width - 16.0f, top + 42.0f),
                  T(L"Dismiss"), ButtonStyle::kSubtle)) {
      load_notice_ = false;
    }
    y += 16.0f;
  }
  if (!save_error_.empty()) y = Banner(ui, x, y, width, 2, T(L"Settings couldn’t be saved"), save_error_) + 16.0f;
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  D2D1_RECT_F control{};

  const std::vector<LocaleInfo>& locales = SupportedLocales();
  int language_index = 0;
  for (std::size_t i = 0; i < locales.size(); ++i) {
    if (config_.language == locales[i].code) language_index = static_cast<int>(i) + 1;
  }
  D2D1_RECT_F language_button{};
  BeginOptions();
  y = SettingRow(ui, cx, y, cw, T(L"Language"), T(L"Changes Elixir’s language, not Roblox’s."), std::min(240.0f, cw),
                 32.0f, &language_button, true);
  const bool language_open = MenuOpen(kLanguageControl);
  if (ui.ComboBox(kLanguageControl, language_button, T(L"Language"),
                  language_index == 0 ? T(L"System default") : std::wstring(locales[language_index - 1].name),
                  language_open, true)) {
    Defer([this, language_open, language_index, count = static_cast<int>(locales.size()) + 1] {
      SetMenu(kLanguageControl, language_open ? -1 : language_index, count);
    });
  }
  y = EndOptions(y) + 24.0f;

  y = SectionTitle(ui, x, y, width, T(L"Graphics"));
  BeginOptions();
  float cy = y;
  const float slider_value_width = std::max(56.0f, std::ceil(ui.TextWidth(T(L"Off"), TextStyle::kBody)) + 12.0f);
  {
    double resolution = config_.graphics.resolution_scale;
    cy = SettingRow(ui, cx, cy, cw, T(L"Resolution scale"),
                    T(L"Lower values give more FPS in games. Menus stay sharp."),
                    300.0f, 32.0f, &control, true);
    if (ui.Slider(L"settings.scale", control, &resolution, kResolutionScaleMin, kResolutionScaleMax, 0.05,
                  T(L"Resolution scale"), FormatPercent(resolution), true, slider_value_width)) {
      config_.graphics.resolution_scale = resolution;
      SaveConfig();
    }
  }
  {
    double stretch = config_.graphics.stretch;
    cy = SettingRow(ui, cx, cy, cw, T(L"Stretch"),
                    T(L"Renders games narrower and stretches them to fill the window."), 300.0f, 32.0f, &control,
                    false);
    if (ui.Slider(L"settings.stretch", control, &stretch, 0.0, kStretchMax, 0.05, T(L"Stretch"),
                  stretch < 0.005 ? std::wstring(T(L"Off")) : FormatPercent(stretch), true, slider_value_width)) {
      config_.graphics.stretch = stretch;
      SaveConfig();
    }
  }
  y = EndOptions(cy) + 24.0f;

  y = SectionTitle(ui, x, y, width, T(L"Audio"));
  const auto audio_device_card = [&](float top, const std::wstring& id, std::wstring_view title,
                                     std::wstring_view description, int group, const std::vector<AudioDevice>& devices,
                                     const std::wstring& error, std::string& configured_id,
                                     std::string& configured_name, bool input) {
    const int card = ui.BeginCard(x, top, width);
    float row_y = top + 18.0f;
    const float refresh_width = ui.ButtonWidth(T(L"Refresh"), glyph::kRefresh);
    const float title_height =
        ui.Text(cx, row_y, cw - refresh_width - 16.0f, title, TextStyle::kBody, theme::kText);
    const float description_height = ui.Text(cx, row_y + title_height + 2.0f, cw - refresh_width - 16.0f,
                                             description, TextStyle::kCaption, theme::kTextTertiary);
    if (ui.Button(id + L".refresh",
                  D2D1::RectF(cx + cw - refresh_width, row_y, cx + cw, row_y + theme::kButtonHeight), T(L"Refresh"),
                  ButtonStyle::kStandard, true, glyph::kRefresh)) {
      RefreshAudioDevices();
    }
    row_y += std::max(title_height + description_height + 2.0f, theme::kButtonHeight) + 10.0f;
    std::wstring default_name;
    for (const AudioDevice& device : devices) {
      if (device.is_default) default_name = device.name;
    }
    const bool system_default = configured_id.empty();
    if (ui.Radio(id + L".default", D2D1::RectF(cx - 8.0f, row_y, cx + cw, row_y + 36.0f), system_default,
                 T(L"System default"), default_name.empty() ? std::wstring() : TF(L"currently {0}", default_name),
                 group)) {
      configured_id.clear();
      configured_name.clear();
      SaveConfig();
    }
    row_y += 38.0f;
    bool configured_found = system_default;
    for (std::size_t i = 0; i < devices.size(); ++i) {
      const AudioDevice& device = devices[i];
      const std::string device_id = WideToUtf8(device.id);
      const bool selected = !system_default && device_id == configured_id;
      configured_found |= selected;
      if (ui.Radio(id + L"." + std::to_wstring(i), D2D1::RectF(cx - 8.0f, row_y, cx + cw, row_y + 36.0f), selected,
                   device.name, device.is_default ? T(L"Windows default") : T(L""), group)) {
        configured_id = device_id;
        configured_name = WideToUtf8(device.name);
        SaveConfig();
      }
      row_y += 38.0f;
    }
    if (!audio_loaded_) {
      row_y += ui.Text(cx, row_y + 4.0f, cw, T(L"Looking for audio devices…"), TextStyle::kSecondary,
                       theme::kTextTertiary) + 8.0f;
    } else if (!error.empty()) {
      row_y += ui.Text(cx, row_y + 4.0f, cw, error, TextStyle::kSecondary, theme::kWarn) + 8.0f;
    } else if (!configured_found) {
      const std::wstring name = configured_name.empty() ? T(L"The selected device") : Utf8ToWide(configured_name);
      const std::wstring notice =
          input ? TF(L"{0} isn’t connected. Microphone capture will be unavailable.", name)
                : TF(L"{0} isn’t connected, so the system default is used.", name);
      row_y += ui.Text(cx, row_y + 4.0f, cw, notice, TextStyle::kSecondary, theme::kWarn) + 8.0f;
    } else if (devices.empty()) {
      row_y += ui.Text(cx, row_y + 4.0f, cw, input ? T(L"No microphones found.") : T(L"No output devices found."),
                       TextStyle::kSecondary, theme::kWarn) + 8.0f;
    }
    return ui.EndCard(card, row_y - 6.0f);
  };
  y = audio_device_card(y, L"settings.audio", T(L"Output device"), T(L"Where Roblox plays sound."), 9001, audio_devices_,
                        audio_error_, config_.audio.output_device_id, config_.audio.output_device_name, false) +
      8.0f;
  y = audio_device_card(y, L"settings.audio.input", T(L"Input device"), T(L"Microphone for Roblox voice chat on next launch."),
                        9002, audio_input_devices_, audio_input_error_, config_.audio.input_device_id,
                        config_.audio.input_device_name, true) + 24.0f;

  y = SectionTitle(ui, x, y, width, T(L"Storage"));
  int card = ui.BeginCard(x, y, width);
  cy = y + 18.0f;
  {
    const float open_width = ui.ButtonWidth(T(L"Open folder"), glyph::kFolder);
    const float text_width = cw - open_width - 16.0f;
    cy += ui.Text(cx, cy, text_width, T(L"Data folder"), TextStyle::kBody, theme::kText) + 2.0f;
    cy += ui.Text(cx, cy, text_width, T(L"Your settings, Roblox installs and crash logs."), TextStyle::kCaption,
                  theme::kTextTertiary);
    if (ui.Button(L"settings.data.open",
                  D2D1::RectF(cx + cw - open_width, y + 18.0f, cx + cw, y + 18.0f + theme::kButtonHeight),
                  T(L"Open folder"), ButtonStyle::kStandard, true, glyph::kFolder)) {
      Defer([this] { OpenFolder(paths_.data_root); });
    }
    cy = std::max(cy, y + 18.0f + theme::kButtonHeight);
  }
  y = ui.EndCard(card, cy) + 16.0f;
  y = BuildCacheCard(ui, x, y, width);
  if (language_open) {
    std::vector<std::wstring> names{T(L"System default")};
    for (const LocaleInfo& locale : locales) names.emplace_back(locale.name);
    const int chosen = MenuPanel(ui, kLanguageControl, language_button, names, language_index);
    if (chosen >= 0) {
      Defer([this, chosen] {
        CloseMenu();
        ChooseLanguage(chosen);
      });
    }
  }
  return y;
}

float App::BuildCrashLogs(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, T(L"Crash logs"), L"");
  const bool has_selection = crash_.selected >= 0 && crash_.selected < static_cast<int>(crash_.entries.size());
  float bx = x;
  const auto toolbar_button = [&](const wchar_t* id, const std::wstring& label, wchar_t icon, bool enabled,
                                  ButtonStyle style = ButtonStyle::kStandard) {
    const float button_width = ui.ButtonWidth(label, icon);
    const bool pressed = ui.Button(id, D2D1::RectF(bx, y, bx + button_width, y + theme::kButtonHeight), label, style,
                                   enabled, icon);
    bx += button_width + 8.0f;
    return pressed;
  };
  if (toolbar_button(L"crash.refresh", T(L"Refresh"), glyph::kRefresh, true)) RefreshCrashLogs();
  if (toolbar_button(L"crash.folder", T(L"Open folder"), glyph::kFolder, true)) {
    Defer([this] { OpenFolder(paths_.crashes_dir); });
  }
  if (toolbar_button(L"crash.delete", T(L"Delete"), glyph::kDelete, has_selection, ButtonStyle::kDanger)) {
    Defer([this] { DeleteSelectedCrashLog(); });
  }
  y += theme::kButtonHeight + 20.0f;

  if (crash_.entries.empty()) {
    const int card = ui.BeginCard(x, y, width);
    float cy = y + 24.0f;
    ui.Glyph(x + 46.0f, cy + 22.0f, glyph::kCheck, TextStyle::kIconLarge, theme::kText);
    const float title_height = ui.Text(x + 84.0f, cy + 2.0f, width - 104.0f, T(L"No crash reports"), TextStyle::kBodyStrong, theme::kText);
    const float body_height = ui.Text(x + 84.0f, cy + 4.0f + title_height, width - 104.0f,
                                      T(L"If Roblox crashes, a report will show up here."), TextStyle::kSecondary,
                                      theme::kTextSecondary);
    ui.Label(D2D1::RectF(x, y, x + width, cy + title_height + body_height), T(L"No crash reports"));
    cy += std::max(44.0f, title_height + body_height + 6.0f);
    return ui.EndCard(card, cy + 4.0f);
  }

  const float list_width = std::clamp(std::floor(width * 0.4f), std::min(270.0f, std::floor(width * 0.5f)), 330.0f);
  const float viewer_x = x + list_width + 16.0f;
  const float viewer_width = width - list_width - 16.0f;
  const float available = viewport_.bottom - y - 28.0f;
  const float list_height = static_cast<float>(crash_.entries.size()) * 58.0f + 16.0f;
  const float panel_height = std::max({360.0f, available, list_height});

  int card = ui.BeginCard(x, y, list_width);
  float ly = y + 8.0f;
  constexpr int kCrashGroup = 9101;
  for (std::size_t i = 0; i < crash_.entries.size(); ++i) {
    const CrashLogEntry& entry = crash_.entries[i];
    const std::wstring subtitle =
        std::wstring(entry.is_dump ? T(L"Debug file") : T(L"Report")) + L"  ·  " + FormatByteSize(entry.size);
    if (ui.ListItem(L"crash.item." + std::to_wstring(i), D2D1::RectF(x + 6.0f, ly, x + list_width - 6.0f, ly + 54.0f),
                    static_cast<int>(i) == crash_.selected, FormatFileTimeLocal(entry.modified), subtitle, L"",
                    entry.is_dump ? glyph::kBug : glyph::kDocument, kCrashGroup)) {
      SelectCrashLog(static_cast<int>(i));
    }
    ly += 58.0f;
  }
  ui.EndCard(card, y + panel_height - theme::kCardPadding);

  card = ui.BeginCard(viewer_x, y, viewer_width, theme::kViewer);
  if (has_selection) {
    const CrashLogEntry& entry = crash_.entries[static_cast<std::size_t>(crash_.selected)];
    const std::wstring when = FormatFileTimeLocal(entry.modified);
    float actions_left = viewer_x + viewer_width - 10.0f;
    if (!entry.is_dump) {
      const float open_width = ui.ButtonWidth(T(L"Open"), glyph::kOpenExternal);
      actions_left -= open_width;
      if (ui.Button(L"crash.open", D2D1::RectF(actions_left, y + 9.0f, actions_left + open_width, y + 39.0f), T(L"Open"),
                    ButtonStyle::kSubtle, true, glyph::kOpenExternal, T(L"Open the report in a text editor"))) {
        const std::wstring path = entry.path;
        Defer([this, path] { OpenPath(path); });
      }
      const float copy_width = ui.ButtonWidth(T(L"Copy"), glyph::kCopy);
      actions_left -= copy_width + 4.0f;
      if (ui.Button(L"crash.copy", D2D1::RectF(actions_left, y + 9.0f, actions_left + copy_width, y + 39.0f), T(L"Copy"),
                    ButtonStyle::kSubtle, !crash_.text.empty(), glyph::kCopy, T(L"Copy the report to the clipboard"))) {
        Defer([this] {
          if (!OpenClipboard(hwnd_)) return;
          EmptyClipboard();
          const std::size_t bytes = (crash_.text.size() + 1) * sizeof(wchar_t);
          HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
          if (memory != nullptr) {
            std::memcpy(GlobalLock(memory), crash_.text.c_str(), bytes);
            GlobalUnlock(memory);
            if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) GlobalFree(memory);
          }
          CloseClipboard();
          ShowToast(T(L"Crash report copied to the clipboard."));
        });
      }
    }
    ui.TextLine(D2D1::RectF(viewer_x + 16.0f, y + 10.0f, actions_left - 8.0f, y + 40.0f), when,
                TextStyle::kBodyStrong, theme::kText);
    ui.Fill(D2D1::RectF(viewer_x + 1.0f, y + 48.0f, viewer_x + viewer_width - 1.0f, y + 49.0f), theme::kDivider);
    if (!crash_.error.empty()) {
      ui.Text(viewer_x + 16.0f, y + 64.0f, viewer_width - 32.0f, crash_.error, TextStyle::kBody, theme::kFail);
    } else if (entry.is_dump) {
      float cy = y + 72.0f;
      cy += ui.Text(viewer_x + 20.0f, cy, viewer_width - 40.0f,
                    T(L"This file can’t be shown here. Share it with the report when you ask for help."),
                    TextStyle::kBody, theme::kTextSecondary) + 14.0f;
      if (ui.Button(L"crash.dump.folder", D2D1::RectF(viewer_x + 20.0f, cy, viewer_x + 160.0f, cy + theme::kButtonHeight),
                    T(L"Open folder"), ButtonStyle::kStandard, true, glyph::kFolder)) {
        Defer([this] { OpenFolder(paths_.crashes_dir); });
      }
    } else {
      ui.TextView(L"crash.viewer", D2D1::RectF(viewer_x, y + 49.0f, viewer_x + viewer_width, y + panel_height - 2.0f),
                  crash_.text, &crash_.scroll, TF(L"Crash report from {0}", when));
    }
  }
  ui.EndCard(card, y + panel_height - theme::kCardPadding);
  return y + panel_height;
}

float App::BuildUpdates(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, T(L"Updates"), L"");
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  const bool busy_update = update_.stage == UpdateFlow::Stage::kChecking ||
                           update_.stage == UpdateFlow::Stage::kDownloading ||
                           update_.stage == UpdateFlow::Stage::kInstalling;

  int card = ui.BeginCard(x, y, width);
  float cy = y + 18.0f;
  const float check_width = ui.ButtonWidth(T(L"Check now"));
  ui.Text(cx, cy, cw - check_width - 16.0f, std::wstring(L"Elixir ") + kVersion, TextStyle::kHero, theme::kText);
  ui.Text(cx, cy + 28.0f, cw - check_width - 16.0f, T(L"Installed version"), TextStyle::kSecondary,
          theme::kTextSecondary);
  ui.Label(D2D1::RectF(cx, cy, cx + cw - check_width - 16.0f, cy + 44.0f),
           TF(L"Installed version: Elixir {0}", kVersion));
  if (ui.Button(L"updates.check",
                D2D1::RectF(cx + cw - check_width, cy + 5.0f, cx + cw, cy + 5.0f + theme::kButtonHeight),
                T(L"Check now"), ButtonStyle::kStandard, !busy_update)) {
    CheckForUpdates(true);
  }
  cy += 44.0f;
  y = ui.EndCard(card, cy) + theme::kOptionGap;

  BeginOptions();
  D2D1_RECT_F control{};
  cy = SettingRow(ui, cx, y, cw, T(L"Automatically update Elixir"),
                  T(L"Elixir will automatically check and update itself when launching Roblox."), ui.ToggleWidth(), 32.0f,
                  &control, true);
  if (ui.Toggle(L"updates.auto", control, &config_.updates.auto_update, T(L"Automatically update Elixir"))) {
    SaveConfig();
  }
  y = EndOptions(cy) + 16.0f;

  const std::wstring version = Utf8ToWide(update_.manifest.version);
  switch (update_.stage) {
    case UpdateFlow::Stage::kIdle:
      break;
    case UpdateFlow::Stage::kChecking: {
      card = ui.BeginCard(x, y, width);
      cy = y + 18.0f;
      cy += ui.Text(cx, cy, cw, T(L"Checking for updates…"), TextStyle::kBody, theme::kText) + 12.0f;
      ui.Progress(D2D1::RectF(cx, cy, cx + cw, cy + 4.0f), -1.0f);
      y = ui.EndCard(card, cy + 4.0f) + 16.0f;
      break;
    }
    case UpdateFlow::Stage::kUpToDate:
      y = Banner(ui, x, y, width, 0, T(L"Elixir is up to date"), L"") + 16.0f;
      break;
    case UpdateFlow::Stage::kAvailable: {
      card = ui.BeginCard(x, y, width);
      cy = y + 18.0f;
      const std::wstring title = TF(L"Elixir {0} is available", version);
      const float title_height = ui.Text(cx, cy, cw, title, TextStyle::kCardTitle, theme::kText);
      ui.Label(D2D1::RectF(cx, cy, cx + cw, cy + title_height), title);
      cy += title_height + 8.0f;
      if (!update_.manifest.notes.empty()) {
        cy += ui.Text(cx, cy, cw, Utf8ToWide(update_.manifest.notes), TextStyle::kBody, theme::kTextSecondary) + 10.0f;
      }
      cy += 4.0f;
      const float install_width = ui.ButtonWidth(T(L"Update now"));
      if (ui.Button(L"updates.install", D2D1::RectF(cx, cy, cx + install_width, cy + 36.0f), T(L"Update now"),
                    ButtonStyle::kStandard, !runtime_.running)) {
        DownloadUpdate();
      }
      if (runtime_.running) {
        const std::wstring& close_first = T(L"Close Roblox to update.");
        const D2D1_RECT_F hint = D2D1::RectF(cx + install_width + 14.0f, cy, cx + cw, cy + 36.0f);
        ui.TextLine(hint, close_first, TextStyle::kSecondary, theme::kTextTertiary);
        ui.Label(hint, close_first);
      }
      cy += 36.0f;
      y = ui.EndCard(card, cy) + 16.0f;
      break;
    }
    case UpdateFlow::Stage::kDownloading: {
      card = ui.BeginCard(x, y, width);
      cy = y + 18.0f;
      cy += ui.Text(cx, cy, cw, TF(L"Downloading Elixir {0}…", version), TextStyle::kBody, theme::kText) + 12.0f;
      ui.Progress(D2D1::RectF(cx, cy, cx + cw - 110.0f, cy + 4.0f),
                  update_.total > 0 ? static_cast<float>(update_.done) / static_cast<float>(update_.total) : -1.0f);
      if (ui.Button(L"updates.cancel", D2D1::RectF(cx + cw - 92.0f, cy - 14.0f, cx + cw, cy + 18.0f), T(L"Cancel"))) {
        if (update_.cancel) *update_.cancel = true;
      }
      cy += 16.0f;
      cy += ui.Text(cx, cy, cw,
                    update_.total ? TF(L"{0} of {1}", FormatByteSize(update_.done), FormatByteSize(update_.total))
                                  : FormatByteSize(update_.done),
                    TextStyle::kSecondary, theme::kTextSecondary);
      y = ui.EndCard(card, cy) + 16.0f;
      break;
    }
    case UpdateFlow::Stage::kInstalling: {
      card = ui.BeginCard(x, y, width);
      cy = y + 18.0f;
      cy += ui.Text(cx, cy, cw, TF(L"Installing Elixir {0}…", version), TextStyle::kBody, theme::kText) + 12.0f;
      ui.Progress(D2D1::RectF(cx, cy, cx + cw, cy + 4.0f), -1.0f);
      y = ui.EndCard(card, cy + 4.0f) + 16.0f;
      break;
    }
    case UpdateFlow::Stage::kFailed:
      y = Banner(ui, x, y, width, 2,
                 update_.error_title.empty() ? std::wstring(T(L"Elixir couldn’t update")) : update_.error_title,
                 update_.error) +
          16.0f;
      break;
  }
  return BuildVersionHistory(ui, x, y + 8.0f, width);
}

float App::BuildVersionHistory(Ui& ui, float x, float y, float width) {
  y = SectionTitle(ui, x, y, width, T(L"Version history"));
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  if (releases_.list.empty()) {
    if (releases_.stage == ReleaseHistory::Stage::kLoading) {
      const int card = ui.BeginCard(x, y, width);
      float cy = y + 18.0f;
      cy += ui.Text(cx, cy, cw, T(L"Loading…"), TextStyle::kBody, theme::kText) + 12.0f;
      ui.Progress(D2D1::RectF(cx, cy, cx + cw, cy + 4.0f), -1.0f);
      return ui.EndCard(card, cy + 4.0f);
    }
    if (releases_.stage == ReleaseHistory::Stage::kFailed) {
      return Banner(ui, x, y, width, 1, T(L"The version history couldn’t be loaded"), releases_.error);
    }
    if (releases_.stage == ReleaseHistory::Stage::kIdle) return y;
    const std::wstring& none = T(L"No versions have been released yet.");
    const float height = ui.Text(x, y, width, none, TextStyle::kSecondary, theme::kTextTertiary);
    ui.Label(D2D1::RectF(x, y, x + width, y + height), none);
    return y + height;
  }
  for (std::size_t i = 0; i < releases_.list.size(); ++i) {
    const ElixirRelease& release = releases_.list[i];
    const int card = ui.BeginCard(x, y, width);
    float cy = y + 16.0f;
    std::wstring title = L"Elixir " + Utf8ToWide(release.version);
    if (release.prerelease) title = TF(L"{0} (pre-release)", title);
    const std::wstring date = Utf8ToWide(release.published);
    const float date_width = date.empty() ? 0.0f : ui.TextWidth(date, TextStyle::kSecondary) + 16.0f;
    const bool installed = CompareVersions(release.version, ELIXIR_VERSION) == 0;
    const float title_width = std::min(ui.TextWidth(title, TextStyle::kCardTitle), cw - date_width - 96.0f);
    const float title_height = ui.Text(cx, cy, title_width + 2.0f, title, TextStyle::kCardTitle, theme::kText);
    if (installed) {
      const std::wstring& installed_tag = T(L"Installed");
      const float tag_width = ui.TextWidth(installed_tag, TextStyle::kCaption) + 18.0f;
      const D2D1_RECT_F tag = D2D1::RectF(cx + title_width + 12.0f, cy - 1.0f, cx + title_width + 12.0f + tag_width,
                                          cy + 21.0f);
      ui.Fill(tag, theme::kInfoFill, 11.0f);
      ui.Stroke(tag, theme::kInfoBorder, 11.0f);
      ui.TextLine(tag, installed_tag, TextStyle::kCaption, theme::kText, Align::kCenter);
    }
    if (!date.empty()) {
      ui.TextLine(D2D1::RectF(cx + cw - date_width, cy, cx + cw, cy + title_height), date, TextStyle::kSecondary,
                  theme::kTextTertiary, Align::kTrailing);
    }
    ui.Label(D2D1::RectF(cx, cy, cx + cw, cy + title_height),
             (installed ? TF(L"{0}, installed", title) : title) + (date.empty() ? L"" : L", " + date));
    cy += title_height;
    if (!release.notes.empty()) {
      const std::wstring notes = Utf8ToWide(release.notes);
      cy += 8.0f;
      const float notes_height = ui.Text(cx, cy, cw, notes, TextStyle::kBody, theme::kTextSecondary);
      ui.Label(D2D1::RectF(cx, cy, cx + cw, cy + notes_height), notes);
      cy += notes_height;
    }
    if (!release.page_url.empty()) {
      cy += 10.0f;
      if (ui.Link(L"updates.history#" + std::to_wstring(i), cx, cy, T(L"View on GitHub"), nullptr,
                  TextStyle::kSecondary)) {
        const std::wstring url = Utf8ToWide(release.page_url);
        Defer([this, url] { OpenPath(url); });
      }
      cy += 18.0f;
    }
    y = ui.EndCard(card, cy) + 8.0f;
  }
  return y - 8.0f;
}

void App::BuildOverlay(Ui& ui, float width, float height, const Overlay& overlay, float shown) {
  D2D1_COLOR_F dim = theme::kOverlayDim;
  dim.a *= shown;
  ui.Fill(D2D1::RectF(0, 0, width, height), dim);
  ui.PushLayer(shown, (1.0f - shown) * theme::kPopupRise);
  const float dialog_width = std::min(540.0f, width - 48.0f);
  const float pad = 24.0f;
  const float inner = dialog_width - 2.0f * pad;
  const float title_height = ui.TextHeight(overlay.title, TextStyle::kHero, inner);
  const float body_height = ui.TextHeight(overlay.body, TextStyle::kBody, inner);
  const float content_height = pad + title_height + 14.0f + body_height + pad;
  const float footer_height = overlay.actions.empty() ? 0.0f : 80.0f;
  const float dialog_height = content_height + footer_height;
  const float left = std::floor((width - dialog_width) / 2.0f);
  const float top = std::max(24.0f, std::floor((height - dialog_height) / 2.0f));
  const D2D1_RECT_F dialog = D2D1::RectF(left, top, left + dialog_width, top + dialog_height);
  ui.Fill(D2D1::RectF(dialog.left - 1, dialog.top + 2, dialog.right + 1, dialog.bottom + 6), theme::Rgb(0x000000, 0.35f), 10.0f);
  ui.Fill(dialog, theme::kDialog, 8.0f);
  if (footer_height > 0) {
    const float footer_top = dialog.bottom - footer_height;
    ui.Fill(D2D1::RectF(dialog.left, footer_top, dialog.right, dialog.bottom), theme::kDialogFooter, 8.0f);
    ui.Fill(D2D1::RectF(dialog.left, footer_top, dialog.right, footer_top + 10.0f), theme::kDialogFooter);
    ui.Fill(D2D1::RectF(dialog.left, footer_top, dialog.right, footer_top + 1.0f), theme::White(0.06f));
  }
  ui.Stroke(dialog, theme::White(0.12f), 8.0f);

  float y = top + pad;
  ui.Text(left + pad, y, inner, overlay.title, TextStyle::kHero, theme::kText);
  ui.Label(D2D1::RectF(left, y, left + dialog_width, y + title_height), overlay.title);
  y += title_height + 14.0f;
  ui.Text(left + pad, y, inner, overlay.body, TextStyle::kBody, theme::kTextSecondary);
  ui.Label(D2D1::RectF(left, y, left + dialog_width, y + body_height), overlay.body);
  y += body_height;
  if (!overlay.actions.empty()) {
    const float count = static_cast<float>(overlay.actions.size());
    const float button_width = (inner - (count - 1.0f) * 8.0f) / count;
    const float button_top = dialog.bottom - footer_height + (footer_height - 36.0f) / 2.0f;
    float bx = left + pad;
    for (const OverlayAction& action : overlay.actions) {
      if (ui.Button(action.id, D2D1::RectF(bx, button_top, bx + button_width, button_top + 36.0f), action.label,
                    action.style)) {
        const std::wstring id = action.id;
        Defer([this, id] { OnOverlayAction(id); });
      }
      bx += button_width + 8.0f;
    }
  }
  ui.PopLayer();
}

}
