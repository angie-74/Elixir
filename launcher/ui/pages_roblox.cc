// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <unordered_set>

#include "core/instances.h"
#include "core/shared_settings.h"
#include "core/util.h"
#include "core/i18n.h"
#include "resource.h"
#include "ui/app.h"
#include "ui/theme.h"

namespace elixir::launcher::ui {

namespace {

std::string DecimalSetting(float value) {
  char buffer[32];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  return std::string(buffer, result.ptr);
}

std::wstring ErrorLine(const std::wstring& error) {
  constexpr std::wstring_view kMark = L"(line ";
  const std::size_t start = error.rfind(kMark);
  if (start == std::wstring::npos) return {};
  const std::size_t end = error.find(L',', start);
  if (end == std::wstring::npos) return {};
  return error.substr(start + kMark.size(), end - start - kMark.size());
}

const std::wstring& Shown(const std::string& text) { return T(Utf8ToWide(text)); }

[[maybe_unused]] constexpr const wchar_t* kPresetTexts[] = {
    ELIXIR_TR(L"Geometry"),
    ELIXIR_TR(L"Rendering and Graphics"),
    ELIXIR_TR(L"Mesh detail"),
    ELIXIR_TR(L"Control how detailed meshes appear in-game."),
    ELIXIR_TR(L"Off"),
    ELIXIR_TR(L"On"),
    ELIXIR_TR(L"Lowest"),
    ELIXIR_TR(L"Low"),
    ELIXIR_TR(L"High"),
    ELIXIR_TR(L"Highest"),
    ELIXIR_TR(L"Lowest quality"),
    ELIXIR_TR(L"Highest quality"),
    ELIXIR_TR(L"Lower qualities may cause meshes to disappear."),
    ELIXIR_TR(L"Anti-aliasing quality (MSAA)"),
    ELIXIR_TR(L"Automatic"),
    ELIXIR_TR(L"Preserve rendering quality with display scaling"),
    ELIXIR_TR(L"Roblox reduces your rendering quality depending on how your display is scaled in Windows."),
    ELIXIR_TR(L"Graphics quality override"),
    ELIXIR_TR(L"Choose the graphics quality Roblox should use."),
    ELIXIR_TR(L"Hide grass"),
    ELIXIR_TR(L"Turns grass off, which can run faster."),
    ELIXIR_TR(L"Gray sky"),
    ELIXIR_TR(L"Replaces the sky with a plain gray one."),
    ELIXIR_TR(L"Pause lighting updates"),
    ELIXIR_TR(L"Can run faster, but lighting may not update when the world changes."),
};

class ButtonFlow {
 public:
  ButtonFlow(Ui& ui, float left, float right, float top) : ui_(ui), left_(left), right_(right), x_(left), y_(top) {}
  bool Add(std::wstring_view id, std::wstring_view label, ButtonStyle style, bool enabled, wchar_t glyph,
           std::wstring_view description = {}) {
    const float width = ui_.ButtonWidth(label, glyph);
    if (x_ + width > right_ && x_ > left_) {
      x_ = left_;
      y_ += theme::kButtonHeight + 8.0f;
    }
    const bool pressed =
        ui_.Button(id, D2D1::RectF(x_, y_, x_ + width, y_ + theme::kButtonHeight), label, style, enabled, glyph,
                   description);
    x_ += width + 8.0f;
    return pressed;
  }
  float bottom() const { return y_ + theme::kButtonHeight; }

 private:
  Ui& ui_;
  float left_;
  float right_;
  float x_;
  float y_;
};

}

namespace {

bool IsPresetFlag(std::string_view name) {
  static const std::unordered_set<std::string> names = [] {
    std::unordered_set<std::string> all;
    for (const fflags::Preset& preset : fflags::BuiltinPresets()) {
      for (const fflags::PresetOption& option : preset.options) {
        all.insert(option.remove.begin(), option.remove.end());
        for (const fflags::Flag& flag : option.set) all.insert(flag.name);
      }
    }
    return all;
  }();
  return names.count(std::string(name)) != 0;
}

constexpr wchar_t kFlagRulesAnnouncement[] =
    L"https://devforum.roblox.com/t/allowlist-for-local-client-configuration-via-fast-flags/3966569";

}

float App::FastFlagsWarning(Ui& ui, float x, float y, float width) {
  const float link_width = ui.TextWidth(T(L"Learn more"), TextStyle::kBody) + 2.0f;
  const float top = y;
  y = Banner(ui, x, y, width, 1, T(L"Warning"), T(L"Roblox only applies whitelisted FFlags."), link_width + 28.0f);
  if (ui.Link(L"fflags.learnmore", x + width - link_width - 18.0f, top + (y - top) / 2.0f - 10.0f, T(L"Learn more"))) {
    Defer([this] { OpenPath(kFlagRulesAnnouncement); });
  }
  return y;
}

float App::BuildFastFlags(Ui& ui, float x, float y, float width) {
  pending_menu_.reset();
  if (!fast_flags_.editor) LoadFastFlags(true);
  if (fast_flags_.editor_open) return BuildFastFlagEditor(ui, x, y, width);
  y = PageHeader(ui, x, y, width, T(L"Fast Flags"),
                 T(L"Control how specific Roblox engine parameters and features are configured."));

  if (!fast_flags_.load_error.empty()) {
    const float top = y;
    const std::wstring line = ErrorLine(fast_flags_.load_error);
    y = Banner(ui, x, y, width, 2, T(L"Your Fast Flags couldn’t be read"),
               line.empty() ? T(L"Start over to make a new list.")
                            : TF(L"There’s a mistake on line {0}. Start over to make a new list.", line),
               140.0f);
    if (ui.Button(L"fflags.startover", D2D1::RectF(x + width - 136.0f, top + 12.0f, x + width - 16.0f, top + 44.0f),
                  T(L"Start over"), ButtonStyle::kStandard)) {
      Overlay overlay;
      overlay.id = L"fflags.startover";
      overlay.status = 1;
      overlay.title = T(L"Start an empty list?");
      overlay.body = T(L"Your old Fast Flags are kept as a backup.");
      overlay.actions = {{L"fflags.startover.confirm", T(L"Start over"), ButtonStyle::kDanger},
                         {L"overlay.close", T(L"Cancel"), ButtonStyle::kStandard}};
      Defer([this, overlay] { ShowOverlay(overlay); });
    }
    return y;
  }
  y = FastFlagsWarning(ui, x, y, width) + 16.0f;
  if (!fast_flags_.message.empty() && fast_flags_.message_status == 2) {
    y = Banner(ui, x, y, width, 2, fast_flags_.message, L"") + 16.0f;
  }

  fflags::FlagEditor& editor = *fast_flags_.editor;
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  D2D1_RECT_F control{};

  BeginOptions();
  float cy = y;
  {
    const float button = ui.ButtonWidth(T(L"Open"), glyph::kEdit);
    cy = SettingRow(ui, cx, cy, cw, T(L"Fast Flag Editor"), T(L"Manage your own Fast Flags. Use with caution."), button,
                    32.0f, &control, true);
    if (ui.Button(L"fflags.open", control, T(L"Open"), ButtonStyle::kStandard, true, glyph::kEdit)) {
      OpenFastFlagEditor(true);
    }
  }
  cy = SettingRow(ui, cx, cy, cw, T(L"Allow Elixir to manage Fast Flags"),
                  T(L"Turning this off stops anything set here from reaching Roblox."), ui.ToggleWidth(), 32.0f, &control, false);
  if (ui.Toggle(L"fflags.manage", control, &config_.advanced.manage_fast_flags, T(L"Allow Elixir to manage Fast Flags"))) {
    SaveConfig();
  }
  {
    const float button = ui.ButtonWidth(T(L"Reset"), glyph::kReset);
    cy = SettingRow(ui, cx, cy, cw, T(L"Reset everything to defaults"), T(L"Removes every Fast Flag set here and in the editor."),
                    button, 32.0f, &control, false);
    if (ui.Button(L"fflags.reset", control, T(L"Reset"), ButtonStyle::kDanger, !editor.flags().empty(), glyph::kReset)) {
      Overlay overlay;
      overlay.id = L"fflags.reset";
      overlay.status = 1;
      overlay.title = T(L"Reset every Fast Flag?");
      overlay.body = T(L"This removes all of your Fast Flags. Roblox goes back to its own settings.");
      overlay.actions = {{L"fflags.reset.confirm", T(L"Reset"), ButtonStyle::kDanger},
                         {L"overlay.close", T(L"Cancel"), ButtonStyle::kStandard}};
      Defer([this, overlay] { ShowOverlay(overlay); });
    }
  }
  y = EndOptions(cy) + 24.0f;

  std::vector<std::string> groups;
  for (const fflags::Preset& preset : fflags::BuiltinPresets()) {
    if (std::find(groups.begin(), groups.end(), preset.group) == groups.end()) groups.push_back(preset.group);
  }
  for (const std::string& group : groups) {
    y = SectionTitle(ui, x, y, width, Shown(group));
    BeginOptions();
    cy = y;
    for (const fflags::Preset& preset : fflags::BuiltinPresets()) {
      if (preset.group == group) cy = FastFlagSetting(ui, cx, cy, cw, preset);
    }
    y = EndOptions(cy) + 24.0f;
  }
  if (pending_menu_) {
    const PendingMenu menu = std::move(*pending_menu_);
    pending_menu_.reset();
    const int chosen = MenuPanel(ui, menu.control, menu.button, menu.options, menu.selected);
    if (chosen >= 0) {
      Defer([this, preset = menu.preset, chosen, selected = menu.selected] {
        CloseMenu();
        if (chosen == selected || !fast_flags_.editor) return;
        const fflags::EditResult result = fast_flags_.editor->ApplyPreset(preset, chosen);
        if (result.ok) {
          FastFlagsChanged();
        } else {
          SetFastFlagMessage(Utf8ToWide(result.message), 2);
        }
      });
    }
  }
  return y - 24.0f;
}

float App::FastFlagSetting(Ui& ui, float x, float y, float width, const fflags::Preset& preset) {
  fflags::FlagEditor& editor = *fast_flags_.editor;
  const std::wstring id = L"fflags.preset." + Utf8ToWide(preset.id);
  const std::wstring title = Shown(preset.title);
  const int last = static_cast<int>(preset.options.size()) - 1;
  int active = editor.ActiveOption(preset.id);
  const auto apply = [&](int option) {
    const fflags::EditResult result = editor.ApplyPreset(preset.id, option);
    if (result.ok) {
      FastFlagsChanged();
    } else {
      SetFastFlagMessage(Utf8ToWide(result.message), 2);
    }
  };
  D2D1_RECT_F control{};

  if (preset.control == fflags::PresetControl::kToggle) {
    bool on = active != 0;
    const float bottom =
        SettingRow(ui, x, y, width, title, Shown(preset.description), ui.ToggleWidth(), 32.0f, &control, true);
    if (ui.Toggle(id, control, &on, title)) apply(on ? last : 0);
    return bottom;
  }

  if (preset.control == fflags::PresetControl::kChoice) {
    const float bottom =
        SettingRow(ui, x, y, width, title, Shown(preset.description), std::min(200.0f, width), 32.0f, &control, true);
    const std::wstring label = active < 0 ? std::wstring(T(L"Custom")) : Shown(preset.options[active].label);
    const bool open = MenuOpen(id);
    if (ui.ComboBox(id, control, title, label, open, true)) {
      Defer([this, id, open, active, count = last + 1] { SetMenu(id, open ? -1 : std::max(0, active), count); });
    }
    if (open) {
      PendingMenu menu;
      menu.control = id;
      menu.button = control;
      menu.selected = active;
      menu.preset = preset.id;
      for (const fflags::PresetOption& option : preset.options) menu.options.push_back(Shown(option.label));
      pending_menu_ = std::move(menu);
    }
    return bottom;
  }

  const bool standalone = preset.control == fflags::PresetControl::kSlider;
  const int first = standalone ? 0 : 1;
  const auto slider_row = [&](D2D1_RECT_F row) {
    const std::wstring low = Shown(preset.low_label);
    const std::wstring high = Shown(preset.high_label);
    const float low_width = low.empty() ? 0.0f : ui.TextWidth(low, TextStyle::kBody) + 8.0f;
    const float high_width = high.empty() ? 0.0f : ui.TextWidth(high, TextStyle::kBody) + 8.0f;
    float box_width = 0.0f;
    if (preset.show_value) {
      box_width = 30.0f;
      for (int option = first; option <= last; ++option) {
        box_width = std::max(box_width, ui.TextWidth(Shown(preset.options[option].label), TextStyle::kBody) + 12.0f);
      }
      if (active < 0) box_width = std::max(box_width, ui.TextWidth(T(L"Custom"), TextStyle::kBody) + 12.0f);
    }
    if (!low.empty()) ui.TextLine(D2D1::RectF(row.left, row.top, row.left + low_width, row.bottom), low, TextStyle::kBody,
                                  theme::kText);
    const float slider_right = row.right - box_width - (box_width > 0.0f ? 8.0f : 0.0f) - high_width;
    const int position = active >= first ? active : last;
    double value = position;
    const std::wstring label = active < 0 ? std::wstring(T(L"Custom")) : Shown(preset.options[position].label);
    if (ui.Slider(id + L".slider", D2D1::RectF(row.left + low_width, row.top, slider_right, row.bottom), &value,
                  first, last, 1.0, standalone ? title : TF(L"{0} level", title), label, true, 0.0f)) {
      apply(static_cast<int>(std::lround(value)));
    }
    if (!high.empty()) {
      ui.TextLine(D2D1::RectF(slider_right + 8.0f, row.top, slider_right + high_width, row.bottom), high,
                  TextStyle::kBody, theme::kText);
    }
    if (box_width > 0.0f) {
      const D2D1_RECT_F box = D2D1::RectF(row.right - box_width, row.top + 2.0f, row.right, row.bottom - 2.0f);
      ui.Fill(box, theme::kValueBox, 4.0f);
      ui.TextLine(box, label, TextStyle::kBody, theme::kText, Align::kCenter);
    }
    return row.bottom;
  };

  if (standalone) {
    const float bottom = SettingRow(ui, x, y, width, title, Shown(preset.description),
                                    std::min(360.0f, width), 32.0f, &control, true);
    slider_row(control);
    return bottom;
  }

  bool on = active != 0;
  const bool cards = option_cards_;
  option_cards_ = false;
  const int card = ui.BeginCard(x - theme::kCardPadding, y, width + 2.0f * theme::kCardPadding);
  float bottom = SettingRow(ui, x, y, width, title, Shown(preset.description), ui.ToggleWidth(), 32.0f, &control, true);
  if (ui.Toggle(id, control, &on, title)) {
    apply(on ? last : 0);
    active = editor.ActiveOption(preset.id);
  }
  if (on) {
    ui.Fill(D2D1::RectF(x - theme::kCardPadding, bottom, x + width + theme::kCardPadding, bottom + 1.0f),
            theme::kDivider);
    bottom += 12.0f;
    if (!preset.note.empty()) {
      bottom += ui.Text(x, bottom, width, Shown(preset.note), TextStyle::kCaption, theme::kTextTertiary,
                        Align::kCenter) + 10.0f;
    }
    bottom = slider_row(D2D1::RectF(x, bottom, x + width, bottom + 32.0f)) + 16.0f;
  }
  ui.EndCard(card, bottom - theme::kCardPadding);
  option_cards_ = cards;
  return bottom + (cards ? theme::kOptionGap : 0.0f);
}

float App::BuildFastFlagEditor(Ui& ui, float x, float y, float width) {
  fflags::FlagEditor& editor = *fast_flags_.editor;
  y = PageHeader(ui, x, y, width, T(L"Fast Flag Editor"), T(L"Manage your own Fast Flags. Click a name or value to edit it."));
  y = FastFlagsWarning(ui, x, y, width) + 16.0f;

  std::vector<fflags::EditorRow> rows;
  for (fflags::EditorRow& row : editor.Rows(WideToUtf8(fast_flags_.search), fflags::RowFilter::kAll)) {
    if (fast_flags_.show_preset_flags || !IsPresetFlag(row.name)) rows.push_back(std::move(row));
  }
  for (auto it = fast_flags_.selected.begin(); it != fast_flags_.selected.end();) {
    it = editor.flags().Contains(*it) ? std::next(it) : fast_flags_.selected.erase(it);
  }

  {
    ButtonFlow flow(ui, x, x + width, y);
    if (flow.Add(L"fflags.back", T(L"Back"), ButtonStyle::kStandard, true, glyph::kBack)) OpenFastFlagEditor(false);
    if (flow.Add(L"fflags.add", T(L"Add new"), ButtonStyle::kStandard, true, glyph::kAdd)) {
      fast_flags_.adding = true;
      Defer([this] { SetFocusTo(L"fflags.new.name", true); });
    }
    const std::size_t selected = fast_flags_.selected.size();
    if (flow.Add(L"fflags.delete", T(L"Delete selected"), ButtonStyle::kDanger, selected > 0, glyph::kDelete)) {
      fast_flags_.motion.leaving.clear();
      for (std::size_t i = 0; i < rows.size(); ++i) {
        if (fast_flags_.selected.count(rows[i].name) == 0) continue;
        fast_flags_.motion.Leave(Utf8ToWide(rows[i].name), i, Utf8ToWide(rows[i].value));
      }
      editor.RemoveFlags(std::vector<std::string>(fast_flags_.selected.begin(), fast_flags_.selected.end()));
      fast_flags_.selected.clear();
      fast_flags_.message.clear();
      FastFlagsChanged();
    }
    if (flow.Add(L"fflags.presetflags", fast_flags_.show_preset_flags ? T(L"Hide preset flags") : T(L"Show preset flags"),
                 ButtonStyle::kStandard, true, fast_flags_.show_preset_flags ? glyph::kEyeOff : glyph::kEye)) {
      fast_flags_.show_preset_flags = !fast_flags_.show_preset_flags;
    }
    if (flow.Add(L"fflags.import", T(L"Import JSON"), ButtonStyle::kStandard, true, glyph::kImport)) {
      Overlay overlay;
      overlay.id = L"fflags.import";
      overlay.status = 4;
      overlay.title = T(L"Import JSON");
      overlay.body = T(L"Add Fast Flags from JSON you copied, or from a file. Flags you already have get the new values.");
      overlay.actions = {{L"fflags.import.paste", T(L"Paste"), ButtonStyle::kStandard},
                         {L"fflags.import.file", T(L"Open file…"), ButtonStyle::kStandard},
                         {L"overlay.close", T(L"Cancel"), ButtonStyle::kStandard}};
      Defer([this, overlay] { ShowOverlay(overlay); });
    }
    if (flow.Add(L"fflags.export", T(L"Export JSON"), ButtonStyle::kStandard, !editor.flags().empty(), glyph::kExport)) {
      Overlay overlay;
      overlay.id = L"fflags.export";
      overlay.status = 4;
      overlay.title = T(L"Export JSON");
      overlay.body = T(L"Copy your Fast Flags as JSON, or save them to a file.");
      overlay.actions = {{L"fflags.export.copy", T(L"Copy"), ButtonStyle::kStandard},
                         {L"fflags.export.file", T(L"Save file…"), ButtonStyle::kStandard},
                         {L"overlay.close", T(L"Cancel"), ButtonStyle::kStandard}};
      Defer([this, overlay] { ShowOverlay(overlay); });
    }
    y = flow.bottom() + 12.0f;
  }
  if (!fast_flags_.message.empty()) {
    const float height = ui.Text(x, y, width, fast_flags_.message, TextStyle::kSecondary,
                                 fast_flags_.message_status == 2 ? theme::kFail : theme::kWarn);
    ui.Label(D2D1::RectF(x, y, x + width, y + height), fast_flags_.message);
    y += std::max(20.0f, height) + 12.0f;
  }
  ui.TextField(L"fflags.search", D2D1::RectF(x, y, x + width, y + 34.0f), &fast_flags_.search, T(L"Search"),
               T(L"Search Fast Flags"));
  y += 34.0f + 12.0f;

  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  if (fast_flags_.adding) {
    const int card = ui.BeginCard(x, y, width);
    float cy = y + 16.0f;
    const float add_width = ui.ButtonWidth(T(L"Add"), glyph::kAdd);
    const float cancel_width = ui.ButtonWidth(T(L"Cancel"));
    const float value_width = std::max(140.0f, std::floor((cw - add_width - cancel_width - 24.0f) * 0.34f));
    const float name_width = cw - add_width - cancel_width - value_width - 24.0f;
    ui.TextField(L"fflags.new.name", D2D1::RectF(cx, cy, cx + name_width, cy + 32.0f), &fast_flags_.new_name, T(L"Name"),
                 T(L"New flag name"));
    ui.TextField(L"fflags.new.value",
                 D2D1::RectF(cx + name_width + 8.0f, cy, cx + name_width + 8.0f + value_width, cy + 32.0f),
                 &fast_flags_.new_value, T(L"Value"), T(L"New flag value"));
    const float buttons = cx + name_width + value_width + 16.0f;
    if (ui.Button(L"fflags.new.add", D2D1::RectF(buttons, cy, buttons + add_width, cy + 32.0f), T(L"Add"),
                  ButtonStyle::kStandard, !TrimWide(fast_flags_.new_name).empty(), glyph::kAdd)) {
      AddFastFlag();
    }
    if (ui.Button(L"fflags.new.cancel",
                  D2D1::RectF(buttons + add_width + 8.0f, cy, buttons + add_width + 8.0f + cancel_width, cy + 32.0f),
                  T(L"Cancel"), ButtonStyle::kStandard)) {
      fast_flags_.adding = false;
      fast_flags_.new_name.clear();
      fast_flags_.new_value.clear();
    }
    y = ui.EndCard(card, cy + 32.0f) + 12.0f;
  }

  const int card = ui.BeginCard(x, y, width);
  float cy = y + 12.0f;
  const float check_width = 32.0f;
  const float value_width = std::max(140.0f, std::floor((cw - check_width) * 0.34f));
  const float name_width = cw - check_width - value_width - 8.0f;
  ui.TextLine(D2D1::RectF(cx + check_width, cy, cx + check_width + name_width, cy + 24.0f), T(L"Name"),
              TextStyle::kCaptionStrong, theme::kTextTertiary);
  ui.TextLine(D2D1::RectF(cx + check_width + name_width + 8.0f, cy, cx + cw, cy + 24.0f), T(L"Value"),
              TextStyle::kCaptionStrong, theme::kTextTertiary);
  cy += 30.0f;
  struct Line {
    int row = -1;
    const ListMotion::Row* ghost = nullptr;
  };
  std::vector<Line> lines;
  for (std::size_t i = 0; i < rows.size(); ++i) lines.push_back(Line{static_cast<int>(i), nullptr});
  if (ui.painting()) {
    for (const ListMotion::Row& ghost : fast_flags_.motion.leaving) {
      lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(std::min(ghost.index, lines.size())),
                   Line{-1, &ghost});
    }
  }
  if (lines.empty()) {
    ui.Fill(D2D1::RectF(cx, cy - 4.0f, cx + cw, cy - 3.0f), theme::kDivider);
    const std::wstring empty = editor.flags().empty() ? T(L"No Fast Flags yet.") : T(L"No Fast Flags match.");
    cy += 8.0f + ui.Text(cx, cy + 8.0f, cw, empty, TextStyle::kSecondary, theme::kTextTertiary);
    return ui.EndCard(card, cy + 4.0f);
  }
  constexpr float kRowHeight = 40.0f;
  std::vector<std::uint32_t> ended;
  for (const Line& line : lines) {
    float shown = 1.0f;
    const ListMotion::Row* entering =
        line.ghost == nullptr ? fast_flags_.motion.Entering(Utf8ToWide(rows[static_cast<std::size_t>(line.row)].name))
                              : nullptr;
    if (line.ghost != nullptr) {
      shown = ui.Animate(L"fflags.out." + std::to_wstring(line.ghost->serial), 0.0f, theme::kListMs, 1.0f);
      if (shown <= 0.0f) ended.push_back(line.ghost->serial);
    } else if (entering != nullptr) {
      shown = ui.Animate(L"fflags.in." + std::to_wstring(entering->serial), 1.0f, theme::kListMs, 0.0f);
      if (ui.painting() && shown >= 1.0f) ended.push_back(entering->serial);
    }
    const bool moving = shown < 1.0f || line.ghost != nullptr;
    if (moving) {
      ui.PushClip(D2D1::RectF(x, cy - 4.0f, x + width, cy - 4.0f + kRowHeight * shown));
      ui.PushLayer(shown, 0.0f);
    }
    ui.Fill(D2D1::RectF(cx, cy - 4.0f, cx + cw, cy - 3.0f), theme::kDivider);
    if (line.ghost != nullptr) {
      const bool inert = ui.inert();
      ui.SetInert(true);
      const std::wstring ghost_id = L"fflags.gone." + std::to_wstring(line.ghost->serial);
      bool checked = true;
      std::wstring gone_name = line.ghost->key;
      std::wstring gone_value = line.ghost->text;
      ui.CheckBox(ghost_id + L".select", D2D1::RectF(cx, cy, cx + 24.0f, cy + 32.0f), &checked, L"");
      ui.TextField(ghost_id + L".name", D2D1::RectF(cx + check_width, cy, cx + check_width + name_width, cy + 32.0f),
                   &gone_name, L"", L"");
      ui.TextField(ghost_id + L".value", D2D1::RectF(cx + check_width + name_width + 8.0f, cy, cx + cw, cy + 32.0f),
                   &gone_value, L"", L"");
      ui.SetInert(inert);
      ui.PopLayer();
      ui.PopClip();
      cy += kRowHeight * shown;
      continue;
    }
    const std::size_t i = static_cast<std::size_t>(line.row);
    const fflags::EditorRow& row = rows[i];
    const std::wstring index = std::to_wstring(i);
    const std::wstring name = Utf8ToWide(row.name);
    bool selected = fast_flags_.selected.count(row.name) > 0;
    if (ui.CheckBox(L"fflags.row." + index + L".select", D2D1::RectF(cx, cy, cx + 24.0f, cy + 32.0f), &selected,
                    TF(L"Select {0}", name))) {
      if (selected) {
        fast_flags_.selected.insert(row.name);
      } else {
        fast_flags_.selected.erase(row.name);
      }
    }
    std::wstring name_text = name;
    if (ui.TextField(L"fflags.row." + index + L".name",
                     D2D1::RectF(cx + check_width, cy, cx + check_width + name_width, cy + 32.0f), &name_text,
                     T(L"Name"), TF(L"Name of {0}", name))) {
      const fflags::EditResult result = editor.RenameFlag(row.name, WideToUtf8(name_text));
      if (result.ok) {
        if (fast_flags_.selected.erase(row.name) > 0) fast_flags_.selected.insert(WideToUtf8(TrimWide(name_text)));
        FastFlagsChanged();
      } else {
        SetFastFlagMessage(Utf8ToWide(result.message), 2);
      }
    }
    std::wstring value_text = Utf8ToWide(row.value);
    if (ui.TextField(L"fflags.row." + index + L".value",
                     D2D1::RectF(cx + check_width + name_width + 8.0f, cy, cx + cw, cy + 32.0f), &value_text,
                     T(L"Value"), TF(L"Value of {0}", name))) {
      const fflags::EditResult result = editor.SetFlag(row.name, WideToUtf8(value_text));
      if (result.ok) {
        FastFlagsChanged();
      } else {
        SetFastFlagMessage(Utf8ToWide(result.message), 2);
      }
    }
    if (moving) {
      ui.PopLayer();
      ui.PopClip();
    }
    cy += kRowHeight * shown;
  }
  if (ui.painting() && !ended.empty()) fast_flags_.motion.Finish(ended);
  return ui.EndCard(card, cy - 4.0f);
}

void App::RefreshGlobalSettings() {
  ShareSettingsWith(paths_, paths_);
  GlobalSettings settings;
  global_settings_error_.clear();
  if (LoadGlobalSettings(paths_, &settings, &global_settings_error_)) {
    global_settings_ = settings;
  } else {
    global_settings_.reset();
  }
  global_data_in_use_ = DataFolderInUse(paths_.data_root);
  if (MenuOpen(kTextSizeControl) &&
      (!global_settings_ || global_settings_->read_only || global_data_in_use_ || runtime_.running)) {
    CloseMenu();
  }
  global_quality_override_.clear();
  if (config_.advanced.manage_fast_flags) {
    const auto flags = fflags::LoadFlagsFile(std::filesystem::path(FflagsPath()));
    if (const auto* level = flags.flags.Find("DFIntDebugFRMQualityLevelOverride"); flags.ok && level) {
      int number = 0;
      const auto parsed = std::from_chars(level->data(), level->data() + level->size(), number);
      if (parsed.ec == std::errc{} && parsed.ptr == level->data() + level->size() && number > 0) {
        global_quality_override_ = std::to_wstring(number);
      }
    }
  }
  Invalidate();
}

void App::SaveGlobalValue(GlobalSetting setting, std::string value) {
  std::wstring error;
  const bool saved = SaveGlobalSetting(paths_, setting, value, &error);
  RefreshGlobalSettings();
  if (!saved) ShowToast(error);
}

float App::BuildGlobalSettings(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, T(L"Global Settings"), T(L"In-game preferences, shared by all your accounts."));
  if (!global_settings_) {
    const float top = y;
    const float button_width = ui.ButtonWidth(T(L"Refresh"), glyph::kRefresh);
    y = Banner(ui, x, y, width, 1, T(L"Game settings unavailable"), global_settings_error_, button_width + 12.0f);
    const float right = x + width - theme::kCardPadding;
    if (ui.Button(L"global.refresh", D2D1::RectF(right - button_width, top + 12.0f, right, top + 44.0f),
                  T(L"Refresh"), ButtonStyle::kStandard, true, glyph::kRefresh)) RefreshGlobalSettings();
    return y;
  }

  const GlobalSettings settings = *global_settings_;
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  const bool in_use = global_data_in_use_ || runtime_.running;
  const bool editable = !settings.read_only && !in_use;
  D2D1_RECT_F control{};
  if (in_use) {
    y = Banner(ui, x, y, width, 4, T(L"Roblox is using this profile"), T(L"Close its game window to edit these settings.")) + 16.0f;
  }

  BeginOptions();
  y = SettingRow(ui, cx, y, cw, T(L"Read-only settings file"),
                 settings.read_only ? T(L"Turn this off to edit the settings.") : T(L"Prevent Roblox from overwriting these preferences."),
                 ui.ToggleWidth(), 32.0f, &control, true);
  bool locked = settings.read_only;
  if (ui.Toggle(L"global.readonly", control, &locked, T(L"Read-only settings file"), !in_use)) {
    std::wstring error;
    const bool saved = SetGlobalSettingsReadOnly(paths_, locked, &error);
    RefreshGlobalSettings();
    if (!saved) ShowToast(error);
  }
  y = EndOptions(y) + 24.0f;

  y = SectionTitle(ui, x, y, width, T(L"Graphics"));
  BeginOptions();
  double quality = settings.quality_level.value_or(0);
  const std::wstring quality_text = settings.quality_level
      ? (quality == 0 ? T(L"Auto") : std::to_wstring(*settings.quality_level)) : T(L"Not set");
  const std::wstring quality_hint = global_quality_override_.empty()
      ? T(L"Choose Auto or a quality level from 1 to 10.")
      : TF(L"Fast Flags overrides this preference with level {0}.", global_quality_override_);
  y = SettingRow(ui, cx, y, cw, T(L"Graphics quality"), quality_hint, std::min(300.0f, cw), 32.0f, &control, true);
  if (ui.Slider(L"global.quality", control, &quality, 0, 10, 1, T(L"Graphics quality"), quality_text, editable)) {
    SaveGlobalValue(GlobalSetting::kQualityLevel, std::to_string(static_cast<int>(quality)));
  }

  std::wstring fps = settings.framerate_cap && *settings.framerate_cap > 0
      ? std::to_wstring(*settings.framerate_cap) : L"";
  y = SettingRow(ui, cx, y, cw, T(L"Framerate cap"), T(L"Frames per second. Clear the field to use the default."),
                 145.0f, 32.0f, &control, false);
  if (ui.TextField(L"global.fps", control, &fps, settings.framerate_cap ? T(L"Default") : T(L"Not set"), T(L"Framerate cap"), editable)) {
    fps = TrimWide(fps);
    SaveGlobalValue(GlobalSetting::kFramerateCap, fps.empty() ? "-1" : WideToUtf8(fps));
  }
  y = EndOptions(y) + 24.0f;

  y = SectionTitle(ui, x, y, width, T(L"Interface"));
  BeginOptions();
  double transparency = std::round(settings.transparency.value_or(0.0f) * 1000000.0) / 1000000.0;
  const std::wstring transparency_text = settings.transparency
      ? std::to_wstring(std::lround(transparency * 100)) + L"%" : T(L"Not set");
  y = SettingRow(ui, cx, y, cw, T(L"Background transparency"), T(L"Transparency of supported Roblox interface backgrounds."),
                 std::min(300.0f, cw), 32.0f, &control, true);
  if (ui.Slider(L"global.transparency", control, &transparency, 0.0, 1.0, 0.1, T(L"Background transparency"),
                transparency_text, editable)) {
    SaveGlobalValue(GlobalSetting::kTransparency, DecimalSetting(static_cast<float>(transparency)));
  }

  bool motion = settings.reduced_motion.value_or(false);
  y = SettingRow(ui, cx, y, cw, T(L"Reduced motion"),
                 settings.reduced_motion ? T(L"Reduce interface animations in Roblox.") : T(L"No preference saved."),
                 ui.ToggleWidth(), 32.0f, &control, false);
  if (ui.Toggle(L"global.motion", control, &motion, T(L"Reduced motion"), editable)) {
    SaveGlobalValue(GlobalSetting::kReducedMotion, motion ? "true" : "false");
  }

  D2D1_RECT_F size_button{};
  y = SettingRow(ui, cx, y, cw, T(L"Text size"), T(L"Medium is the default text size."), 145.0f, 32.0f, &size_button, false);
  const int size_index = settings.text_size.value_or(0) - 1;
  const bool menu_open = MenuOpen(kTextSizeControl) && editable;
  if (ui.ComboBox(kTextSizeControl, size_button, T(L"Text size"),
                  size_index >= 0 ? T(kTextSizes[size_index]) : T(L"Not set"),
                  menu_open, editable)) {
    Defer([this, menu_open, size_index] {
      SetMenu(kTextSizeControl, menu_open ? -1 : std::max(0, size_index), static_cast<int>(kTextSizes.size()));
    });
  }
  y = EndOptions(y) + 24.0f;

  y = SectionTitle(ui, x, y, width, T(L"Input"));
  BeginOptions();
  std::wstring sensitivity = settings.mouse_sensitivity ? Utf8ToWide(DecimalSetting(*settings.mouse_sensitivity)) : L"";
  y = SettingRow(ui, cx, y, cw, T(L"Mouse sensitivity"), T(L"Camera sensitivity from 0 to 4."), 145.0f, 32.0f, &control, true);
  if (ui.TextField(L"global.sensitivity", control, &sensitivity, T(L"Not set"), T(L"Mouse sensitivity"), editable)) {
    SaveGlobalValue(GlobalSetting::kMouseSensitivity, WideToUtf8(TrimWide(sensitivity)));
  }
  const float bottom = EndOptions(y);
  if (menu_open) {
    std::vector<std::wstring> sizes;
    for (const wchar_t* size : kTextSizes) sizes.push_back(T(size));
    const int chosen = MenuPanel(ui, kTextSizeControl, size_button, sizes, size_index);
    if (chosen >= 0) {
      Defer([this, chosen, size_index] {
        CloseMenu();
        if (chosen != size_index) SaveGlobalValue(GlobalSetting::kTextSize, std::to_string(chosen + 1));
      });
    }
  }
  return bottom;
}

float App::BuildIntegrations(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, T(L"Integrations"), L"");
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  IntegrationSettings& settings = config_.integrations;
  D2D1_RECT_F control{};

  y = SectionTitle(ui, x, y, width, T(L"Discord Rich Presence"));
  BeginOptions();
  float cy = y;
  cy = SettingRow(ui, cx, cy, cw, T(L"Show Roblox activity in Discord"),
                  T(L"Shows the experience you’re in on your Discord profile while Discord is open."), ui.ToggleWidth(), 32.0f,
                  &control, true);
  if (ui.Toggle(L"integrations.discord", control, &settings.discord_rich_presence, T(L"Discord Rich Presence"))) {
    SaveConfig();
  }
  const bool discord = settings.discord_rich_presence;
  cy = SettingRow(ui, cx, cy, cw, T(L"Show the game you’re playing"), T(L"Its name, icon and creator. Off shows just Roblox."),
                  ui.ToggleWidth(), 32.0f, &control, false);
  if (ui.Toggle(L"integrations.discord.game", control, &settings.discord_show_game, T(L"Show the game you’re playing"),
                discord)) {
    SaveConfig();
  }
  const bool game = discord && settings.discord_show_game;
  cy = SettingRow(ui, cx, cy, cw, T(L"Show a Join button"), T(L"Friends can join your server. Public servers only."), ui.ToggleWidth(),
                  32.0f, &control, false);
  if (ui.Toggle(L"integrations.discord.join", control, &settings.discord_join_button, T(L"Show a Join button"), game)) {
    SaveConfig();
  }
  cy = SettingRow(ui, cx, cy, cw, T(L"Let games set your activity"),
                  T(L"Games can change the text and images with BloxstrapRPC."), ui.ToggleWidth(), 32.0f, &control, false);
  if (ui.Toggle(L"integrations.discord.rpc", control, &settings.discord_allow_game_presence,
                T(L"Let games set your activity"), game)) {
    SaveConfig();
  }
  y = EndOptions(cy);
  return y;
}

namespace {

std::wstring ReleaseDay(std::string_view iso) {
  if (iso.size() < 10 || iso[4] != '-' || iso[7] != '-') return {};
  int year = 0;
  int month = 0;
  int day = 0;
  if (std::from_chars(iso.data(), iso.data() + 4, year).ec != std::errc{} ||
      std::from_chars(iso.data() + 5, iso.data() + 7, month).ec != std::errc{} ||
      std::from_chars(iso.data() + 8, iso.data() + 10, day).ec != std::errc{} || month < 1 || month > 12) {
    return {};
  }
  return FormatDate(year, month, day);
}

}

float App::BuildRobloxVersions(Ui& ui, float x, float y, float width) {
  using Stage = RobloxVersionList::Stage;
  y = SectionTitle(ui, x, y, width, T(L"Roblox versions"));
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  const int card = ui.BeginCard(x, y, width);
  float cy = y + 12.0f;
  const bool possible = RobloxUpdatePossible();
  if (possible && !roblox_list_.asked && roblox_list_.stage == Stage::kIdle) {
    roblox_list_.asked = true;
    Defer([this] { LoadRobloxVersions(); });
  }
  const RobloxReleaseList& list = roblox_list_.list;
  const std::vector<RobloxRelease>& releases = list.releases;
  const bool loading = roblox_list_.stage == Stage::kLoading;
  const bool updating = RobloxUpdateRunning();
  const bool installed = config_.installed.present() && config_.installed.package == kRobloxPackage;

  std::wstring note;
  if (!possible) {
    note = T(L"This copy of Elixir can’t download Roblox. Install it from a file.");
  } else if (loading && releases.empty()) {
    note = T(L"Loading versions…");
  } else if (roblox_list_.stage == Stage::kFailed || list.pinned) {
    note = list.error.empty() ? T(L"Couldn’t get the list of versions.") : list.error;
    const std::int64_t now = UnixNow();
    if (list.retry_at > now) note = TF(L"{0} Try again at {1}.", note, FormatLocalClockTime(list.retry_at, now));
  }
  if (!note.empty()) {
    const bool retry = possible && !loading;
    const float retry_width = retry ? ui.ButtonWidth(T(L"Retry"), glyph::kRefresh) : 0.0f;
    const float note_width = cw - (retry ? retry_width + 16.0f : 0.0f);
    const float note_height = ui.TextHeight(note, TextStyle::kSecondary, note_width);
    const float row_height = std::max(note_height, retry ? theme::kButtonHeight : 0.0f);
    const float top = cy + 6.0f;
    ui.Text(cx, top + (row_height - note_height) / 2.0f, note_width, note, TextStyle::kSecondary, theme::kTextSecondary);
    ui.Label(D2D1::RectF(cx, top, cx + note_width, top + row_height), note);
    if (retry && ui.Button(L"versions.retry", D2D1::RectF(cx + cw - retry_width, top, cx + cw, top + theme::kButtonHeight),
                           T(L"Retry"), ButtonStyle::kStandard, true, glyph::kRefresh)) {
      Defer([this] { LoadRobloxVersions(); });
    }
    cy = top + row_height + 8.0f;
  }

  if (!releases.empty()) {
    constexpr int kVersionGroup = 9201;
    constexpr std::size_t kShownAtFirst = 5;
    const std::size_t shown = roblox_list_.show_all ? releases.size() : std::min(releases.size(), kShownAtFirst);
    std::int64_t newest = 0;
    for (const RobloxRelease& release : releases) newest = std::max(newest, release.version_code);
    for (std::size_t i = 0; i < shown; ++i) {
      const RobloxRelease& release = releases[i];
      std::wstring subtitle = ReleaseDay(release.published_at);
      if (release.TotalSize() > 0) {
        if (!subtitle.empty()) subtitle += L"  ·  ";
        subtitle += FormatByteSize(release.TotalSize());
      }
      const bool is_installed = installed && config_.installed.version_code == release.version_code;
      const std::wstring& trailing =
          is_installed ? T(L"Installed") : release.version_code == newest && !list.pinned ? T(L"Latest") : T(L"");
      if (ui.ListItem(L"versions.item." + std::to_wstring(i), D2D1::RectF(cx - 8.0f, cy, cx + cw + 8.0f, cy + 54.0f),
                      static_cast<int>(i) == roblox_list_.selected, L"Roblox " + Utf8ToWide(release.version_name),
                      subtitle, trailing, glyph::kPackage, kVersionGroup, IDR_ROBLOX_DOWNLOAD_PACKAGE_PNG)) {
        roblox_list_.selected = static_cast<int>(i);
      }
      cy += 56.0f;
    }
    if (releases.size() > kShownAtFirst) {
      if (ui.Link(L"versions.more", cx, cy + 4.0f,
                  roblox_list_.show_all ? T(L"Show fewer") : TF(L"Show all {0} versions", releases.size()))) {
        roblox_list_.show_all = !roblox_list_.show_all;
        if (!roblox_list_.show_all && roblox_list_.selected >= static_cast<int>(kShownAtFirst)) roblox_list_.selected = -1;
      }
      cy += 30.0f;
    }
    cy += 8.0f;
    if (updating) {
      const std::wstring status = RobloxUpdateStatus();
      cy += ui.Text(cx, cy, cw - 110.0f, status, TextStyle::kBody, theme::kText) + 12.0f;
      ui.Label(D2D1::RectF(cx, cy - 30.0f, cx + cw - 110.0f, cy), status);
      ui.Progress(D2D1::RectF(cx, cy, cx + cw - 110.0f, cy + 4.0f),
                  roblox_update_.percent >= 0 ? static_cast<float>(std::min(roblox_update_.percent, 100)) / 100.0f : -1.0f);
      if (ui.Button(L"versions.cancel", D2D1::RectF(cx + cw - 92.0f, cy - 14.0f, cx + cw, cy + 18.0f), T(L"Cancel"),
                    ButtonStyle::kStandard)) {
        if (roblox_update_.cancel) *roblox_update_.cancel = true;
      }
      cy += 18.0f;
    } else {
      const bool chosen = roblox_list_.selected >= 0 && roblox_list_.selected < static_cast<int>(releases.size());
      const bool chosen_installed =
          chosen && installed &&
          releases[static_cast<std::size_t>(roblox_list_.selected)].version_code == config_.installed.version_code;
      const float install_width = ui.ButtonWidth(T(L"Download and install"), glyph::kDownload);
      if (ui.Button(L"versions.install", D2D1::RectF(cx, cy, cx + install_width, cy + theme::kButtonHeight),
                    T(L"Download and install"), ButtonStyle::kStandard,
                    chosen && !chosen_installed && possible && !PackageWorkRunning() && !cache_.clearing, glyph::kDownload,
                    T(L"Downloads the selected version, checks that it is Roblox’s own file and installs it"))) {
        Defer([this] { InstallSelectedRobloxVersion(); });
      }
      if (roblox_update_.stage == RobloxUpdateFlow::Stage::kFailed && roblox_update_.chosen) {
        const D2D1_RECT_F failed =
            D2D1::RectF(cx + install_width + 14.0f, cy, cx + cw, cy + theme::kButtonHeight);
        ui.TextLine(failed, roblox_update_.error, TextStyle::kSecondary, theme::kFail);
        ui.Label(failed, roblox_update_.error);
      }
      cy += theme::kButtonHeight;
    }
  }
  return ui.EndCard(card, cy);
}

float App::BuildRobloxUpdates(Ui& ui, float x, float y, float width) {
  y = SectionTitle(ui, x, y, width, T(L"Roblox updates"));
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  D2D1_RECT_F control{};
  const bool installed = config_.installed.present() && config_.installed.package == kRobloxPackage;
  const bool checking = roblox_version_.pending != nullptr;
  const bool updating = RobloxUpdateRunning();
  const bool can_update = RobloxUpdatePossible();
  bool newer = false;
  BeginOptions();
  float cy = y;
  std::wstring status;
  if (updating) {
    status = RobloxUpdateStatus();
  } else if (checking) {
    status = T(L"Checking for Roblox updates…");
  } else if (roblox_update_.stage == RobloxUpdateFlow::Stage::kFailed) {
    status = TF(L"Roblox wasn’t updated. {0}", roblox_update_.error);
    newer = installed && roblox_version_.latest && roblox_version_.latest->version_code > config_.installed.version_code;
  } else if (!roblox_version_.error.empty()) {
    status = roblox_version_.error;
  } else if (roblox_version_.latest) {
    const auto& latest = *roblox_version_.latest;
    const std::wstring version = Utf8ToWide(latest.version_name);
    if (!installed) {
      status = TF(L"Latest known version is {0}.", version);
    } else if (latest.version_code == config_.installed.version_code) {
      status = T(L"Roblox is up to date.");
    } else if (latest.version_code < config_.installed.version_code) {
      status = TF(L"Your installed version is newer than {0}.", version);
    } else {
      newer = true;
      status = can_update ? TF(L"Roblox {0} is available.", version)
                          : TF(L"Roblox {0} is available. Install it from a file to update.", version);
    }
  } else {
    status = T(L"Check for a newer version of Roblox.");
  }
  if (!checking && !updating && roblox_update_.stage != RobloxUpdateFlow::Stage::kFailed &&
      config_.roblox.last_check > 0 && roblox_version_.error.empty()) {
    status = TF(L"{0} Last checked at {1}.", status, FormatLocalClockTime(config_.roblox.last_check, UnixNow()));
  }
  const std::wstring title = installed ? TF(L"Installed Roblox {0}", Utf8ToWide(config_.installed.version_name))
                                       : T(L"Roblox isn’t installed");
  const bool offer_update = newer && can_update && !updating;
  const std::wstring& button_label = offer_update ? T(L"Update now") : T(L"Check now");
  const float check_width =
      std::max(ui.ButtonWidth(T(L"Check now"), glyph::kRefresh), ui.ButtonWidth(T(L"Update now"), glyph::kRefresh));
  cy = SettingRow(ui, cx, cy, cw, title, status, check_width, 32.0f, &control, true);
  if (offer_update) {
    if (ui.Button(L"roblox.update.install", control, button_label, ButtonStyle::kStandard,
                  !PackageWorkRunning() && !cache_.clearing, glyph::kRefresh, title + L". " + status)) {
      Defer([this] { StartRobloxUpdate( false); });
    }
  } else if (ui.Button(L"roblox.update.check", control, button_label, ButtonStyle::kStandard, !checking && !updating,
                       glyph::kRefresh, title + L". " + status)) {
    Defer([this] { CheckRobloxVersion( false); });
  }

  if (can_update) {
    cy = SettingRow(ui, cx, cy, cw, T(L"Update Roblox automatically"),
                    T(L"Installs a newer version when Elixir starts. Your data is kept."),
                    ui.ToggleWidth(), 32.0f, &control, false);
    if (ui.Toggle(L"roblox.update.auto", control, &config_.roblox.auto_update, T(L"Update Roblox automatically"))) {
      SaveConfig();
    }
  }

  cy = SettingRow(ui, cx, cy, cw, T(L"Check when Elixir starts"),
                  T(L"Notify me when a newer version of Roblox is available."),
                  ui.ToggleWidth(), 32.0f, &control, false);
  if (ui.Toggle(L"roblox.update.startup", control, &config_.roblox.check_for_new_version,
                T(L"Check for new Roblox versions when Elixir starts"))) {
    SaveConfig();
  }

  const InstalledApp& previous = config_.roblox.previous;
  if (previous.present() && previous.package == config_.installed.package) {
    const bool on_disk = PathIsDirectory(Utf8ToWide(previous.install_dir));
    const float back_width = ui.ButtonWidth(T(L"Roll back"));
    cy = SettingRow(ui, cx, cy, cw, TF(L"Roll back to Roblox {0}", Utf8ToWide(previous.version_name)),
                    on_disk ? T(L"Goes back to the version you had before the last update.")
                            : T(L"That version is no longer available."),
                    back_width, 32.0f, &control, false);
    if (ui.Button(L"roblox.rollback", control, T(L"Roll back"), ButtonStyle::kStandard,
                  on_disk && !PackageWorkRunning() && !runtime_.running)) {
      Defer([this] { AskRollBackRoblox(); });
    }
  }
  return EndOptions(cy) + 16.0f;
}

float App::BuildCacheCard(Ui& ui, float x, float y, float width) {
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  const int card = ui.BeginCard(x, y, width);
  float cy = y + 18.0f;
  cy += ui.Text(cx, cy, cw, T(L"Free up space"), TextStyle::kCardTitle, theme::kText) + 6.0f;
  cy += ui.Text(cx, cy, cw,
                T(L"Roblox downloads what it needs again. Your sign-in, settings and Fast Flags are kept."),
                TextStyle::kSecondary, theme::kTextSecondary) + 12.0f;
  std::uint64_t total = 0;
  const float size_width = 110.0f;
  for (const CacheLocation& location : cache_.locations) {
    total += location.bytes;
    const float title_height = ui.Text(cx, cy, cw - size_width - 16.0f, location.title, TextStyle::kBody, theme::kText);
    const std::wstring description =
        location.redirected ? std::wstring(T(L"Skipped: this folder points somewhere else."))
                            : location.description;
    const float description_height = ui.Text(cx, cy + title_height + 1.0f, cw - size_width - 16.0f, description,
                                             TextStyle::kCaption, location.redirected ? theme::kWarn : theme::kTextTertiary);
    const std::wstring size = location.redirected ? std::wstring(L"skipped")
                              : !location.exists  ? std::wstring(L"empty")
                                                  : FormatByteSize(location.bytes);
    ui.TextLine(D2D1::RectF(cx + cw - size_width, cy, cx + cw, cy + title_height), size, TextStyle::kBody,
                location.exists && location.bytes > 0 ? theme::kText : theme::kTextTertiary, Align::kTrailing);
    ui.Label(D2D1::RectF(cx, cy, cx + cw, cy + title_height + description_height),
             location.title + L": " + size + L". " + description);
    cy += title_height + description_height + 10.0f;
  }
  if (cache_.locations.empty()) {
    cy += ui.Text(cx, cy, cw, cache_.measured ? T(L"No cache folders.") : T(L"Measuring…"), TextStyle::kSecondary,
                  theme::kTextTertiary) + 10.0f;
  }
  const bool running = RuntimeRunning();
  const bool working = PackageWorkRunning();
  ui.Fill(D2D1::RectF(cx, cy, cx + cw, cy + 1.0f), theme::kDivider);
  cy += 12.0f;
  ButtonFlow flow(ui, cx, cx + cw, cy);
  if (flow.Add(L"cache.clear", T(L"Clear cache…"), ButtonStyle::kDanger,
               !running && !working && !cache_.clearing && cache_.measured && total > 0, glyph::kBroom,
               T(L"Asks before deleting anything"))) {
    Defer([this] { AskClearCaches(); });
  }
  cy = flow.bottom() + 10.0f;
  std::wstring status = cache_.measured ? TF(L"Total {0}.", FormatByteSize(total)) : T(L"Measuring…");
  if (running) status += L" " + T(L"Close Roblox to clear the cache.");
  if (working) status += L" " + T(L"Wait for Roblox to finish installing.");
  cy += ui.Text(cx, cy, cw, status, TextStyle::kSecondary, running ? theme::kWarn : theme::kTextSecondary);
  if (!cache_.message.empty()) {
    const float height = ui.Text(cx, cy + 6.0f, cw, cache_.message, TextStyle::kSecondary,
                                 cache_.message_status == 2 ? theme::kFail : theme::kTextSecondary);
    ui.Label(D2D1::RectF(cx, cy + 6.0f, cx + cw, cy + 6.0f + height), cache_.message);
    cy += 6.0f + height;
  }
  return ui.EndCard(card, cy);
}

}
