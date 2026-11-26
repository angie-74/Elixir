// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_WINDOW_WIN32_TEXT_INPUT_H_
#define ELIXIR_WINDOW_WIN32_TEXT_INPUT_H_

#include <windows.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "mocktail/platform/platform_runtime.h"
#include "window/window_text_input_owner.h"

namespace elixir::window {

bool RobloxTextInputTypeDisablesIme(int roblox_text_input_type);

std::string Utf16ToUtf8(const wchar_t* text, std::size_t length);
int CodePointsInUtf16Prefix(const std::wstring& text, std::size_t utf16_units);

class Win32TextInputBackend final : public mocktail::window::TextInputBackend {
 public:
  using TransformFn = std::function<mocktail::platform::SurfaceCoordinateTransform()>;

  Win32TextInputBackend() = default;
  void Attach(HWND hwnd, TransformFn transform);
  void Detach();

  bool SetArea(const mocktail::window::TextInputArea& area) override;
  bool ClearArea() override;
  bool Start(const mocktail::window::TextInputOptions& options) override;
  bool Stop() override;
  void FlushTextEvents() override;

  bool active() const { return active_; }
  bool ime_enabled() const { return ime_enabled_; }

  bool HandleChar(UINT message, WPARAM wparam,
                  std::optional<mocktail::platform::TextInputEvent>* out);
  bool HandleImeComposition(
      LPARAM lparam, std::optional<mocktail::platform::TextEditingEvent>* editing,
      std::optional<mocktail::platform::TextInputEvent>* result);
  bool HandleImeEndComposition(
      std::optional<mocktail::platform::TextEditingEvent>* editing);
  bool composing() const { return composing_; }
  void set_composing(bool composing) { composing_ = composing; }

 private:
  void EnableIme(bool enabled);
  void ApplyAreaToContext();

  HWND hwnd_ = nullptr;
  TransformFn transform_;
  bool active_ = false;
  bool ime_enabled_ = false;
  bool composing_ = false;
  bool has_area_ = false;
  RECT area_pixels_{};
  int cursor_pixels_ = 0;
  wchar_t pending_high_surrogate_ = 0;
};

}

#endif
