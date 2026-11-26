// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The windows, their layout and their text follow Fishstrap's installer and uninstaller (MIT).

#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#undef PathIsDirectory

#include <algorithm>
#include <cmath>
#include <string>

#include "core/installer.h"
#include "core/project.h"
#include "core/util.h"
#include "ui/app.h"
#include "ui/theme.h"

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif

namespace elixir::launcher::ui {

namespace {

using Microsoft::WRL::ComPtr;

constexpr wchar_t kVersion[] = L"" ELIXIR_VERSION;
constexpr float kMargin = 12.0f;
constexpr float kNavWidth = 200.0f;
constexpr float kBarHeight = 56.0f;
constexpr float kBarButtonWidth = 96.0f;
constexpr float kPromptButtonWidth = 100.0f;
constexpr float kPromptFooterHeight = 64.0f;
constexpr const wchar_t* kSteps[] = {L"Welcome", L"Install", L"Completion"};

constexpr wchar_t kWelcomeThanks[] = L"Thank you for downloading Elixir.";
constexpr wchar_t kWelcomeSourceBefore[] = L"The only official website to download Elixir from is ";
constexpr wchar_t kWelcomeSourceLink[] = L"GitHub";
constexpr wchar_t kWelcomeSourceAfter[] = L", so please verify that you got it from an official source.";
constexpr wchar_t kWelcomeProcess[] =
    L"This installation process will be quick and simple, and you will be able to configure any of Elixir’s "
    L"settings after installation.";
constexpr wchar_t kLocationText[] =
    L"Roblox will also be installed to this path. Change this if you prefer to install all your games to a separate "
    L"drive. Otherwise, it’s recommended that you keep this as it is.";
constexpr wchar_t kShortcutsText[] =
    L"These are general shortcuts that bring up a multi-choice launch menu. Shortcuts for specific functions can be "
    L"created later in the settings.";
constexpr wchar_t kInstalledText[] = L"Elixir has successfully been installed.";
constexpr wchar_t kRobloxLaterText[] =
    L"Roblox has not yet been installed, that will happen when you launch it with Elixir for the first time. "
    L"However, before you do that, you may want to configure Elixir’s settings first.";

std::wstring OwnFolderQuestion(const std::wstring& suggestion) {
  return L"The folder you’ve chosen to install Elixir to already exists and is NOT empty. It is strongly "
         L"recommended for Elixir to be installed to its own independent folder.\n\n"
         L"Changing to the following location is suggested:\n" +
         suggestion +
         L"\n\nWould you like to change to the suggested location?\n"
         L"Selecting ‘No’ will ignore this warning and continue installation.";
}

}

SIZE InstallerWindowSize(UINT dpi) {
  return SIZE{MulDiv(840, static_cast<int>(dpi), 96), MulDiv(540, static_cast<int>(dpi), 96)};
}

SIZE PromptWindowSize(UINT dpi) {
  return SIZE{MulDiv(480, static_cast<int>(dpi), 96), MulDiv(260, static_cast<int>(dpi), 96)};
}

void App::BuildInstallerFrame(Ui& ui, float width, float height) {
  if (mica_) ui.Fill(D2D1::RectF(0, 0, width, height), theme::kMicaTint);
  if (ui.painting() && installer_.step != installer_.shown_step) {
    installer_.shown_step = installer_.step;
    ++page_epoch_;
  }
  const float bar_top = height - kBarHeight;

  float nav_y = kMargin + 4.0f;
  for (int i = 0; i < 3; ++i) {
    const D2D1_RECT_F row = D2D1::RectF(kMargin, nav_y, kMargin + kNavWidth - 12.0f, nav_y + 38.0f);
    const bool current = i == installer_.step;
    if (current) {
      ui.Fill(row, theme::kNavSelected, theme::kControlRadius);
      ui.Fill(D2D1::RectF(row.left, row.top + 11.0f, row.left + 3.0f, row.bottom - 11.0f), theme::kText, 1.5f);
    }
    ui.TextLine(D2D1::RectF(row.left + 16.0f, row.top, row.right - 8.0f, row.bottom), kSteps[i], TextStyle::kBody,
                current ? theme::kText : theme::kTextDisabled);
    ui.Label(row, kSteps[i]);
    nav_y += 42.0f;
  }

  const float x = kMargin + kNavWidth;
  const float content_width = std::max(240.0f, width - x - 14.0f);
  float top = kMargin;
  const float title_height =
      ui.Text(x, top, content_width, kSteps[installer_.step], TextStyle::kTitle, theme::kText);
  ui.Label(D2D1::RectF(x, top, x + content_width, top + title_height), kSteps[installer_.step]);
  top += title_height + 9.0f;

  viewport_ = D2D1::RectF(x - 4.0f, top, width, bar_top);
  const float viewport_height = viewport_.bottom - viewport_.top;
  const int index = static_cast<int>(page_);
  float& scroll = scroll_[index];
  float& content_height = content_height_[index];
  const float max_scroll = std::max(0.0f, content_height - viewport_height);
  scroll = std::clamp(scroll, 0.0f, max_scroll);
  scroll_target_[index] = std::clamp(scroll_target_[index], 0.0f, max_scroll);

  ui.PushClip(viewport_);
  const float shown = ui.Animate(L"page." + std::to_wstring(page_epoch_), 1.0f, theme::kPageTransitionMs, 0.0f,
                                 Ease::kWpfDecelerate);
  ui.PushLayer(shown, (1.0f - shown) * theme::kPageSlide);
  const float page_top = top - scroll;
  float bottom = page_top;
  switch (installer_.step) {
    case 0: bottom = BuildInstallerWelcome(ui, x, page_top, content_width); break;
    case 1: bottom = BuildInstallerLocation(ui, x, page_top, content_width); break;
    default: bottom = BuildInstallerCompletion(ui, x, page_top, content_width); break;
  }
  ui.PopLayer();
  ui.PopClip();
  if (ui.painting()) content_height = bottom - page_top + 14.0f;

  ui.Fill(D2D1::RectF(0, bar_top, width, height), theme::kLayer);
  ui.Fill(D2D1::RectF(0, bar_top, width, bar_top + 1.0f), theme::kDivider);
  const float button_top = bar_top + (kBarHeight - theme::kButtonHeight) / 2.0f;
  const float close_left = width - 14.0f - kBarButtonWidth;
  const float next_left = close_left - 12.0f - kBarButtonWidth;
  const float back_left = next_left - 8.0f - kBarButtonWidth;
  const auto bar_button = [&](const wchar_t* id, float left, const wchar_t* label, bool enabled) {
    return ui.Button(id, D2D1::RectF(left, button_top, left + kBarButtonWidth, button_top + theme::kButtonHeight),
                     label, ButtonStyle::kStandard, enabled);
  };
  if (bar_button(L"installer.back", back_left, L"Back", installer_.step == 1)) {
    Defer([this] {
      if (!state_.editing.empty()) EndEdit(true);
      installer_.step = 0;
      Invalidate();
    });
  }
  if (bar_button(L"installer.next", next_left, installer_.step == 1 ? L"Install" : L"Next", installer_.step < 2)) {
    Defer([this] { InstallerNext(); });
  }
  if (bar_button(L"installer.close", close_left, L"Close", true)) {
    Defer([this] { PostMessageW(hwnd_, WM_CLOSE, 0, 0); });
  }
}

float App::BuildInstallerWelcome(Ui& ui, float x, float y, float width) {
  const auto paragraph = [&](std::wstring_view text) {
    const float height = ui.Text(x, y, width, text, TextStyle::kBody, theme::kText);
    ui.Label(D2D1::RectF(x, y, x + width, y + height), text);
    y += height;
  };
  const float line = ui.TextHeight(L"Ag", TextStyle::kBody, width);
  paragraph(kWelcomeThanks);
  y += line;

  const float source_top = y;
  const float before_width = ui.TextWidth(kWelcomeSourceBefore, TextStyle::kBody);
  ui.Text(x, y, before_width + 2.0f, kWelcomeSourceBefore, TextStyle::kBody, theme::kText);
  float link_width = 0.0f;
  if (ui.Link(L"installer.website", x + before_width, y, kWelcomeSourceLink, &link_width, TextStyle::kBody)) {
    Defer([this] { OpenPath(kProjectWebsite); });
  }
  const std::wstring_view after = kWelcomeSourceAfter;
  const float room = width - before_width - link_width;
  std::size_t fitted = 0;
  for (std::size_t at = 0; at < after.size();) {
    const std::size_t space = after.find(L' ', at);
    const std::size_t end = space == std::wstring_view::npos ? after.size() : space;
    if (ui.TextWidth(after.substr(0, end), TextStyle::kBody) > room) break;
    fitted = end;
    at = end == after.size() ? end : end + 1;
  }
  if (fitted != 0) {
    ui.Text(x + before_width + link_width, y, room + 2.0f, after.substr(0, fitted), TextStyle::kBody, theme::kText);
  }
  y += line;
  std::wstring_view rest = after.substr(fitted);
  while (!rest.empty() && rest.front() == L' ') rest.remove_prefix(1);
  if (!rest.empty()) y += ui.Text(x, y, width, rest, TextStyle::kBody, theme::kText);
  ui.Label(D2D1::RectF(x, source_top, x + width, y),
           std::wstring(kWelcomeSourceBefore) + kWelcomeSourceLink + kWelcomeSourceAfter);
  y += line;

  paragraph(kWelcomeProcess);
  y += 24.0f;
  paragraph(L"Please click ‘Next’ to continue.");
  return y;
}

float App::BuildInstallerLocation(Ui& ui, float x, float y, float width) {
  const auto heading = [&](const wchar_t* text) {
    const float height = ui.Text(x, y, width, text, TextStyle::kSection, theme::kText);
    ui.Label(D2D1::RectF(x, y, x + width, y + height), text);
    y += height + 2.0f;
  };
  const auto paragraph = [&](std::wstring_view text, const D2D1_COLOR_F& color) {
    const float height = ui.Text(x, y, width, text, TextStyle::kBody, color);
    ui.Label(D2D1::RectF(x, y, x + width, y + height), text);
    y += height;
  };

  heading(L"Choose where to install to");
  paragraph(kLocationText, theme::kText);
  y += 8.0f;

  int card = ui.BeginCard(x, y, width);
  const float pad = 12.0f;
  const float row_top = y + pad;
  const float browse_width = std::max(84.0f, ui.ButtonWidth(L"Browse"));
  const float reset_width = std::max(76.0f, ui.ButtonWidth(L"Reset"));
  const float reset_left = x + width - pad - reset_width;
  const float browse_left = reset_left - 8.0f - browse_width;
  const D2D1_RECT_F field = D2D1::RectF(x + pad, row_top, browse_left - 8.0f, row_top + 35.0f);
  if (ui.TextField(L"installer.location", field, &installer_.location, L"", L"Install location")) {
    installer_.location = NormalizeInstallLocation(installer_.location);
    installer_.data_found = ExistingDataPresent(installer_.location);
    installer_.own_folder_declined = false;
    installer_.error.clear();
  }
  if (ui.Button(L"installer.browse", D2D1::RectF(browse_left, row_top, browse_left + browse_width, row_top + 35.0f),
                L"Browse")) {
    Defer([this] { InstallerBrowse(); });
  }
  if (ui.Button(L"installer.reset", D2D1::RectF(reset_left, row_top, reset_left + reset_width, row_top + 35.0f),
                L"Reset")) {
    Defer([this] {
      if (!state_.editing.empty()) EndEdit(false);
      installer_.location = DefaultInstallLocation(options_.install_environment);
      installer_.data_found = ExistingDataPresent(installer_.location);
      installer_.own_folder_declined = false;
      installer_.error.clear();
      Invalidate();
    });
  }
  y = ui.EndCard(card, row_top + 35.0f + pad - theme::kCardPadding);

  if (installer_.data_found) {
    y += 8.0f;
    paragraph(L"Existing data found. Your mods and settings will be restored.", theme::kText);
  }
  if (!installer_.error.empty()) {
    y += 8.0f;
    paragraph(installer_.error, theme::kFail);
  }

  y += 16.0f;
  heading(L"Shortcuts");
  paragraph(kShortcutsText, theme::kText);
  y += 8.0f;

  const float gap = 8.0f;
  const float half = (width - gap) / 2.0f;
  const auto option = [&](const wchar_t* id, float left, const wchar_t* label, bool* value) {
    const int option_card = ui.BeginCard(left, y, half);
    const float toggle_width = 96.0f;
    ui.TextLine(D2D1::RectF(left + 16.0f, y, left + half - toggle_width - 16.0f, y + 56.0f), label, TextStyle::kBody,
                theme::kText);
    ui.Toggle(id, D2D1::RectF(left + half - 12.0f - toggle_width, y + 12.0f, left + half - 12.0f, y + 44.0f), value,
              label);
    return ui.EndCard(option_card, y + 56.0f - theme::kCardPadding);
  };
  const float options_bottom = option(L"installer.desktop", x, L"Desktop icon", &installer_.desktop);
  option(L"installer.startmenu", x + half + gap, L"Start Menu icon", &installer_.start_menu);
  return options_bottom;
}

float App::BuildInstallerCompletion(Ui& ui, float x, float y, float width) {
  y += 8.0f;
  std::wstring text = kInstalledText;
  if (!installer_.roblox_installed) text += std::wstring(L"\n\n") + kRobloxLaterText;
  text += L"\n\nWhat would you like to do?";
  const float height = ui.Text(x, y, width, text, TextStyle::kBody, theme::kText);
  ui.Label(D2D1::RectF(x, y, x + width, y + height), text);
  y += height + 16.0f;

  constexpr int kActionGroup = 9401;
  const auto action = [&](const wchar_t* id, const wchar_t* title, const wchar_t* description,
                          InstallerAction next) {
    const D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + 62.0f);
    ui.Fill(rect, theme::kCard, theme::kControlRadius);
    ui.Stroke(rect, theme::kCardBorder, theme::kControlRadius);
    if (ui.ListItem(id, rect, false, title, description, L"", 0, kActionGroup)) {
      Defer([this, next] { FinishInstaller(next); });
    }
    ui.Glyph(rect.right - 22.0f, (rect.top + rect.bottom) / 2.0f, glyph::kChevronRight, TextStyle::kIconSmall,
             theme::kTextSecondary);
    y += 62.0f + 8.0f;
  };
  action(L"installer.settings", L"Configure Elixir’s settings", L"Tweak with all the features it has to offer",
         InstallerAction::kSettings);
  action(L"installer.launch", installer_.roblox_installed ? L"Launch Roblox" : L"Install and launch Roblox",
         L"Will drop you into Roblox once everything’s done", InstallerAction::kLaunch);
  return y - 8.0f;
}

void App::BuildPromptFrame(Ui& ui, float width, float height) {
  if (mica_) ui.Fill(D2D1::RectF(0, 0, width, height), theme::kMicaTint);
  const PromptSpec& prompt = options_.prompt;
  const float x = kMargin + 4.0f;
  const float text_width = width - 2.0f * x;
  float y = kMargin;
  if (!prompt.heading.empty()) {
    const float heading_height = ui.Text(x, y, text_width, prompt.heading, TextStyle::kSection, theme::kText);
    ui.Label(D2D1::RectF(x, y, x + text_width, y + heading_height), prompt.heading);
    y += heading_height + 6.0f;
  }
  const float body_height = ui.Text(x, y, text_width, prompt.text, TextStyle::kBody, theme::kText);
  ui.Label(D2D1::RectF(x, y, x + text_width, y + body_height), prompt.text);
  y += body_height;
  if (!prompt.checkbox.empty()) {
    y += 16.0f;
    const float label_width = ui.TextWidth(prompt.checkbox, TextStyle::kBody);
    const D2D1_RECT_F box = D2D1::RectF(x, y, x + 30.0f + label_width, y + 26.0f);
    if (ui.CheckBox(L"prompt.check", box, &installer_.prompt_checked, prompt.checkbox)) Invalidate();
    ui.TextLine(D2D1::RectF(x + 30.0f, y, x + 31.0f + label_width, y + 26.0f), prompt.checkbox, TextStyle::kBody,
                theme::kText);
    y += 26.0f;
    if (installer_.prompt_checked && !prompt.checkbox_note.empty()) {
      y += 4.0f;
      const float note_height = ui.Text(x, y, text_width, prompt.checkbox_note, TextStyle::kBody, theme::kText);
      ui.Label(D2D1::RectF(x, y, x + text_width, y + note_height), prompt.checkbox_note);
      y += note_height;
    }
  }
  y += kMargin + 10.0f;
  const float needed = y + kPromptFooterHeight;
  const float footer_top = std::max(y, height - kPromptFooterHeight);
  ui.Fill(D2D1::RectF(0, footer_top, width, height), theme::kDialogFooter);
  ui.Fill(D2D1::RectF(0, footer_top, width, footer_top + 1.0f), theme::White(0.06f));
  float right = width - 15.0f;
  for (int i = static_cast<int>(prompt.buttons.size()) - 1; i >= 0; --i) {
    const std::wstring& label = prompt.buttons[static_cast<std::size_t>(i)];
    const float button_width = std::max(kPromptButtonWidth, ui.ButtonWidth(label));
    const D2D1_RECT_F rect = D2D1::RectF(right - button_width, footer_top + 15.0f, right, footer_top + 49.0f);
    if (ui.Button(L"prompt.button." + std::to_wstring(i), rect, label)) {
      prompt_result_.button = i;
      prompt_result_.checked = installer_.prompt_checked;
      Defer([this] { PostMessageW(hwnd_, WM_CLOSE, 0, 0); });
    }
    right -= button_width + 12.0f;
  }
  viewport_ = D2D1::RectF(0, 0, width, footer_top);
  if (ui.painting() && frame_size_override_.width <= 0.0f && std::fabs(needed - installer_.prompt_height) > 0.5f) {
    installer_.prompt_height = needed;
    Defer([this, needed] {
      if (hwnd_ == nullptr) return;
      RECT client{};
      GetClientRect(hwnd_, &client);
      RECT frame{0, 0, client.right, static_cast<LONG>(std::lround(needed * scale()))};
      AdjustWindowRectExForDpi(&frame, WindowStyleForMode(), FALSE, 0, dpi_);
      RECT now{};
      GetWindowRect(hwnd_, &now);
      const int new_height = frame.bottom - frame.top;
      const int top = now.top + ((now.bottom - now.top) - new_height) / 2;
      SetWindowPos(hwnd_, nullptr, now.left, top, now.right - now.left, new_height, SWP_NOZORDER | SWP_NOACTIVATE);
    });
  }
}

void App::InstallerNext() {
  if (!state_.editing.empty()) EndEdit(true);
  if (installer_.step == 0) {
    installer_.step = 1;
    Invalidate();
    return;
  }
  if (installer_.step != 1) return;
  installer_.error.clear();
  installer_.location = NormalizeInstallLocation(installer_.location);
  installer_.data_found = ExistingDataPresent(installer_.location);
  const std::wstring suggestion =
      installer_.own_folder_declined ? std::wstring() : SuggestedOwnFolder(installer_.location);
  if (!suggestion.empty() && InstallLocationAllowed(installer_.location, options_.install_environment)) {
    Overlay overlay;
    overlay.id = L"installer.own";
    overlay.status = 1;
    overlay.title = L"Elixir Installer";
    overlay.body = OwnFolderQuestion(suggestion);
    overlay.actions = {{L"installer.own.yes", L"Yes", ButtonStyle::kStandard},
                       {L"installer.own.no", L"No", ButtonStyle::kStandard},
                       {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
    ShowOverlay(std::move(overlay));
    return;
  }
  RunInstall();
}

void App::InstallerBrowse() {
  if (options_.test_mode) return;
  if (!state_.editing.empty()) EndEdit(true);
  ComPtr<IFileOpenDialog> dialog;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
  DWORD flags = 0;
  dialog->GetOptions(&flags);
  dialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
  if (FAILED(dialog->Show(hwnd_))) return;
  ComPtr<IShellItem> item;
  PWSTR path = nullptr;
  if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return;
  installer_.location = NormalizeInstallLocation(path);
  CoTaskMemFree(path);
  installer_.data_found = ExistingDataPresent(installer_.location);
  installer_.own_folder_declined = false;
  installer_.error.clear();
  Invalidate();
}

void App::RunInstall() {
  InstallRequest request;
  request.source = options_.install_source;
  request.location = installer_.location;
  request.version = kVersion;
  request.desktop_shortcut = installer_.desktop;
  request.start_menu_shortcut = installer_.start_menu;
  std::wstring error;
  if (!InstallElixir(request, options_.install_environment, &error)) {
    installer_.error = error.empty() ? std::wstring(L"Elixir couldn’t be installed here.") : error;
    Invalidate();
    return;
  }
  installer_outcome_.finished = true;
  installer_outcome_.location = installer_.location;
  installer_.roblox_installed =
      PathIsDirectory(JoinPath(JoinPath(installer_.location, L"apps"), Utf8ToWide(kRobloxPackage)));
  installer_.step = 2;
  Invalidate();
}

void App::FinishInstaller(InstallerAction action) {
  installer_outcome_.action = action;
  if (hwnd_ != nullptr && !options_.test_mode) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

bool App::ConfirmInstallerClose() {
  if (installer_outcome_.finished || installer_.cancel_confirmed || options_.test_mode) return true;
  if (overlay_.open && overlay_.id == L"installer.cancel") return false;
  Overlay overlay;
  overlay.id = L"installer.cancel";
  overlay.status = 1;
  overlay.title = L"Elixir Installer";
  overlay.body = L"Are you sure you want to cancel the installation?";
  overlay.actions = {{L"installer.cancel.yes", L"Yes", ButtonStyle::kStandard},
                     {L"overlay.close", L"No", ButtonStyle::kStandard}};
  ShowOverlay(std::move(overlay));
  return false;
}

bool App::OnInstallerOverlayAction(const std::wstring& id) {
  if (options_.mode != AppMode::kInstaller) return false;
  if (id == L"installer.cancel.yes") {
    installer_.cancel_confirmed = true;
    CloseOverlay();
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    return true;
  }
  if (id == L"installer.own.yes" || id == L"installer.own.no") {
    if (id == L"installer.own.yes") {
      const std::wstring suggestion = SuggestedOwnFolder(installer_.location);
      if (!suggestion.empty()) installer_.location = suggestion;
      installer_.data_found = ExistingDataPresent(installer_.location);
    } else {
      installer_.own_folder_declined = true;
    }
    CloseOverlay();
    RunInstall();
    return true;
  }
  return false;
}

}
