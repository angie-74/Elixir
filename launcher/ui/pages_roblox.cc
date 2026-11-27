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
#include "core/util.h"
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
  const std::size_t start = error.rfind(L"(line ");
  if (start == std::wstring::npos) return {};
  const std::size_t end = error.find(L',', start);
  if (end == std::wstring::npos) return {};
  return error.substr(start + 1, end - start - 1);
}

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
  const float link_width = ui.TextWidth(L"Learn more", TextStyle::kBody) + 2.0f;
  const float top = y;
  y = Banner(ui, x, y, width, 1, L"Warning", L"Roblox only applies whitelisted FFlags.", link_width + 28.0f);
  if (ui.Link(L"fflags.learnmore", x + width - link_width - 18.0f, top + (y - top) / 2.0f - 10.0f, L"Learn more")) {
    Defer([this] { OpenPath(kFlagRulesAnnouncement); });
  }
  return y;
}

float App::BuildFastFlags(Ui& ui, float x, float y, float width) {
  pending_menu_.reset();
  if (!fast_flags_.editor) LoadFastFlags(true);
  if (fast_flags_.editor_open) return BuildFastFlagEditor(ui, x, y, width);
  y = PageHeader(ui, x, y, width, L"Fast Flags",
                 L"Control how specific Roblox engine parameters and features are configured.");

  if (!fast_flags_.load_error.empty()) {
    const float top = y;
    const std::wstring line = ErrorLine(fast_flags_.load_error);
    y = Banner(ui, x, y, width, 2, L"Your Fast Flags couldn’t be read",
               line.empty() ? std::wstring(L"Start over to make a new list.")
                            : L"There’s a mistake on " + line + L". Start over to make a new list.",
               140.0f);
    if (ui.Button(L"fflags.startover", D2D1::RectF(x + width - 136.0f, top + 12.0f, x + width - 16.0f, top + 44.0f),
                  L"Start over", ButtonStyle::kStandard)) {
      Overlay overlay;
      overlay.id = L"fflags.startover";
      overlay.status = 1;
      overlay.title = L"Start an empty list?";
      overlay.body = L"Your old Fast Flags are kept as a backup.";
      overlay.actions = {{L"fflags.startover.confirm", L"Start over", ButtonStyle::kDanger},
                         {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
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
    const float button = ui.ButtonWidth(L"Open", glyph::kEdit);
    cy = SettingRow(ui, cx, cy, cw, L"Fast Flag Editor", L"Manage your own Fast Flags. Use with caution.", button,
                    32.0f, &control, true);
    if (ui.Button(L"fflags.open", control, L"Open", ButtonStyle::kStandard, true, glyph::kEdit)) {
      OpenFastFlagEditor(true);
    }
  }
  cy = SettingRow(ui, cx, cy, cw, L"Allow Elixir to manage Fast Flags",
                  L"Turning this off stops anything set here from reaching Roblox.", 96.0f, 32.0f, &control, false);
  if (ui.Toggle(L"fflags.manage", control, &config_.advanced.manage_fast_flags, L"Allow Elixir to manage Fast Flags")) {
    SaveConfig();
  }
  {
    const float button = ui.ButtonWidth(L"Reset", glyph::kReset);
    cy = SettingRow(ui, cx, cy, cw, L"Reset everything to defaults", L"Removes every Fast Flag set here and in the editor.",
                    button, 32.0f, &control, false);
    if (ui.Button(L"fflags.reset", control, L"Reset", ButtonStyle::kDanger, !editor.flags().empty(), glyph::kReset)) {
      Overlay overlay;
      overlay.id = L"fflags.reset";
      overlay.status = 1;
      overlay.title = L"Reset every Fast Flag?";
      overlay.body = L"This removes all of your Fast Flags. Roblox goes back to its own settings.";
      overlay.actions = {{L"fflags.reset.confirm", L"Reset", ButtonStyle::kDanger},
                         {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
      Defer([this, overlay] { ShowOverlay(overlay); });
    }
  }
  y = EndOptions(cy) + 24.0f;

  std::vector<std::string> groups;
  for (const fflags::Preset& preset : fflags::BuiltinPresets()) {
    if (std::find(groups.begin(), groups.end(), preset.group) == groups.end()) groups.push_back(preset.group);
  }
  for (const std::string& group : groups) {
    y = SectionTitle(ui, x, y, width, Utf8ToWide(group));
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
  const std::wstring title = Utf8ToWide(preset.title);
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
        SettingRow(ui, x, y, width, title, Utf8ToWide(preset.description), 96.0f, 32.0f, &control, true);
    if (ui.Toggle(id, control, &on, title)) apply(on ? last : 0);
    return bottom;
  }

  if (preset.control == fflags::PresetControl::kChoice) {
    const float bottom =
        SettingRow(ui, x, y, width, title, Utf8ToWide(preset.description), std::min(200.0f, width), 32.0f, &control, true);
    const std::wstring label = active < 0 ? std::wstring(L"Custom") : Utf8ToWide(preset.options[active].label);
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
      for (const fflags::PresetOption& option : preset.options) menu.options.push_back(Utf8ToWide(option.label));
      pending_menu_ = std::move(menu);
    }
    return bottom;
  }

  const bool standalone = preset.control == fflags::PresetControl::kSlider;
  const int first = standalone ? 0 : 1;
  const auto slider_row = [&](D2D1_RECT_F row) {
    const std::wstring low = Utf8ToWide(preset.low_label);
    const std::wstring high = Utf8ToWide(preset.high_label);
    const float low_width = low.empty() ? 0.0f : ui.TextWidth(low, TextStyle::kBody) + 8.0f;
    const float high_width = high.empty() ? 0.0f : ui.TextWidth(high, TextStyle::kBody) + 8.0f;
    float box_width = 0.0f;
    if (preset.show_value) {
      box_width = 30.0f;
      for (int option = first; option <= last; ++option) {
        box_width = std::max(box_width, ui.TextWidth(Utf8ToWide(preset.options[option].label), TextStyle::kBody) + 12.0f);
      }
      if (active < 0) box_width = std::max(box_width, ui.TextWidth(L"Custom", TextStyle::kBody) + 12.0f);
    }
    if (!low.empty()) ui.TextLine(D2D1::RectF(row.left, row.top, row.left + low_width, row.bottom), low, TextStyle::kBody,
                                  theme::kText);
    const float slider_right = row.right - box_width - (box_width > 0.0f ? 8.0f : 0.0f) - high_width;
    const int position = active >= first ? active : last;
    double value = position;
    const std::wstring label = active < 0 ? std::wstring(L"Custom") : Utf8ToWide(preset.options[position].label);
    if (ui.Slider(id + L".slider", D2D1::RectF(row.left + low_width, row.top, slider_right, row.bottom), &value,
                  first, last, 1.0, standalone ? title : title + L" level", label, true, 0.0f)) {
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
    const float bottom = SettingRow(ui, x, y, width, title, Utf8ToWide(preset.description),
                                    std::min(360.0f, width), 32.0f, &control, true);
    slider_row(control);
    return bottom;
  }

  bool on = active != 0;
  const bool cards = option_cards_;
  option_cards_ = false;
  const int card = ui.BeginCard(x - theme::kCardPadding, y, width + 2.0f * theme::kCardPadding);
  float bottom = SettingRow(ui, x, y, width, title, Utf8ToWide(preset.description), 96.0f, 32.0f, &control, true);
  if (ui.Toggle(id, control, &on, title)) {
    apply(on ? last : 0);
    active = editor.ActiveOption(preset.id);
  }
  if (on) {
    ui.Fill(D2D1::RectF(x - theme::kCardPadding, bottom, x + width + theme::kCardPadding, bottom + 1.0f),
            theme::kDivider);
    bottom += 12.0f;
    if (!preset.note.empty()) {
      bottom += ui.Text(x, bottom, width, Utf8ToWide(preset.note), TextStyle::kCaption, theme::kTextTertiary,
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
  y = PageHeader(ui, x, y, width, L"Fast Flag Editor", L"Manage your own Fast Flags. Click a name or value to edit it.");
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
    if (flow.Add(L"fflags.back", L"Back", ButtonStyle::kStandard, true, glyph::kBack)) OpenFastFlagEditor(false);
    if (flow.Add(L"fflags.add", L"Add new", ButtonStyle::kStandard, true, glyph::kAdd)) {
      fast_flags_.adding = true;
      Defer([this] { SetFocusTo(L"fflags.new.name", true); });
    }
    const std::size_t selected = fast_flags_.selected.size();
    if (flow.Add(L"fflags.delete", L"Delete selected", ButtonStyle::kDanger, selected > 0, glyph::kDelete)) {
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
    if (flow.Add(L"fflags.presetflags", fast_flags_.show_preset_flags ? L"Hide preset flags" : L"Show preset flags",
                 ButtonStyle::kStandard, true, fast_flags_.show_preset_flags ? glyph::kEyeOff : glyph::kEye)) {
      fast_flags_.show_preset_flags = !fast_flags_.show_preset_flags;
    }
    if (flow.Add(L"fflags.import", L"Import JSON", ButtonStyle::kStandard, true, glyph::kImport)) {
      Overlay overlay;
      overlay.id = L"fflags.import";
      overlay.status = 4;
      overlay.title = L"Import JSON";
      overlay.body = L"Add Fast Flags from JSON you copied, or from a file. Flags you already have get the new values.";
      overlay.actions = {{L"fflags.import.paste", L"Paste", ButtonStyle::kStandard},
                         {L"fflags.import.file", L"Open file…", ButtonStyle::kStandard},
                         {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
      Defer([this, overlay] { ShowOverlay(overlay); });
    }
    if (flow.Add(L"fflags.export", L"Export JSON", ButtonStyle::kStandard, !editor.flags().empty(), glyph::kExport)) {
      Overlay overlay;
      overlay.id = L"fflags.export";
      overlay.status = 4;
      overlay.title = L"Export JSON";
      overlay.body = L"Copy your Fast Flags as JSON, or save them to a file.";
      overlay.actions = {{L"fflags.export.copy", L"Copy", ButtonStyle::kStandard},
                         {L"fflags.export.file", L"Save file…", ButtonStyle::kStandard},
                         {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
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
  ui.TextField(L"fflags.search", D2D1::RectF(x, y, x + width, y + 34.0f), &fast_flags_.search, L"Search",
               L"Search Fast Flags");
  y += 34.0f + 12.0f;

  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  if (fast_flags_.adding) {
    const int card = ui.BeginCard(x, y, width);
    float cy = y + 16.0f;
    const float add_width = ui.ButtonWidth(L"Add", glyph::kAdd);
    const float cancel_width = ui.ButtonWidth(L"Cancel");
    const float value_width = std::max(140.0f, std::floor((cw - add_width - cancel_width - 24.0f) * 0.34f));
    const float name_width = cw - add_width - cancel_width - value_width - 24.0f;
    ui.TextField(L"fflags.new.name", D2D1::RectF(cx, cy, cx + name_width, cy + 32.0f), &fast_flags_.new_name, L"Name",
                 L"New flag name");
    ui.TextField(L"fflags.new.value",
                 D2D1::RectF(cx + name_width + 8.0f, cy, cx + name_width + 8.0f + value_width, cy + 32.0f),
                 &fast_flags_.new_value, L"Value", L"New flag value");
    const float buttons = cx + name_width + value_width + 16.0f;
    if (ui.Button(L"fflags.new.add", D2D1::RectF(buttons, cy, buttons + add_width, cy + 32.0f), L"Add",
                  ButtonStyle::kStandard, !TrimWide(fast_flags_.new_name).empty(), glyph::kAdd)) {
      AddFastFlag();
    }
    if (ui.Button(L"fflags.new.cancel",
                  D2D1::RectF(buttons + add_width + 8.0f, cy, buttons + add_width + 8.0f + cancel_width, cy + 32.0f),
                  L"Cancel", ButtonStyle::kStandard)) {
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
  ui.TextLine(D2D1::RectF(cx + check_width, cy, cx + check_width + name_width, cy + 24.0f), L"Name",
              TextStyle::kCaptionStrong, theme::kTextTertiary);
  ui.TextLine(D2D1::RectF(cx + check_width + name_width + 8.0f, cy, cx + cw, cy + 24.0f), L"Value",
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
    const std::wstring empty = editor.flags().empty() ? L"No Fast Flags yet." : L"No Fast Flags match.";
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
                    L"Select " + name)) {
      if (selected) {
        fast_flags_.selected.insert(row.name);
      } else {
        fast_flags_.selected.erase(row.name);
      }
    }
    std::wstring name_text = name;
    if (ui.TextField(L"fflags.row." + index + L".name",
                     D2D1::RectF(cx + check_width, cy, cx + check_width + name_width, cy + 32.0f), &name_text,
                     L"Name", L"Name of " + name)) {
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
                     L"Value", L"Value of " + name)) {
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
  y = PageHeader(ui, x, y, width, L"Global Settings", L"In-game preferences for this Roblox profile.");
  if (!global_settings_) {
    const float top = y;
    const float button_width = ui.ButtonWidth(L"Refresh", glyph::kRefresh);
    y = Banner(ui, x, y, width, 1, L"Game settings unavailable", global_settings_error_, button_width + 12.0f);
    const float right = x + width - theme::kCardPadding;
    if (ui.Button(L"global.refresh", D2D1::RectF(right - button_width, top + 12.0f, right, top + 44.0f),
                  L"Refresh", ButtonStyle::kStandard, true, glyph::kRefresh)) RefreshGlobalSettings();
    return y;
  }

  const GlobalSettings settings = *global_settings_;
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  const bool in_use = global_data_in_use_ || runtime_.running;
  const bool editable = !settings.read_only && !in_use;
  D2D1_RECT_F control{};
  if (in_use) {
    y = Banner(ui, x, y, width, 4, L"Roblox is using this profile", L"Close its game window to edit these settings.") + 16.0f;
  }

  BeginOptions();
  y = SettingRow(ui, cx, y, cw, L"Read-only settings file",
                 settings.read_only ? L"Turn this off to edit the settings." : L"Prevent Roblox from overwriting these preferences.",
                 96.0f, 32.0f, &control, true);
  bool locked = settings.read_only;
  if (ui.Toggle(L"global.readonly", control, &locked, L"Read-only settings file", !in_use)) {
    std::wstring error;
    const bool saved = SetGlobalSettingsReadOnly(paths_, locked, &error);
    RefreshGlobalSettings();
    if (!saved) ShowToast(error);
  }
  y = EndOptions(y) + 24.0f;

  y = SectionTitle(ui, x, y, width, L"Graphics");
  BeginOptions();
  double quality = settings.quality_level.value_or(0);
  const std::wstring quality_text = settings.quality_level
      ? (quality == 0 ? L"Auto" : std::to_wstring(*settings.quality_level)) : L"Not set";
  const std::wstring quality_hint = global_quality_override_.empty()
      ? L"Choose Auto or a quality level from 1 to 10."
      : L"Fast Flags overrides this preference with level " + global_quality_override_ + L".";
  y = SettingRow(ui, cx, y, cw, L"Graphics quality", quality_hint, std::min(300.0f, cw), 32.0f, &control, true);
  if (ui.Slider(L"global.quality", control, &quality, 0, 10, 1, L"Graphics quality", quality_text, editable)) {
    SaveGlobalValue(GlobalSetting::kQualityLevel, std::to_string(static_cast<int>(quality)));
  }

  std::wstring fps = settings.framerate_cap && *settings.framerate_cap > 0
      ? std::to_wstring(*settings.framerate_cap) : L"";
  y = SettingRow(ui, cx, y, cw, L"Framerate cap", L"Frames per second. Clear the field to use the default.",
                 145.0f, 32.0f, &control, false);
  if (ui.TextField(L"global.fps", control, &fps, settings.framerate_cap ? L"Default" : L"Not set", L"Framerate cap", editable)) {
    fps = TrimWide(fps);
    SaveGlobalValue(GlobalSetting::kFramerateCap, fps.empty() ? "-1" : WideToUtf8(fps));
  }
  y = EndOptions(y) + 24.0f;

  y = SectionTitle(ui, x, y, width, L"Interface");
  BeginOptions();
  double transparency = std::round(settings.transparency.value_or(0.0f) * 1000000.0) / 1000000.0;
  const std::wstring transparency_text = settings.transparency
      ? std::to_wstring(std::lround(transparency * 100)) + L"%" : L"Not set";
  y = SettingRow(ui, cx, y, cw, L"Background transparency", L"Transparency of supported Roblox interface backgrounds.",
                 std::min(300.0f, cw), 32.0f, &control, true);
  if (ui.Slider(L"global.transparency", control, &transparency, 0.0, 1.0, 0.1, L"Background transparency",
                transparency_text, editable)) {
    SaveGlobalValue(GlobalSetting::kTransparency, DecimalSetting(static_cast<float>(transparency)));
  }

  bool motion = settings.reduced_motion.value_or(false);
  y = SettingRow(ui, cx, y, cw, L"Reduced motion",
                 settings.reduced_motion ? L"Reduce interface animations in Roblox." : L"No preference saved.",
                 96.0f, 32.0f, &control, false);
  if (ui.Toggle(L"global.motion", control, &motion, L"Reduced motion", editable)) {
    SaveGlobalValue(GlobalSetting::kReducedMotion, motion ? "true" : "false");
  }

  D2D1_RECT_F size_button{};
  y = SettingRow(ui, cx, y, cw, L"Text size", L"Medium is the default text size.", 145.0f, 32.0f, &size_button, false);
  const int size_index = settings.text_size.value_or(0) - 1;
  const bool menu_open = MenuOpen(kTextSizeControl) && editable;
  if (ui.ComboBox(kTextSizeControl, size_button, L"Text size", size_index >= 0 ? kTextSizes[size_index] : L"Not set",
                  menu_open, editable)) {
    Defer([this, menu_open, size_index] {
      SetMenu(kTextSizeControl, menu_open ? -1 : std::max(0, size_index), static_cast<int>(kTextSizes.size()));
    });
  }
  y = EndOptions(y) + 24.0f;

  y = SectionTitle(ui, x, y, width, L"Input");
  BeginOptions();
  std::wstring sensitivity = settings.mouse_sensitivity ? Utf8ToWide(DecimalSetting(*settings.mouse_sensitivity)) : L"";
  y = SettingRow(ui, cx, y, cw, L"Mouse sensitivity", L"Camera sensitivity from 0 to 4.", 145.0f, 32.0f, &control, true);
  if (ui.TextField(L"global.sensitivity", control, &sensitivity, L"Not set", L"Mouse sensitivity", editable)) {
    SaveGlobalValue(GlobalSetting::kMouseSensitivity, WideToUtf8(TrimWide(sensitivity)));
  }
  const float bottom = EndOptions(y);
  if (menu_open) {
    const std::vector<std::wstring> sizes(kTextSizes.begin(), kTextSizes.end());
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
  y = PageHeader(ui, x, y, width, L"Integrations", L"");
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  IntegrationSettings& settings = config_.integrations;
  D2D1_RECT_F control{};

  y = SectionTitle(ui, x, y, width, L"Discord Rich Presence");
  BeginOptions();
  float cy = y;
  cy = SettingRow(ui, cx, cy, cw, L"Show Roblox activity in Discord",
                  L"Shows the experience you’re in on your Discord profile while Discord is open.", 96.0f, 32.0f,
                  &control, true);
  if (ui.Toggle(L"integrations.discord", control, &settings.discord_rich_presence, L"Discord Rich Presence")) {
    SaveConfig();
  }
  const bool discord = settings.discord_rich_presence;
  cy = SettingRow(ui, cx, cy, cw, L"Show the game you’re playing", L"Its name, icon and creator. Off shows just Roblox.",
                  96.0f, 32.0f, &control, false);
  if (ui.Toggle(L"integrations.discord.game", control, &settings.discord_show_game, L"Show the game you’re playing",
                discord)) {
    SaveConfig();
  }
  const bool game = discord && settings.discord_show_game;
  cy = SettingRow(ui, cx, cy, cw, L"Show a Join button", L"Friends can join your server. Public servers only.", 96.0f,
                  32.0f, &control, false);
  if (ui.Toggle(L"integrations.discord.join", control, &settings.discord_join_button, L"Show a Join button", game)) {
    SaveConfig();
  }
  cy = SettingRow(ui, cx, cy, cw, L"Let games set your activity",
                  L"Games can change the text and images with BloxstrapRPC.", 96.0f, 32.0f, &control, false);
  if (ui.Toggle(L"integrations.discord.rpc", control, &settings.discord_allow_game_presence,
                L"Let games set your activity", game)) {
    SaveConfig();
  }
  y = EndOptions(cy);
  return y;
}

namespace {

std::wstring ReleaseDay(std::string_view iso) {
  static constexpr const wchar_t* kMonths[] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
                                               L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
  if (iso.size() < 10 || iso[4] != '-' || iso[7] != '-') return {};
  int year = 0;
  int month = 0;
  int day = 0;
  if (std::from_chars(iso.data(), iso.data() + 4, year).ec != std::errc{} ||
      std::from_chars(iso.data() + 5, iso.data() + 7, month).ec != std::errc{} ||
      std::from_chars(iso.data() + 8, iso.data() + 10, day).ec != std::errc{} || month < 1 || month > 12) {
    return {};
  }
  return std::wstring(kMonths[month - 1]) + L" " + std::to_wstring(day) + L", " + std::to_wstring(year);
}

}

float App::BuildRobloxVersions(Ui& ui, float x, float y, float width) {
  using Stage = RobloxVersionList::Stage;
  y = SectionTitle(ui, x, y, width, L"Roblox versions");
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
    note = L"This copy of Elixir can’t download Roblox. Install it from a file.";
  } else if (loading && releases.empty()) {
    note = L"Loading versions…";
  } else if (roblox_list_.stage == Stage::kFailed || list.pinned) {
    note = list.error.empty() ? std::wstring(L"Couldn’t get the list of versions.") : list.error;
    const std::int64_t now = UnixNow();
    if (list.retry_at > now) note += L" Try again at " + FormatLocalClockTime(list.retry_at, now) + L".";
  }
  if (!note.empty()) {
    const bool retry = possible && !loading;
    const float retry_width = retry ? ui.ButtonWidth(L"Retry", glyph::kRefresh) : 0.0f;
    const float note_width = cw - (retry ? retry_width + 16.0f : 0.0f);
    const float note_height = ui.TextHeight(note, TextStyle::kSecondary, note_width);
    const float row_height = std::max(note_height, retry ? theme::kButtonHeight : 0.0f);
    const float top = cy + 6.0f;
    ui.Text(cx, top + (row_height - note_height) / 2.0f, note_width, note, TextStyle::kSecondary, theme::kTextSecondary);
    ui.Label(D2D1::RectF(cx, top, cx + note_width, top + row_height), note);
    if (retry && ui.Button(L"versions.retry", D2D1::RectF(cx + cw - retry_width, top, cx + cw, top + theme::kButtonHeight),
                           L"Retry", ButtonStyle::kStandard, true, glyph::kRefresh)) {
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
      const wchar_t* trailing = is_installed ? L"Installed" : release.version_code == newest && !list.pinned ? L"Latest" : L"";
      if (ui.ListItem(L"versions.item." + std::to_wstring(i), D2D1::RectF(cx - 8.0f, cy, cx + cw + 8.0f, cy + 54.0f),
                      static_cast<int>(i) == roblox_list_.selected, L"Roblox " + Utf8ToWide(release.version_name),
                      subtitle, trailing, glyph::kPackage, kVersionGroup, IDR_ROBLOX_DOWNLOAD_PACKAGE_PNG)) {
        roblox_list_.selected = static_cast<int>(i);
      }
      cy += 56.0f;
    }
    if (releases.size() > kShownAtFirst) {
      if (ui.Link(L"versions.more", cx, cy + 4.0f,
                  roblox_list_.show_all ? std::wstring(L"Show fewer")
                                        : L"Show all " + std::to_wstring(releases.size()) + L" versions")) {
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
      if (ui.Button(L"versions.cancel", D2D1::RectF(cx + cw - 92.0f, cy - 14.0f, cx + cw, cy + 18.0f), L"Cancel",
                    ButtonStyle::kStandard)) {
        if (roblox_update_.cancel) *roblox_update_.cancel = true;
      }
      cy += 18.0f;
    } else {
      const bool chosen = roblox_list_.selected >= 0 && roblox_list_.selected < static_cast<int>(releases.size());
      const bool chosen_installed =
          chosen && installed &&
          releases[static_cast<std::size_t>(roblox_list_.selected)].version_code == config_.installed.version_code;
      const float install_width = ui.ButtonWidth(L"Download and install", glyph::kDownload);
      if (ui.Button(L"versions.install", D2D1::RectF(cx, cy, cx + install_width, cy + theme::kButtonHeight),
                    L"Download and install", ButtonStyle::kStandard,
                    chosen && !chosen_installed && possible && !PackageWorkRunning() && !cache_.clearing, glyph::kDownload,
                    L"Downloads the selected version, checks that it is Roblox’s own file and installs it")) {
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
  y = SectionTitle(ui, x, y, width, L"Roblox updates");
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
    status = L"Checking for Roblox updates…";
  } else if (roblox_update_.stage == RobloxUpdateFlow::Stage::kFailed) {
    status = L"Roblox wasn’t updated. " + roblox_update_.error;
    newer = installed && roblox_version_.latest && roblox_version_.latest->version_code > config_.installed.version_code;
  } else if (!roblox_version_.error.empty()) {
    status = roblox_version_.error;
  } else if (roblox_version_.latest) {
    const auto& latest = *roblox_version_.latest;
    const std::wstring version = Utf8ToWide(latest.version_name);
    if (!installed) {
      status = L"Latest known version is " + version + L".";
    } else if (latest.version_code == config_.installed.version_code) {
      status = L"Roblox is up to date.";
    } else if (latest.version_code < config_.installed.version_code) {
      status = L"Your installed version is newer than " + version + L".";
    } else {
      newer = true;
      status = L"Roblox " + version + L" is available." +
               (can_update ? std::wstring() : std::wstring(L" Install it from a file to update."));
    }
  } else {
    status = L"Check for a newer version of Roblox.";
  }
  if (!checking && !updating && roblox_update_.stage != RobloxUpdateFlow::Stage::kFailed &&
      config_.roblox.last_check > 0 && roblox_version_.error.empty()) {
    status += L" Last checked at " + FormatLocalClockTime(config_.roblox.last_check, UnixNow()) + L".";
  }
  const std::wstring title = installed ? L"Installed Roblox " + Utf8ToWide(config_.installed.version_name)
                                       : L"Roblox isn’t installed";
  const bool offer_update = newer && can_update && !updating;
  const wchar_t* const button_label = offer_update ? L"Update now" : L"Check now";
  const float check_width =
      std::max(ui.ButtonWidth(L"Check now", glyph::kRefresh), ui.ButtonWidth(L"Update now", glyph::kRefresh));
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
    cy = SettingRow(ui, cx, cy, cw, L"Update Roblox automatically",
                    L"Installs a newer version when Elixir starts. Your data is kept.",
                    96.0f, 32.0f, &control, false);
    if (ui.Toggle(L"roblox.update.auto", control, &config_.roblox.auto_update, L"Update Roblox automatically")) {
      SaveConfig();
    }
  }

  cy = SettingRow(ui, cx, cy, cw, L"Check when Elixir starts",
                  L"Notify me when a newer version of Roblox is available.",
                  96.0f, 32.0f, &control, false);
  if (ui.Toggle(L"roblox.update.startup", control, &config_.roblox.check_for_new_version,
                L"Check for new Roblox versions when Elixir starts")) {
    SaveConfig();
  }

  const InstalledApp& previous = config_.roblox.previous;
  if (previous.present() && previous.package == config_.installed.package) {
    const bool on_disk = PathIsDirectory(Utf8ToWide(previous.install_dir));
    const float back_width = ui.ButtonWidth(L"Roll back");
    cy = SettingRow(ui, cx, cy, cw, L"Roll back to Roblox " + Utf8ToWide(previous.version_name),
                    on_disk ? L"Goes back to the version you had before the last update."
                            : L"That version is no longer available.",
                    back_width, 32.0f, &control, false);
    if (ui.Button(L"roblox.rollback", control, L"Roll back", ButtonStyle::kStandard,
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
  cy += ui.Text(cx, cy, cw, L"Free up space", TextStyle::kCardTitle, theme::kText) + 6.0f;
  cy += ui.Text(cx, cy, cw,
                L"Roblox downloads what it needs again. Your sign-in, settings and Fast Flags are kept.",
                TextStyle::kSecondary, theme::kTextSecondary) + 12.0f;
  std::uint64_t total = 0;
  const float size_width = 110.0f;
  for (const CacheLocation& location : cache_.locations) {
    total += location.bytes;
    const float title_height = ui.Text(cx, cy, cw - size_width - 16.0f, location.title, TextStyle::kBody, theme::kText);
    const std::wstring description =
        location.redirected ? std::wstring(L"Skipped: this folder points somewhere else.")
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
    cy += ui.Text(cx, cy, cw, cache_.measured ? L"No cache folders." : L"Measuring…", TextStyle::kSecondary,
                  theme::kTextTertiary) + 10.0f;
  }
  const bool running = RuntimeRunning();
  const bool working = PackageWorkRunning();
  ui.Fill(D2D1::RectF(cx, cy, cx + cw, cy + 1.0f), theme::kDivider);
  cy += 12.0f;
  ButtonFlow flow(ui, cx, cx + cw, cy);
  if (flow.Add(L"cache.clear", L"Clear cache…", ButtonStyle::kDanger,
               !running && !working && !cache_.clearing && cache_.measured && total > 0, glyph::kBroom,
               L"Asks before deleting anything")) {
    Defer([this] { AskClearCaches(); });
  }
  cy = flow.bottom() + 10.0f;
  std::wstring status = cache_.measured ? std::wstring(L"Total ") + FormatByteSize(total) + L"." : std::wstring(L"Measuring…");
  if (running) status += L" Close Roblox to clear the cache.";
  if (working) status += L" Wait for Roblox to finish installing.";
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
