// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "core/i18n.h"
#include "core/shaders.h"
#include "core/util.h"
#include "ui/app.h"
#include "ui/theme.h"

namespace elixir::launcher::ui {

using Microsoft::WRL::ComPtr;

namespace {

std::wstring Percent(double value) { return std::to_wstring(static_cast<int>(std::lround(value * 100.0))) + L"%"; }

std::wstring Number(double value, double step) {
  wchar_t text[32];
  std::swprintf(text, 32, step >= 1.0 ? L"%.0f" : step >= 0.1 ? L"%.1f" : step >= 0.01 ? L"%.2f" : L"%.3f", value);
  return text;
}

std::wstring ShaderTitle(const shaders::InstalledShader& shader) {
  std::wstring name = Utf8ToWide(shader.name);
  if (!shader.folder) {
    const std::size_t dot = name.rfind(L'.');
    if (dot != std::wstring::npos && dot > 0) name.resize(dot);
  }
  return name;
}

}

void App::RefreshShaders() {
  if (shaders_.loading) return;
  shaders_.loading = true;
  const std::wstring folder = paths_.shaders_dir;
  RunInBackground([this, folder]() -> std::function<void()> {
    std::wstring error;
    InstallDefaultShaders(folder, false, &error);
    shaders::ShaderLibrary library = shaders::ReadShaderLibrary(folder);
    std::vector<std::wstring> problems;
    for (const shaders::InstalledShader& shader : library.shaders) problems.push_back(CheckShader(shader));
    std::vector<std::wstring> recovered = TakeShaderRecoveryNote(folder);
    return [this, library = std::move(library), problems = std::move(problems), recovered = std::move(recovered),
            error = std::move(error)]() mutable {
      shaders_.library = std::move(library);
      shaders_.problems = std::move(problems);
      if (!recovered.empty()) shaders_.recovered = std::move(recovered);
      if (!error.empty()) shaders_.message = error;
      shaders_.loaded = true;
      shaders_.loading = false;
      if (shaders_.open >= static_cast<int>(shaders_.library.shaders.size())) shaders_.open = -1;
      Invalidate();
    };
  });
}

void App::SaveShaders() {
  if (!shaders::WriteShaderLibrary(paths_.shaders_dir, shaders_.library)) {
    shaders_.message = T(L"The shader settings couldn’t be saved.");
  }
  Invalidate();
}

void App::ImportShaderFile() {
  if (options_.test_mode || shaders_.working) return;
  ComPtr<IFileOpenDialog> dialog;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
  const COMDLG_FILTERSPEC filters[] = {{T(L"Shaders (*.frag, *.glsl)").c_str(), L"*.frag;*.glsl"},
                                       {T(L"All files (*.*)").c_str(), L"*.*"}};
  dialog->SetFileTypes(2, filters);
  dialog->SetTitle(T(L"Choose a shader").c_str());
  FILEOPENDIALOGOPTIONS flags = 0;
  dialog->GetOptions(&flags);
  dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
  if (FAILED(dialog->Show(hwnd_))) return;
  ComPtr<IShellItem> item;
  PWSTR path = nullptr;
  if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return;
  const std::wstring chosen = path;
  CoTaskMemFree(path);
  ImportShaderFrom(chosen);
}

void App::ImportShaderFrom(const std::wstring& source) {
  const ShaderImport imported = ImportShader(paths_.shaders_dir, source);
  shaders_.message = imported.ok ? std::wstring() : imported.error;
  RefreshShaders();
  Invalidate();
}

void App::ExportShaderAt(std::size_t index) {
  if (options_.test_mode || index >= shaders_.library.shaders.size()) return;
  ComPtr<IFileOpenDialog> dialog;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
  dialog->SetTitle(T(L"Choose where to export the shader").c_str());
  FILEOPENDIALOGOPTIONS flags = 0;
  dialog->GetOptions(&flags);
  dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
  if (FAILED(dialog->Show(hwnd_))) return;
  ComPtr<IShellItem> item;
  PWSTR path = nullptr;
  if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return;
  const std::wstring chosen = path;
  CoTaskMemFree(path);
  std::wstring error;
  shaders_.message.clear();
  if (!ExportShader(paths_.shaders_dir, shaders_.library.shaders[index].name, chosen, &error)) shaders_.message = error;
  Invalidate();
}

void App::RemoveShaderAt(std::size_t index) {
  if (index >= shaders_.library.shaders.size()) return;
  const shaders::InstalledShader shader = shaders_.library.shaders[index];
  std::wstring error;
  if (!RemoveShader(paths_.shaders_dir, shader.name, &error)) {
    shaders_.message = error;
    Invalidate();
    return;
  }
  shaders_.message.clear();
  shaders_.library.shaders.erase(shaders_.library.shaders.begin() + static_cast<std::ptrdiff_t>(index));
  if (index < shaders_.problems.size()) {
    shaders_.problems.erase(shaders_.problems.begin() + static_cast<std::ptrdiff_t>(index));
  }
  shaders_.open = -1;
  Invalidate();
}

void App::MoveShader(std::size_t index, int delta) {
  std::vector<shaders::InstalledShader>& list = shaders_.library.shaders;
  const std::ptrdiff_t target = static_cast<std::ptrdiff_t>(index) + delta;
  if (index >= list.size() || target < 0 || target >= static_cast<std::ptrdiff_t>(list.size())) return;
  std::swap(list[index], list[static_cast<std::size_t>(target)]);
  if (index < shaders_.problems.size() && static_cast<std::size_t>(target) < shaders_.problems.size()) {
    std::swap(shaders_.problems[index], shaders_.problems[static_cast<std::size_t>(target)]);
  }
  shaders_.open = static_cast<int>(target);
  SaveShaders();
}

void App::ResetShaders() {
  std::wstring error;
  shaders_.message.clear();
  if (!ResetShadersToDefaults(paths_.shaders_dir, &error)) shaders_.message = error;
  shaders_.open = -1;
  config_.graphics.upscaling = false;
  config_.graphics.sharpness = 0.5;
  config_.graphics.dynamic_resolution = false;
  config_.graphics.target_fps = 60;
  SaveConfig();
  RefreshShaders();
}

float App::BuildShaders(Ui& ui, float x, float y, float width) {
  y = PageHeader(ui, x, y, width, T(L"Shaders"), L"");
  const float cx = x + theme::kCardPadding;
  const float cw = width - 2.0f * theme::kCardPadding;
  D2D1_RECT_F control{};
  if (!shaders_.recovered.empty()) {
    std::wstring names;
    for (const std::wstring& name : shaders_.recovered) names += (names.empty() ? L"" : L", ") + name;
    y = Banner(ui, x, y, width, 1,
               TF(L"Elixir turned off {0} because Roblox closed while it was starting.", names), L"") +
        12.0f;
  }

  BeginOptions();
  float cy = y;
  cy = SettingRow(ui, cx, cy, cw, T(L"Enable shaders"), T(L"Draws the shaders that are on over the game."),
                  ui.ToggleWidth(), 32.0f, &control, true);
  if (ui.Toggle(L"shaders.enabled", control, &shaders_.library.enabled, T(L"Enable shaders"), shaders_.loaded)) {
    SaveShaders();
  }
  y = EndOptions(cy) + 24.0f;

  y = SectionTitle(ui, x, y, width, T(L"Installed shaders"));
  const int card = ui.BeginCard(x, y, width);
  cy = y + 8.0f;
  std::vector<shaders::InstalledShader>& list = shaders_.library.shaders;
  const float toggle_width = ui.ToggleWidth();
  constexpr float kRowHeight = 48.0f;
  for (std::size_t index = 0; index < list.size(); ++index) {
    shaders::InstalledShader& shader = list[index];
    const std::wstring title = ShaderTitle(shader);
    const std::wstring key = std::to_wstring(index);
    const bool open = shaders_.open == static_cast<int>(index);
    if (index > 0) ui.Line(cx, cy, cx + cw, cy, theme::White(0.06f));
    const D2D1_RECT_F name_rect = D2D1::RectF(cx, cy + 7.0f, cx + cw - toggle_width - 16.0f, cy + 7.0f + theme::kButtonHeight);
    if (ui.Button(L"shaders.open#" + key, name_rect, L"", ButtonStyle::kSubtle, true, 0,
                  TF(L"Show the settings of {0}", title))) {
      shaders_.open = open ? -1 : static_cast<int>(index);
      Invalidate();
    }
    ui.TextLine(D2D1::RectF(name_rect.left + 12.0f, name_rect.top, name_rect.right - 12.0f, name_rect.bottom), title,
                TextStyle::kBody, shader.enabled ? theme::kText : theme::kTextSecondary);
    const D2D1_RECT_F toggle = D2D1::RectF(cx + cw - toggle_width, cy + 8.0f, cx + cw, cy + 40.0f);
    if (ui.Toggle(L"shaders.on#" + key, toggle, &shader.enabled, title)) SaveShaders();
    cy += kRowHeight;
    const std::wstring& problem = index < shaders_.problems.size() ? shaders_.problems[index] : std::wstring();
    if (!problem.empty()) {
      const float height = ui.Text(cx + 12.0f, cy - 6.0f, cw - 24.0f, problem, TextStyle::kCaption, theme::kFail);
      ui.Label(D2D1::RectF(cx, cy - 6.0f, cx + cw, cy - 6.0f + height), problem);
      cy += height + 4.0f;
    }
    if (!open) continue;
    for (const shaders::ShaderParameter& parameter : shader.parameters) {
      const std::wstring label = Utf8ToWide(parameter.label);
      const double step = parameter.step > 0.0f ? parameter.step
                                                 : std::max(0.001, (parameter.maximum - parameter.minimum) / 100.0);
      double value = shaders::ParameterValue(shader, parameter);
      const float label_width = std::min(cw * 0.4f, 220.0f);
      ui.TextLine(D2D1::RectF(cx + 12.0f, cy, cx + label_width, cy + 40.0f), label, TextStyle::kBody,
                  theme::kTextSecondary);
      ui.Label(D2D1::RectF(cx + 12.0f, cy, cx + label_width, cy + 40.0f), label);
      const D2D1_RECT_F slider = D2D1::RectF(cx + label_width + 12.0f, cy + 4.0f, cx + cw, cy + 36.0f);
      if (ui.Slider(L"shaders.value#" + key + L"." + Utf8ToWide(parameter.name), slider, &value, parameter.minimum,
                    parameter.maximum, step, label, Number(value, step), true, 64.0f)) {
        shader.values[parameter.name] = static_cast<float>(value);
        SaveShaders();
      }
      cy += 40.0f;
    }
    float bx = cx + 12.0f;
    const auto action = [&](const wchar_t* id, const std::wstring& label, bool enabled,
                            std::function<void()> run) {
      const float button_width = ui.ButtonWidth(label);
      if (bx + button_width > cx + cw) {
        bx = cx + 12.0f;
        cy += theme::kButtonHeight + 8.0f;
      }
      if (ui.Button(std::wstring(id) + L"#" + key, D2D1::RectF(bx, cy + 4.0f, bx + button_width, cy + 4.0f + theme::kButtonHeight),
                    label, ButtonStyle::kStandard, enabled, 0, label + L": " + title)) {
        Defer(std::move(run));
      }
      bx += button_width + 8.0f;
    };
    action(L"shaders.up", T(L"Move up"), index > 0, [this, index] { MoveShader(index, -1); });
    action(L"shaders.down", T(L"Move down"), index + 1 < list.size(), [this, index] { MoveShader(index, 1); });
    action(L"shaders.export", T(L"Export"), true, [this, index] { ExportShaderAt(index); });
    action(L"shaders.remove", T(L"Remove"), true, [this, index] { RemoveShaderAt(index); });
    cy += theme::kButtonHeight + 16.0f;
  }
  if (list.empty()) {
    const std::wstring& none = T(L"No shaders are installed.");
    const float height = ui.Text(cx, cy + 12.0f, cw, none, TextStyle::kBody, theme::kTextSecondary);
    ui.Label(D2D1::RectF(cx, cy + 12.0f, cx + cw, cy + 12.0f + height), none);
    cy += 12.0f + height + 12.0f;
  }
  y = ui.EndCard(card, cy - 12.0f) + 12.0f;

  {
    float bx = x;
    float by = y;
    const auto button = [&](const wchar_t* id, const std::wstring& label, std::function<void()> run) {
      const float button_width = ui.ButtonWidth(label);
      if (bx + button_width > x + width) {
        bx = x;
        by += theme::kButtonHeight + 8.0f;
      }
      if (ui.Button(id, D2D1::RectF(bx, by, bx + button_width, by + theme::kButtonHeight), label,
                    ButtonStyle::kStandard, shaders_.loaded)) {
        Defer(std::move(run));
      }
      bx += button_width + 8.0f;
    };
    button(L"shaders.import", T(L"Import shader"), [this] { ImportShaderFile(); });
    button(L"shaders.reload", T(L"Reload shaders"), [this] { RefreshShaders(); });
    button(L"shaders.folder", T(L"Open shaders folder"), [this] { OpenFolder(paths_.shaders_dir); });
    button(L"shaders.reset", T(L"Reset to defaults"), [this] { ResetShaders(); });
    y = by + theme::kButtonHeight;
  }
  if (!shaders_.message.empty()) y = Banner(ui, x, y + 12.0f, width, 2, shaders_.message, L"");

  y += 24.0f;
  y = SectionTitle(ui, x, y, width, T(L"Upscaling"));
  BeginOptions();
  cy = y;
  GraphicsSettings& graphics = config_.graphics;
  cy = SettingRow(ui, cx, cy, cw, T(L"Enable upscaling"), T(L"Sharpens a smaller game picture to the window’s size."),
                  ui.ToggleWidth(), 32.0f, &control, true);
  if (ui.Toggle(L"shaders.upscaling", control, &graphics.upscaling, T(L"Enable upscaling"))) SaveConfig();
  {
    double scale = graphics.resolution_scale;
    cy = SettingRow(ui, cx, cy, cw, T(L"Render scale"), T(L"Lower values give more FPS in games. Menus stay sharp."),
                    300.0f, 32.0f, &control, false);
    if (ui.Slider(L"shaders.scale", control, &scale, kResolutionScaleMin, kResolutionScaleMax, 0.05,
                  T(L"Render scale"), Percent(scale), true, 64.0f)) {
      graphics.resolution_scale = scale;
      SaveConfig();
    }
  }
  {
    double sharpness = graphics.sharpness;
    cy = SettingRow(ui, cx, cy, cw, T(L"Sharpness"), L"", 300.0f, 32.0f, &control, false);
    if (ui.Slider(L"shaders.sharpness", control, &sharpness, 0.0, 1.0, 0.05, T(L"Sharpness"), Percent(sharpness),
                  graphics.upscaling, 64.0f)) {
      graphics.sharpness = sharpness;
      SaveConfig();
    }
  }
  cy = SettingRow(ui, cx, cy, cw, T(L"Dynamic resolution"),
                  T(L"Lowers the render scale when the frame rate falls below the target."), ui.ToggleWidth(), 32.0f,
                  &control, false);
  if (ui.Toggle(L"shaders.dynamic", control, &graphics.dynamic_resolution, T(L"Dynamic resolution"),
                graphics.upscaling)) {
    SaveConfig();
  }
  {
    double target = graphics.target_fps;
    cy = SettingRow(ui, cx, cy, cw, T(L"Target FPS"), L"", 300.0f, 32.0f, &control, false);
    if (ui.Slider(L"shaders.target", control, &target, kTargetFpsMin, kTargetFpsMax, 5.0, T(L"Target FPS"),
                  std::to_wstring(static_cast<int>(std::lround(target))), graphics.upscaling && graphics.dynamic_resolution,
                  64.0f)) {
      graphics.target_fps = static_cast<int>(std::lround(target));
      SaveConfig();
    }
  }
  return EndOptions(cy);
}

}
