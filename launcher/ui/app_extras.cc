// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/game_shortcuts.h"
#include "core/mods.h"
#include "core/shortcuts.h"
#include "core/util.h"
#include "platform/windows/app_user_model_id.h"
#include "resource.h"
#include "ui/app.h"

namespace elixir::launcher::ui {

using Microsoft::WRL::ComPtr;

void App::RefreshMods() {
  if (mods_.scanning) return;
  mods_.scanning = true;
  const std::wstring directory = ModsDirectory(paths_);
  RunInBackground([this, directory]() -> std::function<void()> {
    ModsSummary summary = ScanMods(directory);
    return [this, summary = std::move(summary)]() mutable {
      mods_.summary = std::move(summary);
      mods_.scanned = true;
      mods_.scanning = false;
      Invalidate();
    };
  });
}

void App::ChooseCustomFont() {
  if (options_.test_mode || mods_.working) return;
  ComPtr<IFileOpenDialog> dialog;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
  const COMDLG_FILTERSPEC filters[] = {{L"Fonts (*.ttf, *.otf, *.ttc)", L"*.ttf;*.otf;*.ttc"},
                                       {L"All files (*.*)", L"*.*"}};
  dialog->SetFileTypes(2, filters);
  dialog->SetTitle(L"Choose a font");
  FILEOPENDIALOGOPTIONS flags = 0;
  dialog->GetOptions(&flags);
  dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
  if (FAILED(dialog->Show(hwnd_))) return;
  ComPtr<IShellItem> item;
  PWSTR path = nullptr;
  if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return;
  const std::wstring chosen = path;
  CoTaskMemFree(path);
  ApplyCustomFontFile(chosen);
}

void App::ApplyCustomFontFile(const std::wstring& font) {
  if (mods_.working) return;
  if (!config_.installed.present()) {
    mods_.message = L"Install Roblox first.";
    mods_.message_status = 1;
    Invalidate();
    return;
  }
  mods_.working = true;
  mods_.message.clear();
  const std::wstring directory = ModsDirectory(paths_);
  const std::wstring assets = JoinPath(Utf8ToWide(config_.installed.install_dir), L"assets");
  RunInBackground([this, directory, assets, font]() -> std::function<void()> {
    CustomFontResult result = ApplyCustomFont(directory, assets, font);
    return [this, result = std::move(result)] {
      mods_.working = false;
      mods_.message = result.ok ? std::wstring() : result.error;
      mods_.message_status = 2;
      RefreshMods();
      Invalidate();
    };
  });
  Invalidate();
}

void App::RemoveCustomFontMod() {
  if (mods_.working) return;
  mods_.working = true;
  mods_.message.clear();
  const std::wstring directory = ModsDirectory(paths_);
  RunInBackground([this, directory]() -> std::function<void()> {
    std::wstring error;
    const bool ok = RemoveCustomFont(directory, &error);
    return [this, ok, error] {
      mods_.working = false;
      mods_.message = ok ? std::wstring() : error;
      mods_.message_status = 2;
      RefreshMods();
      Invalidate();
    };
  });
  Invalidate();
}

std::wstring App::ShortcutFolder(bool desktop) const {
  if (options_.test_mode) return JoinPath(paths_.data_root, desktop ? L"test-desktop" : L"test-start-menu");
  return desktop ? DesktopDirectory() : StartMenuProgramsDirectory();
}

GameShortcutPlaces App::ShortcutPlaces() const {
  GameShortcutPlaces places;
  places.folder = ShortcutFolder(true);
  places.program = ExecutablePath();
  places.state_file = JoinPath(paths_.data_root, L"shortcuts.json");
  places.icons_dir = JoinPath(paths_.data_root, L"shortcut-icons");
  return places;
}

HttpGetFunction App::WebGet() const {
  if (options_.web_get) return options_.web_get;
  if (options_.test_mode) return {};
  return [](const std::wstring& url, std::uint64_t max_bytes) { return HttpsGet(url, max_bytes, nullptr); };
}

void App::RefreshShortcuts() {
  if (shortcuts_.loading) return;
  shortcuts_.loading = true;
  const std::wstring program = ExecutablePath();
  const std::wstring desktop = ShortcutFolder(true);
  const std::wstring start_menu = ShortcutFolder(false);
  const GameShortcutPlaces places = ShortcutPlaces();
  RunInBackground([this, program, desktop, start_menu, places]() -> std::function<void()> {
    const bool on_desktop = !desktop.empty() && ShortcutRuns(JoinPath(desktop, kElixirShortcutName), program);
    const bool in_start = !start_menu.empty() && ShortcutRuns(JoinPath(start_menu, kElixirShortcutName), program);
    std::vector<GameShortcutEntry> games = ListGameShortcutEntries(places);
    return [this, on_desktop, in_start, games = std::move(games)]() mutable {
      shortcuts_.desktop = on_desktop;
      shortcuts_.start_menu = in_start;
      shortcuts_.games = std::move(games);
      shortcuts_.loaded = true;
      shortcuts_.loading = false;
      if (!shortcuts_.games.empty()) UpdateGameShortcutsNow(false);
      Invalidate();
    };
  });
}

void App::SetElixirShortcut(bool desktop, bool on) {
  const std::wstring folder = ShortcutFolder(desktop);
  const std::wstring program = ExecutablePath();
  const wchar_t* place = desktop ? L"desktop" : L"Start menu";
  if (folder.empty()) {
    shortcuts_.message = std::wstring(L"Windows didn’t say where your ") + place + L" is.";
    Invalidate();
    return;
  }
  const std::wstring link = JoinPath(folder, kElixirShortcutName);
  std::wstring error;
  bool ok = true;
  if (on) {
    ShortcutInfo existing;
    if (PathIsFile(link) && ReadShortcut(link, &existing) && !PathsEqual(existing.target, program)) {
      ok = false;
      error = std::wstring(L"A shortcut named Elixir that opens something else is already on your ") + place + L".";
    } else {
      EnsureDirectory(folder, nullptr);
      ShortcutSpec spec;
      spec.target = program;
      spec.description = L"Elixir";
      spec.app_id = elixir::kAppUserModelId;
      ok = CreateShortcut(link, spec, &error);
    }
  } else {
    ok = RemoveShortcut(link, program);
    if (!ok) error = L"That shortcut couldn’t be removed.";
  }
  shortcuts_.message = ok ? std::wstring() : error;
  RefreshShortcuts();
  Invalidate();
}

void App::CreateGameShortcut() {
  std::uint64_t place_id = 0;
  if (shortcuts_.working || !ParsePlaceId(shortcuts_.game_link, &place_id)) return;
  shortcuts_.working = true;
  shortcuts_.message.clear();
  const GameShortcutPlaces places = ShortcutPlaces();
  const HttpGetFunction get = WebGet();
  RunInBackground([this, places, place_id, get]() -> std::function<void()> {
    GameShortcutChange change = AddGameShortcut(places, place_id, get);
    return [this, change = std::move(change)] {
      shortcuts_.working = false;
      if (change.ok) {
        shortcuts_.game_link.clear();
        shortcuts_.motion.Enter(FileNameOf(change.path));
        shortcuts_.updated_at = GetTickCount64();
      } else {
        shortcuts_.message = change.error;
      }
      RefreshShortcuts();
      Invalidate();
    };
  });
  Invalidate();
}

void App::RenameGameShortcutTo(const std::wstring& path, const std::wstring& name) {
  if (shortcuts_.working) return;
  const GameShortcutChange change = RenameGameShortcut(ShortcutPlaces(), path, name);
  shortcuts_.message = change.ok ? std::wstring() : change.error;
  if (change.ok && name.empty()) shortcuts_.updated_at = 0;
  RefreshShortcuts();
  Invalidate();
}

void App::RemoveGameShortcut(const std::wstring& path) {
  if (shortcuts_.working) return;
  const auto found = std::find_if(shortcuts_.games.begin(), shortcuts_.games.end(),
                                  [&path](const GameShortcutEntry& game) { return PathsEqual(game.path, path); });
  if (DeleteGameShortcut(ShortcutPlaces(), path)) {
    shortcuts_.message.clear();
    if (found != shortcuts_.games.end()) {
      shortcuts_.motion.leaving.clear();
      shortcuts_.motion.Leave(FileNameOf(path), static_cast<std::size_t>(found - shortcuts_.games.begin()), found->name,
                              found->icon);
      shortcuts_.games.erase(found);
    }
  } else {
    shortcuts_.message = L"That shortcut couldn’t be removed.";
  }
  RefreshShortcuts();
  Invalidate();
}

void App::UpdateGameShortcutsNow(bool force) {
  constexpr ULONGLONG kEveryMs = 10 * 60 * 1000;
  const HttpGetFunction get = WebGet();
  if (!get || shortcuts_.updating || shortcuts_.working) return;
  const ULONGLONG now = GetTickCount64();
  if (!force && shortcuts_.updated_at != 0 && now - shortcuts_.updated_at < kEveryMs) return;
  shortcuts_.updating = true;
  const GameShortcutPlaces places = ShortcutPlaces();
  RunInBackground([this, places, get]() -> std::function<void()> {
    const int changed = UpdateGameShortcuts(places, get);
    return [this, changed] {
      shortcuts_.updating = false;
      shortcuts_.updated_at = GetTickCount64();
      if (changed > 0) RefreshShortcuts();
    };
  });
}

void App::LoadNotices() {
  if (notices_loaded_) return;
  notices_loaded_ = true;
  std::string text;
  if (LoadResourceText(IDR_THIRD_PARTY_NOTICES, &text)) notices_ = ParseNotices(Utf8ToWide(text));
  text.clear();
  if (LoadResourceText(IDR_LICENSE, &text)) {
    license_text_ = Utf8ToWide(text);
    license_text_.erase(std::remove(license_text_.begin(), license_text_.end(), L'\r'), license_text_.end());
    while (!license_text_.empty() && license_text_.back() == L'\n') license_text_.pop_back();
  }
}

}
