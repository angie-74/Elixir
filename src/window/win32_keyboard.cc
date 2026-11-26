// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "window/win32_keyboard.h"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>

#include <array>

namespace elixir::window {
namespace {

constexpr std::array<std::uint16_t, 128> BuildBaseTable() {
  std::array<std::uint16_t, 128> t{};
  t[0x01] = SDL_SCANCODE_ESCAPE;
  t[0x02] = SDL_SCANCODE_1;
  t[0x03] = SDL_SCANCODE_2;
  t[0x04] = SDL_SCANCODE_3;
  t[0x05] = SDL_SCANCODE_4;
  t[0x06] = SDL_SCANCODE_5;
  t[0x07] = SDL_SCANCODE_6;
  t[0x08] = SDL_SCANCODE_7;
  t[0x09] = SDL_SCANCODE_8;
  t[0x0A] = SDL_SCANCODE_9;
  t[0x0B] = SDL_SCANCODE_0;
  t[0x0C] = SDL_SCANCODE_MINUS;
  t[0x0D] = SDL_SCANCODE_EQUALS;
  t[0x0E] = SDL_SCANCODE_BACKSPACE;
  t[0x0F] = SDL_SCANCODE_TAB;
  t[0x10] = SDL_SCANCODE_Q;
  t[0x11] = SDL_SCANCODE_W;
  t[0x12] = SDL_SCANCODE_E;
  t[0x13] = SDL_SCANCODE_R;
  t[0x14] = SDL_SCANCODE_T;
  t[0x15] = SDL_SCANCODE_Y;
  t[0x16] = SDL_SCANCODE_U;
  t[0x17] = SDL_SCANCODE_I;
  t[0x18] = SDL_SCANCODE_O;
  t[0x19] = SDL_SCANCODE_P;
  t[0x1A] = SDL_SCANCODE_LEFTBRACKET;
  t[0x1B] = SDL_SCANCODE_RIGHTBRACKET;
  t[0x1C] = SDL_SCANCODE_RETURN;
  t[0x1D] = SDL_SCANCODE_LCTRL;
  t[0x1E] = SDL_SCANCODE_A;
  t[0x1F] = SDL_SCANCODE_S;
  t[0x20] = SDL_SCANCODE_D;
  t[0x21] = SDL_SCANCODE_F;
  t[0x22] = SDL_SCANCODE_G;
  t[0x23] = SDL_SCANCODE_H;
  t[0x24] = SDL_SCANCODE_J;
  t[0x25] = SDL_SCANCODE_K;
  t[0x26] = SDL_SCANCODE_L;
  t[0x27] = SDL_SCANCODE_SEMICOLON;
  t[0x28] = SDL_SCANCODE_APOSTROPHE;
  t[0x29] = SDL_SCANCODE_GRAVE;
  t[0x2A] = SDL_SCANCODE_LSHIFT;
  t[0x2B] = SDL_SCANCODE_BACKSLASH;
  t[0x2C] = SDL_SCANCODE_Z;
  t[0x2D] = SDL_SCANCODE_X;
  t[0x2E] = SDL_SCANCODE_C;
  t[0x2F] = SDL_SCANCODE_V;
  t[0x30] = SDL_SCANCODE_B;
  t[0x31] = SDL_SCANCODE_N;
  t[0x32] = SDL_SCANCODE_M;
  t[0x33] = SDL_SCANCODE_COMMA;
  t[0x34] = SDL_SCANCODE_PERIOD;
  t[0x35] = SDL_SCANCODE_SLASH;
  t[0x36] = SDL_SCANCODE_RSHIFT;
  t[0x37] = SDL_SCANCODE_KP_MULTIPLY;
  t[0x38] = SDL_SCANCODE_LALT;
  t[0x39] = SDL_SCANCODE_SPACE;
  t[0x3A] = SDL_SCANCODE_CAPSLOCK;
  t[0x3B] = SDL_SCANCODE_F1;
  t[0x3C] = SDL_SCANCODE_F2;
  t[0x3D] = SDL_SCANCODE_F3;
  t[0x3E] = SDL_SCANCODE_F4;
  t[0x3F] = SDL_SCANCODE_F5;
  t[0x40] = SDL_SCANCODE_F6;
  t[0x41] = SDL_SCANCODE_F7;
  t[0x42] = SDL_SCANCODE_F8;
  t[0x43] = SDL_SCANCODE_F9;
  t[0x44] = SDL_SCANCODE_F10;
  t[0x45] = SDL_SCANCODE_PAUSE;
  t[0x46] = SDL_SCANCODE_SCROLLLOCK;
  t[0x47] = SDL_SCANCODE_KP_7;
  t[0x48] = SDL_SCANCODE_KP_8;
  t[0x49] = SDL_SCANCODE_KP_9;
  t[0x4A] = SDL_SCANCODE_KP_MINUS;
  t[0x4B] = SDL_SCANCODE_KP_4;
  t[0x4C] = SDL_SCANCODE_KP_5;
  t[0x4D] = SDL_SCANCODE_KP_6;
  t[0x4E] = SDL_SCANCODE_KP_PLUS;
  t[0x4F] = SDL_SCANCODE_KP_1;
  t[0x50] = SDL_SCANCODE_KP_2;
  t[0x51] = SDL_SCANCODE_KP_3;
  t[0x52] = SDL_SCANCODE_KP_0;
  t[0x53] = SDL_SCANCODE_KP_PERIOD;
  t[0x54] = SDL_SCANCODE_PRINTSCREEN;
  t[0x56] = SDL_SCANCODE_NONUSBACKSLASH;
  t[0x57] = SDL_SCANCODE_F11;
  t[0x58] = SDL_SCANCODE_F12;
  t[0x59] = SDL_SCANCODE_KP_EQUALS;
  t[0x64] = SDL_SCANCODE_F13;
  t[0x65] = SDL_SCANCODE_F14;
  t[0x66] = SDL_SCANCODE_F15;
  t[0x67] = SDL_SCANCODE_F16;
  t[0x68] = SDL_SCANCODE_F17;
  t[0x69] = SDL_SCANCODE_F18;
  t[0x6A] = SDL_SCANCODE_F19;
  t[0x6B] = SDL_SCANCODE_F20;
  t[0x6C] = SDL_SCANCODE_F21;
  t[0x6D] = SDL_SCANCODE_F22;
  t[0x6E] = SDL_SCANCODE_F23;
  t[0x70] = SDL_SCANCODE_INTERNATIONAL2;
  t[0x73] = SDL_SCANCODE_INTERNATIONAL1;
  t[0x76] = SDL_SCANCODE_F24;
  t[0x79] = SDL_SCANCODE_INTERNATIONAL4;
  t[0x7B] = SDL_SCANCODE_INTERNATIONAL5;
  t[0x7D] = SDL_SCANCODE_INTERNATIONAL3;
  t[0x7E] = SDL_SCANCODE_KP_COMMA;
  return t;
}

constexpr std::array<std::uint16_t, 128> kBaseTable = BuildBaseTable();

std::uint32_t ExtendedScancode(std::uint32_t scan_code) {
  switch (scan_code) {
    case 0x1C:
      return SDL_SCANCODE_KP_ENTER;
    case 0x1D:
      return SDL_SCANCODE_RCTRL;
    case 0x20:
      return SDL_SCANCODE_MUTE;
    case 0x2E:
      return SDL_SCANCODE_VOLUMEDOWN;
    case 0x30:
      return SDL_SCANCODE_VOLUMEUP;
    case 0x35:
      return SDL_SCANCODE_KP_DIVIDE;
    case 0x37:
      return SDL_SCANCODE_PRINTSCREEN;
    case 0x38:
      return SDL_SCANCODE_RALT;
    case 0x45:
      return SDL_SCANCODE_NUMLOCKCLEAR;
    case 0x46:
      return SDL_SCANCODE_PAUSE;
    case 0x47:
      return SDL_SCANCODE_HOME;
    case 0x48:
      return SDL_SCANCODE_UP;
    case 0x49:
      return SDL_SCANCODE_PAGEUP;
    case 0x4B:
      return SDL_SCANCODE_LEFT;
    case 0x4D:
      return SDL_SCANCODE_RIGHT;
    case 0x4F:
      return SDL_SCANCODE_END;
    case 0x50:
      return SDL_SCANCODE_DOWN;
    case 0x51:
      return SDL_SCANCODE_PAGEDOWN;
    case 0x52:
      return SDL_SCANCODE_INSERT;
    case 0x53:
      return SDL_SCANCODE_DELETE;
    case 0x5B:
      return SDL_SCANCODE_LGUI;
    case 0x5C:
      return SDL_SCANCODE_RGUI;
    case 0x5D:
      return SDL_SCANCODE_APPLICATION;
    case 0x5E:
      return SDL_SCANCODE_POWER;
    default:
      return SDL_SCANCODE_UNKNOWN;
  }
}

bool IsExtendedOnlyVirtualKey(std::uint32_t virtual_key) {
  switch (virtual_key) {
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_APPS:
    case VK_DIVIDE:
    case VK_NUMLOCK:
    case VK_SNAPSHOT:
      return true;
    default:
      return false;
  }
}

bool IsKeypadScancode(std::uint32_t scancode) {
  return (scancode >= SDL_SCANCODE_NUMLOCKCLEAR &&
          scancode <= SDL_SCANCODE_KP_PERIOD) ||
         scancode == SDL_SCANCODE_KP_EQUALS || scancode == SDL_SCANCODE_KP_COMMA;
}

}

std::uint32_t Win32ScanCodeToSdlScancode(std::uint32_t scan_code, bool extended,
                                         std::uint32_t virtual_key) {
  switch (virtual_key) {
    case VK_PAUSE:
    case VK_CANCEL:
      return SDL_SCANCODE_PAUSE;
    case VK_NUMLOCK:
      return SDL_SCANCODE_NUMLOCKCLEAR;
    case VK_SNAPSHOT:
      return SDL_SCANCODE_PRINTSCREEN;
    default:
      break;
  }
  scan_code &= 0xFF;
  if (extended) {
    return ExtendedScancode(scan_code);
  }
  return scan_code < kBaseTable.size() ? kBaseTable[scan_code] : 0;
}

std::uint32_t SdlKeycodeForKey(std::uint32_t sdl_scancode,
                               std::uint32_t virtual_key, HKL layout) {
  switch (sdl_scancode) {
    case SDL_SCANCODE_RETURN:
      return SDLK_RETURN;
    case SDL_SCANCODE_ESCAPE:
      return SDLK_ESCAPE;
    case SDL_SCANCODE_BACKSPACE:
      return SDLK_BACKSPACE;
    case SDL_SCANCODE_TAB:
      return SDLK_TAB;
    case SDL_SCANCODE_SPACE:
      return SDLK_SPACE;
    case SDL_SCANCODE_DELETE:
      return SDLK_DELETE;
    default:
      break;
  }
  if (sdl_scancode == 0) {
    return SDLK_UNKNOWN;
  }
  if (IsKeypadScancode(sdl_scancode) || virtual_key == 0) {
    return SDL_SCANCODE_TO_KEYCODE(sdl_scancode);
  }
  const UINT character =
      MapVirtualKeyExW(virtual_key, MAPVK_VK_TO_CHAR, layout) & 0x7FFFFFFFu;
  if (character >= 0x20 && character != 0x7F) {
    wchar_t buffer[2] = {static_cast<wchar_t>(character), 0};
    CharLowerBuffW(buffer, 1);
    return static_cast<std::uint32_t>(buffer[0]);
  }
  return SDL_SCANCODE_TO_KEYCODE(sdl_scancode);
}

std::uint32_t CurrentSdlModifiers() {
  auto down = [](int vk) { return (GetKeyState(vk) & 0x8000) != 0; };
  auto toggled = [](int vk) { return (GetKeyState(vk) & 0x0001) != 0; };
  std::uint32_t modifiers = 0;
  if (down(VK_LSHIFT)) modifiers |= SDL_KMOD_LSHIFT;
  if (down(VK_RSHIFT)) modifiers |= SDL_KMOD_RSHIFT;
  if (down(VK_LCONTROL)) modifiers |= SDL_KMOD_LCTRL;
  if (down(VK_RCONTROL)) modifiers |= SDL_KMOD_RCTRL;
  if (down(VK_LMENU)) modifiers |= SDL_KMOD_LALT;
  if (down(VK_RMENU)) modifiers |= SDL_KMOD_RALT;
  if (down(VK_LWIN)) modifiers |= SDL_KMOD_LGUI;
  if (down(VK_RWIN)) modifiers |= SDL_KMOD_RGUI;
  if (toggled(VK_NUMLOCK)) modifiers |= SDL_KMOD_NUM;
  if (toggled(VK_CAPITAL)) modifiers |= SDL_KMOD_CAPS;
  if (toggled(VK_SCROLL)) modifiers |= SDL_KMOD_SCROLL;
  return modifiers;
}

DecodedKeyMessage DecodeKeyMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  DecodedKeyMessage decoded;
  if (message != WM_KEYDOWN && message != WM_KEYUP &&
      message != WM_SYSKEYDOWN && message != WM_SYSKEYUP) {
    return decoded;
  }
  const auto bits = static_cast<std::uint32_t>(lparam);
  decoded.pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
  decoded.repeat = decoded.pressed && (bits & (1u << 30)) != 0;
  decoded.alt_context = (bits & (1u << 29)) != 0;
  decoded.virtual_key = static_cast<std::uint32_t>(wparam) & 0xFF;
  decoded.scan_code = (bits >> 16) & 0xFF;
  decoded.extended = (bits & (1u << 24)) != 0;
  if (decoded.scan_code == 0 && decoded.virtual_key != 0) {
    const UINT recovered = MapVirtualKeyW(decoded.virtual_key, MAPVK_VK_TO_VSC_EX);
    decoded.scan_code = recovered & 0xFF;
    decoded.extended = (recovered & 0xFF00) == 0xE000 ||
                       IsExtendedOnlyVirtualKey(decoded.virtual_key);
  }
  decoded.sdl_scancode = Win32ScanCodeToSdlScancode(
      decoded.scan_code, decoded.extended, decoded.virtual_key);
  decoded.valid = true;
  return decoded;
}

mocktail::platform::KeyEvent MakeKeyEvent(const DecodedKeyMessage& decoded,
                                          std::uint32_t keycode,
                                          std::uint32_t modifiers) {
  mocktail::platform::KeyEvent event;
  event.pressed = decoded.pressed;
  event.repeat = decoded.repeat;
  event.scancode = decoded.sdl_scancode;
  event.keycode = keycode;
  event.modifiers = modifiers;
  return event;
}

}
