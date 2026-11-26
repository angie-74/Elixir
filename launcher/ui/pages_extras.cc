// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/mods.h"
#include "core/shortcuts.h"
#include "core/util.h"
#include "ui/app.h"
#include "ui/theme.h"

namespace elixir::launcher::ui {

namespace {

constexpr wchar_t kVersion[] = L"" ELIXIR_VERSION;
constexpr wchar_t kModsHelpUrl[] = L"https://bloxstraplabs.com/wiki/features/modding/";

struct Credit {
  const wchar_t* name;
  const wchar_t* note;
  const wchar_t* url;
};

constexpr const wchar_t* kMocktailAuthors[] = {
    L"komaruworld", L"珍惜", L"Savva", L"farwhile", L"glook9001", L"LightZirconite", L"Obsidian",
    L"techs-sus",   L"vii",  L"Gurov", L"Haumea",   L"Lupey",     L"Mia",            L"Mohamed Maatallah",
    L"Twig6943",    L"caly", L"exefer", L"hxehex.real", L"native", L"reqhald",
};

constexpr Credit kDesignCredits[] = {
    {L"Bloxstrap", L"Where it started: Fast Flags, mods, BloxstrapRPC", L"https://github.com/bloxstraplabs/bloxstrap"},
    {L"Fishstrap", L"A fork of Bloxstrap. Elixir’s launcher follows it", L"https://github.com/fishstrap/fishstrap"},
    {L"WPF UI", L"The look of the sliders", L"https://github.com/lepoco/wpfui"},
    {L"Lucide", L"Icons", L"https://github.com/lucide-icons/lucide"},
    {L"discord-rpc-csharp", L"How Rich Presence talks to Discord", L"https://github.com/Lachee/discord-rpc-csharp"},
};

constexpr Credit kThanksCredits[] = {
    {L"Mocktail", L"The Linux runtime Elixir is built on", L"https://github.com/komaruworld/mocktail"},
    {L"mcpelauncher-linker", L"The Android library loader (ChristopherHX, MCMrARM)", L""},
    {L"Android Open Source Project", L"bionic, the Android C library", L""},
    {L"Android Compatibility Layer", L"Reference (Reveny)", L"https://github.com/reveny/Android-Compatibility-Layer"},
    {L"libhybris", L"Reference", L"https://github.com/libhybris/libhybris"},
    {L"Cordial", L"Published findings on the same client", L"https://github.com/luohoa97/cordial"},
    {L"utf8proc", L"Unicode tables", L"https://github.com/JuliaStrings/utf8proc"},
};

}

float App::BuildMods(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, L"Mods", L"Replace Roblox’s textures, sounds and fonts with your own.");
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  D2D1_RECT_F control{};
  const ModsSummary& summary = mods_.summary;

  BeginOptions();
  float cy = y;
  {
    const float help_width = ui.ButtonWidth(L"Help", glyph::kOpenExternal);
    const float open_width = ui.ButtonWidth(L"Open folder", glyph::kFolder);
    cy = SettingRow(ui, cx, cy, cw, L"Mods folder", L"Files here replace Roblox’s files with the same path.",
                    help_width + 8.0f + open_width, theme::kButtonHeight, &control, true);
    if (ui.Button(L"mods.help", D2D1::RectF(control.left, control.top, control.left + help_width, control.bottom),
                  L"Help", ButtonStyle::kStandard, true, glyph::kOpenExternal,
                  L"Opens Bloxstrap’s guide to mods in your browser")) {
      Defer([this] { OpenPath(kModsHelpUrl); });
    }
    if (ui.Button(L"mods.open", D2D1::RectF(control.right - open_width, control.top, control.right, control.bottom),
                  L"Open folder", ButtonStyle::kStandard, true, glyph::kFolder, L"Opens your Mods folder")) {
      Defer([this] {
        OpenFolder(ModsDirectory(paths_));
        RefreshMods();
      });
    }
  }
  cy = SettingRow(ui, cx, cy, cw, L"Use mods", L"", 96.0f, 32.0f, &control, false);
  if (ui.Toggle(L"mods.enabled", control, &config_.mods.enabled, L"Use mods")) SaveConfig();
  {
    const float choose_width = ui.ButtonWidth(L"Choose font…", glyph::kOpenFile);
    const float remove_width = ui.ButtonWidth(L"Remove", glyph::kDelete);
    cy = SettingRow(ui, cx, cy, cw, L"Custom font",
                    summary.custom_font ? L"Roblox uses your font." : L"One font for all of Roblox’s text.",
                    choose_width + 8.0f + remove_width, theme::kButtonHeight, &control, false);
    const bool idle = !mods_.working;
    if (ui.Button(L"mods.font.choose",
                  D2D1::RectF(control.left, control.top, control.left + choose_width, control.bottom), L"Choose font…",
                  ButtonStyle::kStandard, idle, glyph::kOpenFile)) {
      Defer([this] { ChooseCustomFont(); });
    }
    if (ui.Button(L"mods.font.remove",
                  D2D1::RectF(control.right - remove_width, control.top, control.right, control.bottom), L"Remove",
                  ButtonStyle::kStandard, idle && summary.custom_font, glyph::kDelete, L"Remove the custom font")) {
      Defer([this] { RemoveCustomFontMod(); });
    }
  }
  y = EndOptions(cy) + 16.0f;

  if (mods_.scanned) {
    if (summary.used_files == 0 && summary.unused_files == 0) {
      y = Banner(ui, x, y, width, 4, L"No mods yet", L"") + 12.0f;
    }
    if (summary.used_files != 0) {
      const std::wstring title = std::to_wstring(summary.used_files) +
                                 (summary.used_files == 1 ? L" file replaces Roblox’s" : L" files replace Roblox’s");
      y = Banner(ui, x, y, width, config_.mods.enabled ? 0 : 4, title, config_.mods.enabled ? L"" : L"Mods are off.") +
          12.0f;
    }
    if (summary.unused_files != 0) {
      std::wstring body = L"Only textures, sounds, fonts and sky images are replaced. Not used: ";
      for (std::size_t i = 0; i < summary.unused_examples.size(); ++i) {
        if (i != 0) body += L", ";
        body += summary.unused_examples[i];
      }
      if (summary.unused_files > summary.unused_examples.size()) body += L" and others";
      body += L".";
      const std::wstring title = std::to_wstring(summary.unused_files) +
                                 (summary.unused_files == 1 ? L" file isn’t used" : L" files aren’t used");
      y = Banner(ui, x, y, width, 1, title, body) + 12.0f;
    }
    if (summary.truncated) {
      y = Banner(ui, x, y, width, 1, L"The Mods folder has too many files to count", L"") + 12.0f;
    }
  }
  if (!mods_.message.empty()) y = Banner(ui, x, y, width, mods_.message_status, mods_.message, L"") + 12.0f;
  return y - 12.0f;
}

float App::BuildShortcuts(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, L"Shortcuts", L"");
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  D2D1_RECT_F control{};

  y = SectionTitle(ui, x, y, width, L"Elixir");
  BeginOptions();
  float cy = y;
  cy = SettingRow(ui, cx, cy, cw, L"Desktop shortcut", L"", 96.0f, 32.0f, &control, true);
  bool desktop = shortcuts_.desktop;
  if (ui.Toggle(L"shortcuts.desktop", control, &desktop, L"Desktop shortcut", shortcuts_.loaded)) {
    Defer([this, desktop] { SetElixirShortcut(true, desktop); });
  }
  cy = SettingRow(ui, cx, cy, cw, L"Start menu shortcut", L"", 96.0f, 32.0f, &control, false);
  bool start_menu = shortcuts_.start_menu;
  if (ui.Toggle(L"shortcuts.start", control, &start_menu, L"Start menu shortcut", shortcuts_.loaded)) {
    Defer([this, start_menu] { SetElixirShortcut(false, start_menu); });
  }
  y = EndOptions(cy) + 24.0f;

  y = SectionTitle(ui, x, y, width, L"Game shortcuts");
  const int card = ui.BeginCard(x, y, width);
  cy = y + 18.0f;
  {
    const float add_width = ui.ButtonWidth(L"Add", glyph::kAdd);
    ui.TextField(L"shortcuts.game.link", D2D1::RectF(cx, cy, cx + cw - add_width - 8.0f, cy + 32.0f),
                 &shortcuts_.game_link, L"Game link or place ID", L"Game link or place ID");
    const bool valid = !shortcuts_.working && ParsePlaceId(shortcuts_.game_link, nullptr);
    if (ui.Button(L"shortcuts.game.add", D2D1::RectF(cx + cw - add_width, cy, cx + cw, cy + 32.0f), L"Add",
                  ButtonStyle::kStandard, valid, glyph::kAdd, L"Puts a shortcut to this game on your desktop")) {
      Defer([this] { CreateGameShortcut(); });
    }
    cy += 32.0f + 8.0f;
    static constexpr wchar_t kHint[] =
        L"The shortcut takes the game’s name and icon, and follows them when they change. Type a name to keep your own.";
    const float hint_height = ui.Text(cx, cy, cw, kHint, TextStyle::kCaption, theme::kTextTertiary);
    ui.Label(D2D1::RectF(cx, cy, cx + cw, cy + hint_height), kHint);
    cy += hint_height;
  }

  const std::vector<GameShortcutEntry>& games = shortcuts_.games;
  struct Line {
    int game = -1;
    const ListMotion::Row* ghost = nullptr;
  };
  std::vector<Line> lines;
  for (std::size_t i = 0; i < games.size(); ++i) lines.push_back(Line{static_cast<int>(i), nullptr});
  if (ui.painting()) {
    for (const ListMotion::Row& ghost : shortcuts_.motion.leaving) {
      lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(std::min(ghost.index, lines.size())),
                   Line{-1, &ghost});
    }
  }
  if (!lines.empty()) cy += 12.0f;
  constexpr float kRowHeight = 48.0f;
  const float remove_width = ui.ButtonWidth(L"Remove", glyph::kDelete);
  std::vector<std::uint32_t> ended;
  for (const Line& line : lines) {
    float shown = 1.0f;
    const ListMotion::Row* entering =
        line.ghost == nullptr
            ? shortcuts_.motion.Entering(FileNameOf(games[static_cast<std::size_t>(line.game)].path))
            : nullptr;
    if (line.ghost != nullptr) {
      shown = ui.Animate(L"shortcuts.out." + std::to_wstring(line.ghost->serial), 0.0f, theme::kListMs, 1.0f);
      if (shown <= 0.0f) ended.push_back(line.ghost->serial);
    } else if (entering != nullptr) {
      shown = ui.Animate(L"shortcuts.in." + std::to_wstring(entering->serial), 1.0f, theme::kListMs, 0.0f);
      if (ui.painting() && shown >= 1.0f) ended.push_back(entering->serial);
    }
    const bool moving = shown < 1.0f || line.ghost != nullptr;
    if (moving) {
      ui.PushClip(D2D1::RectF(x, cy, x + width, cy + kRowHeight * shown));
      ui.PushLayer(shown, 0.0f);
    }
    ui.Line(cx, cy, cx + cw, cy, theme::White(0.06f));
    const D2D1_RECT_F icon = D2D1::RectF(cx, cy + 8.0f, cx + 32.0f, cy + 40.0f);
    const D2D1_RECT_F field = D2D1::RectF(cx + 44.0f, cy + 8.0f, cx + cw - remove_width - 12.0f, cy + 40.0f);
    if (line.ghost != nullptr) {
      ui.Fill(icon, theme::White(0.06f), 6.0f);
      ui.Image(icon, line.ghost->detail);
      ui.TextLine(D2D1::RectF(field.left + 11.0f, field.top, field.right, field.bottom), line.ghost->text,
                  TextStyle::kBody, theme::kText);
      ui.PopLayer();
      ui.PopClip();
      cy += kRowHeight * shown;
      continue;
    }
    const GameShortcutEntry& game = games[static_cast<std::size_t>(line.game)];
    const std::wstring index = std::to_wstring(line.game);
    ui.Fill(icon, theme::White(0.06f), 6.0f);
    if (game.icon.empty()) {
      ui.Glyph(cx + 16.0f, cy + 24.5f, glyph::kGame, TextStyle::kIcon, theme::kTextSecondary);
    } else {
      ui.Image(icon, game.icon);
    }
    ui.Label(D2D1::RectF(cx, cy, field.right, cy + kRowHeight),
             game.title.empty() || game.title == game.name ? game.name : game.name + L", " + game.title);
    std::wstring name = game.name;
    if (ui.TextField(L"shortcuts.game.name#" + index, field, &name, game.title.empty() ? L"Name" : game.title,
                     L"Name of the shortcut " + game.name, !shortcuts_.working)) {
      name = TrimWide(name);
      if (name != game.name) {
        const std::wstring path = game.path;
        Defer([this, path, name] { RenameGameShortcutTo(path, name); });
      }
    }
    if (ui.Button(L"shortcuts.game.remove#" + index,
                  D2D1::RectF(cx + cw - remove_width, cy + 7.0f, cx + cw, cy + 7.0f + theme::kButtonHeight), L"Remove",
                  ButtonStyle::kSubtle, !shortcuts_.working, glyph::kDelete, L"Remove the shortcut " + game.name)) {
      const std::wstring path = game.path;
      Defer([this, path] { RemoveGameShortcut(path); });
    }
    if (moving) {
      ui.PopLayer();
      ui.PopClip();
    }
    cy += kRowHeight * shown;
  }
  if (ui.painting() && !ended.empty()) shortcuts_.motion.Finish(ended);
  y = ui.EndCard(card, lines.empty() ? cy : cy - 10.0f);
  if (!shortcuts_.message.empty()) y = Banner(ui, x, y + 12.0f, width, 2, shortcuts_.message, L"");
  return y;
}

float App::BuildAbout(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, L"About", L"");

  const float hero_height = 168.0f;
  ui.FillGradient(D2D1::RectF(x, y, x + width, y + hero_height), theme::kHeroTop, theme::kHeroBottom, 10.0f);
  ui.Stroke(D2D1::RectF(x, y, x + width, y + hero_height), theme::White(0.10f), 10.0f);
  const float logo_height = 64.0f;
  const float logo_width = std::round(logo_height * 886.0f / 320.0f);
  ui.Logo(D2D1::RectF(x + 28.0f, y + 22.0f, x + 28.0f + logo_width, y + 22.0f + logo_height));
  ui.Text(x + 32.0f, y + 98.0f, width - 64.0f, std::wstring(L"Version ") + kVersion + L" · Apache License 2.0",
          TextStyle::kSecondary, theme::kTextSecondary);
  ui.Text(x + 32.0f, y + 122.0f, width - 64.0f,
          L"Elixir lets users experience the Android version of Roblox on their Windows desktops.",
          TextStyle::kSecondary, theme::kTextSecondary);
  ui.Label(D2D1::RectF(x, y, x + width, y + hero_height), std::wstring(L"Elixir version ") + kVersion + L".");
  y += hero_height + 16.0f;

  {
    struct LinkButton {
      const wchar_t* id;
      const wchar_t* label;
      wchar_t glyph;
      const wchar_t* url;
    };
    const LinkButton links[] = {
        {L"about.help", L"Elixir on GitHub", glyph::kOpenExternal, L"https://github.com/angie-74/Elixir"},
        {L"about.mocktail", L"Mocktail on GitHub", glyph::kOpenExternal, L"https://github.com/komaruworld/mocktail"},
        {L"about.fishstrap", L"Fishstrap on GitHub", glyph::kOpenExternal, L"https://github.com/fishstrap/fishstrap"},
        {L"about.bloxstrap", L"Bloxstrap on GitHub", glyph::kOpenExternal,
         L"https://github.com/bloxstraplabs/bloxstrap"},
    };
    float bx = x;
    float by = y;
    for (const LinkButton& link : links) {
      const float button_width = ui.ButtonWidth(link.label, link.glyph);
      if (bx != x && bx + button_width > x + width) {
        bx = x;
        by += theme::kButtonHeight + 8.0f;
      }
      if (ui.Button(link.id, D2D1::RectF(bx, by, bx + button_width, by + theme::kButtonHeight), link.label,
                    ButtonStyle::kStandard, true, link.glyph)) {
        const std::wstring url = link.url;
        Defer([this, url] { OpenPath(url); });
      }
      bx += button_width + 8.0f;
    }
    y = by + theme::kButtonHeight + 20.0f;
  }

  const std::vector<std::wstring> tabs = {L"Credits", L"Licenses"};
  about_tab_ = std::clamp(about_tab_, 0, 1);
  ui.Tabs(L"about.tab", x, y, tabs, &about_tab_, L"About section");
  y += 32.0f + 16.0f;
  return about_tab_ == 0 ? BuildAboutCredits(ui, x, y, width) : BuildAboutLicenses(ui, x, y, width);
}

float App::BuildAboutCredits(Ui& ui, float x, float y, float width) {
  y += ui.Text(x, y, width, L"Contributors", TextStyle::kCardTitle, theme::kText) + 12.0f;

  const bool columns = width >= 720.0f;
  const float gap = 12.0f;
  const float column_width = columns ? std::floor((width - 2.0f * gap) / 3.0f) : width;
  float bottom = y;
  const auto column = [&](int index, const wchar_t* title, wchar_t glyph, const auto& fill) {
    const float left = columns ? x + static_cast<float>(index) * (column_width + gap) : x;
    const float top = columns ? y : bottom + (index == 0 ? 0.0f : gap);
    const int card = ui.BeginCard(left, top, column_width);
    const float cx = left + theme::kCardPadding;
    const float cw = column_width - 2.0f * theme::kCardPadding;
    float cy = top + 16.0f;
    ui.Glyph(cx + 8.0f, cy + 10.0f, glyph, TextStyle::kIconSmall, theme::kTextSecondary);
    cy += ui.Text(cx + 26.0f, cy, cw - 26.0f, title, TextStyle::kBody, theme::kText) + 10.0f;
    cy = fill(cx, cy, cw);
    const float card_bottom = ui.EndCard(card, cy + 4.0f);
    bottom = columns ? std::max(bottom, card_bottom) : card_bottom;
  };
  const auto credits = [&](const Credit* list, std::size_t count, const wchar_t* prefix) {
    return [&ui, this, list, count, prefix](float cx, float cy, float cw) {
      for (std::size_t i = 0; i < count; ++i) {
        const Credit& credit = list[i];
        if (credit.url[0] != L'\0') {
          if (ui.Link(std::wstring(prefix) + std::to_wstring(i), cx, cy, credit.name)) {
            const std::wstring url = credit.url;
            Defer([this, url] { OpenPath(url); });
          }
          cy += 20.0f;
        } else {
          cy += ui.Text(cx, cy, cw, credit.name, TextStyle::kBody, theme::kText);
        }
        if (credit.note[0] != L'\0') {
          cy += ui.Text(cx, cy, cw, credit.note, TextStyle::kCaption, theme::kTextTertiary);
        }
        cy += 8.0f;
      }
      return cy;
    };
  };
  column(0, L"Code", glyph::kDocument, [&ui](float cx, float cy, float cw) {
    cy += ui.Text(cx, cy, cw, L"Elixir Project Authors", TextStyle::kBody, theme::kText) + 10.0f;
    cy += ui.Text(cx, cy, cw, L"Mocktail contributors", TextStyle::kCaption, theme::kTextTertiary) + 4.0f;
    std::wstring names;
    for (const wchar_t* author : kMocktailAuthors) {
      if (!names.empty()) names += L", ";
      names += author;
    }
    cy += ui.Text(cx, cy, cw, names, TextStyle::kBody, theme::kTextSecondary);
    ui.Label(D2D1::RectF(cx, cy - 20.0f, cx + cw, cy), L"Mocktail contributors: " + names);
    return cy + 8.0f;
  });
  column(1, L"Design and features", glyph::kEdit, credits(kDesignCredits, std::size(kDesignCredits), L"about.design#"));
  column(2, L"Special thanks", glyph::kCheck, credits(kThanksCredits, std::size(kThanksCredits), L"about.thanks#"));
  return bottom;
}

float App::BuildAboutLicenses(Ui& ui, float x, float y, float width) {
  LoadNotices();
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;

  int card = ui.BeginCard(x, y, width);
  float cy = y + 18.0f;
  cy += ui.Text(cx, cy, cw, L"Elixir", TextStyle::kCardTitle, theme::kText) + 8.0f;
  cy += ui.Text(cx, cy, cw, L"Apache License 2.0. Third-party components keep their own licenses.", TextStyle::kBody,
                theme::kTextSecondary);
  bool open = true;
  const auto close = [&] {
    if (open) y = ui.EndCard(card, cy + 10.0f) + 12.0f;
    open = false;
  };
  bool first = true;
  for (const NoticeBlock& block : notices_) {
    if (block.kind == NoticeKind::kTitle) continue;
    if (block.kind == NoticeKind::kComponent) {
      close();
      card = ui.BeginCard(x, y, width);
      cy = y + 16.0f;
      cy += ui.Text(cx, cy, cw, block.text, TextStyle::kBodyStrong, theme::kText) + 8.0f;
      open = true;
      first = false;
      continue;
    }
    if (first) continue;
    if (block.kind == NoticeKind::kBullet) {
      ui.Text(cx, cy, 12.0f, L"\u2022", TextStyle::kSecondary, theme::kTextTertiary);
      cy += ui.Text(cx + 14.0f, cy, cw - 14.0f, block.text, TextStyle::kSecondary, theme::kTextSecondary) + 5.0f;
    } else if (block.kind == NoticeKind::kQuote) {
      cy += 3.0f;
      cy += ui.Text(cx + 14.0f, cy, cw - 14.0f, block.text, TextStyle::kCaption, theme::kTextTertiary) + 8.0f;
    } else {
      cy += ui.Text(cx, cy, cw, block.text, TextStyle::kSecondary, theme::kTextSecondary) + 6.0f;
    }
  }
  close();

  if (!license_text_.empty()) {
    card = ui.BeginCard(x, y, width);
    cy = y + 16.0f;
    cy += ui.Text(cx, cy, cw, L"Apache License 2.0", TextStyle::kBodyStrong, theme::kText) + 8.0f;
    cy += ui.Text(cx, cy, cw, license_text_, TextStyle::kCaption, theme::kTextTertiary);
    y = ui.EndCard(card, cy + 10.0f) + 12.0f;
  }
  return y;
}

}
