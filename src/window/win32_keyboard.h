// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_WINDOW_WIN32_KEYBOARD_H_
#define ELIXIR_WINDOW_WIN32_KEYBOARD_H_

#include <windows.h>

#include <cstdint>

#include "mocktail/platform/platform_runtime.h"

namespace elixir::window {

std::uint32_t Win32ScanCodeToSdlScancode(std::uint32_t scan_code, bool extended,
                                         std::uint32_t virtual_key);

std::uint32_t SdlKeycodeForKey(std::uint32_t sdl_scancode,
                               std::uint32_t virtual_key, HKL layout);

std::uint32_t CurrentSdlModifiers();

struct DecodedKeyMessage {
  bool valid = false;
  bool pressed = false;
  bool repeat = false;
  bool alt_context = false;
  std::uint32_t virtual_key = 0;
  std::uint32_t scan_code = 0;
  bool extended = false;
  std::uint32_t sdl_scancode = 0;
};

DecodedKeyMessage DecodeKeyMessage(UINT message, WPARAM wparam, LPARAM lparam);

mocktail::platform::KeyEvent MakeKeyEvent(const DecodedKeyMessage& decoded,
                                          std::uint32_t keycode,
                                          std::uint32_t modifiers);

}

#endif
