// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "window/win32_text_input.h"

#include <imm.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace elixir::window {

namespace platform = mocktail::platform;
namespace mwindow = mocktail::window;

namespace {

class ScopedImc final {
 public:
  explicit ScopedImc(HWND hwnd) : hwnd_(hwnd), himc_(ImmGetContext(hwnd)) {}
  ~ScopedImc() {
    if (himc_ != nullptr) {
      ImmReleaseContext(hwnd_, himc_);
    }
  }
  ScopedImc(const ScopedImc&) = delete;
  ScopedImc& operator=(const ScopedImc&) = delete;
  HIMC get() const { return himc_; }

 private:
  HWND hwnd_;
  HIMC himc_;
};

std::wstring ReadCompositionString(HIMC himc, DWORD index) {
  const LONG bytes = ImmGetCompositionStringW(himc, index, nullptr, 0);
  if (bytes <= 0) {
    return {};
  }
  std::wstring text(static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
  const LONG read = ImmGetCompositionStringW(himc, index, text.data(),
                                             static_cast<DWORD>(bytes));
  if (read <= 0) {
    return {};
  }
  text.resize(static_cast<std::size_t>(read) / sizeof(wchar_t));
  return text;
}

std::string CodePointToUtf8(std::uint32_t code_point) {
  std::string out;
  if (code_point < 0x80) {
    out.push_back(static_cast<char>(code_point));
  } else if (code_point < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else if (code_point < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  } else if (code_point <= 0x10FFFF) {
    out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
  }
  return out;
}

bool IsControl(std::uint32_t code_point) {
  return code_point < 0x20 || code_point == 0x7F ||
         (code_point >= 0x80 && code_point < 0xA0);
}

}

bool RobloxTextInputTypeDisablesIme(int roblox_text_input_type) {
  switch (roblox_text_input_type) {
    case 2:
    case 4:
    case 5:
    case 8:
    case 9:
    case 10:
      return true;
    default:
      return false;
  }
}

std::string Utf16ToUtf8(const wchar_t* text, std::size_t length) {
  if (text == nullptr || length == 0) {
    return {};
  }
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length),
                                        nullptr, 0, nullptr, nullptr);
  if (bytes <= 0) {
    return {};
  }
  std::string out(static_cast<std::size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length), out.data(),
                      bytes, nullptr, nullptr);
  return out;
}

int CodePointsInUtf16Prefix(const std::wstring& text, std::size_t utf16_units) {
  const std::size_t end = std::min(utf16_units, text.size());
  int count = 0;
  for (std::size_t i = 0; i < end; ++i) {
    const wchar_t unit = text[i];
    if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < text.size() &&
        text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
      ++i;
    }
    ++count;
  }
  return count;
}

void Win32TextInputBackend::Attach(HWND hwnd, TransformFn transform) {
  hwnd_ = hwnd;
  transform_ = std::move(transform);
  EnableIme(false);
}

void Win32TextInputBackend::Detach() {
  if (hwnd_ != nullptr && active_) {
    Stop();
  }
  hwnd_ = nullptr;
  transform_ = nullptr;
}

void Win32TextInputBackend::EnableIme(bool enabled) {
  if (hwnd_ == nullptr) {
    return;
  }
  ImmAssociateContextEx(hwnd_, nullptr, enabled ? IACE_DEFAULT : 0);
  ime_enabled_ = enabled;
}

bool Win32TextInputBackend::SetArea(const mwindow::TextInputArea& area) {
  if (hwnd_ == nullptr || !transform_) {
    return false;
  }
  const platform::SurfaceCoordinateTransform transform = transform_();
  if (!transform.valid()) {
    return false;
  }
  const int left =
      static_cast<int>(std::lround(transform.GuestToHostLogicalX(area.x)));
  const int top =
      static_cast<int>(std::lround(transform.GuestToHostLogicalY(area.y)));
  const int right = static_cast<int>(std::lround(
      transform.GuestToHostLogicalX(static_cast<float>(area.x) + area.width)));
  const int bottom = static_cast<int>(std::lround(
      transform.GuestToHostLogicalY(static_cast<float>(area.y) + area.height)));
  area_pixels_ = {left, top, left + std::max(1, right - left),
                  top + std::max(1, bottom - top)};
  const int native_cursor = std::max(0, std::min(area.cursor, area.width));
  const int cursor_edge = static_cast<int>(std::lround(transform.GuestToHostLogicalX(
      static_cast<float>(area.x) + native_cursor)));
  cursor_pixels_ = std::max(0, std::min(cursor_edge - left,
                                        static_cast<int>(area_pixels_.right - left)));
  has_area_ = true;
  ApplyAreaToContext();
  return true;
}

void Win32TextInputBackend::ApplyAreaToContext() {
  if (hwnd_ == nullptr) {
    return;
  }
  ScopedImc imc(hwnd_);
  if (imc.get() == nullptr) {
    return;
  }
  if (!has_area_) {
    COMPOSITIONFORM composition{};
    composition.dwStyle = CFS_DEFAULT;
    ImmSetCompositionWindow(imc.get(), &composition);
    return;
  }
  const POINT caret{area_pixels_.left + cursor_pixels_, area_pixels_.top};
  COMPOSITIONFORM composition{};
  composition.dwStyle = CFS_POINT;
  composition.ptCurrentPos = caret;
  ImmSetCompositionWindow(imc.get(), &composition);
  CANDIDATEFORM candidate{};
  candidate.dwIndex = 0;
  candidate.dwStyle = CFS_EXCLUDE;
  candidate.ptCurrentPos = caret;
  candidate.rcArea = area_pixels_;
  ImmSetCandidateWindow(imc.get(), &candidate);
}

bool Win32TextInputBackend::ClearArea() {
  has_area_ = false;
  ApplyAreaToContext();
  return hwnd_ != nullptr;
}

bool Win32TextInputBackend::Start(const mwindow::TextInputOptions& options) {
  if (hwnd_ == nullptr) {
    return false;
  }
  active_ = true;
  composing_ = false;
  pending_high_surrogate_ = 0;
  EnableIme(!RobloxTextInputTypeDisablesIme(options.roblox_text_input_type));
  ApplyAreaToContext();
  return true;
}

bool Win32TextInputBackend::Stop() {
  if (hwnd_ == nullptr) {
    return false;
  }
  if (composing_) {
    ScopedImc imc(hwnd_);
    if (imc.get() != nullptr) {
      ImmNotifyIME(imc.get(), NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    }
  }
  active_ = false;
  composing_ = false;
  pending_high_surrogate_ = 0;
  EnableIme(false);
  return true;
}

void Win32TextInputBackend::FlushTextEvents() {
  pending_high_surrogate_ = 0;
  if (hwnd_ == nullptr) {
    return;
  }
  MSG message;
  while (PeekMessageW(&message, hwnd_, WM_CHAR, WM_DEADCHAR, PM_REMOVE)) {
  }
  while (PeekMessageW(&message, hwnd_, WM_UNICHAR, WM_UNICHAR, PM_REMOVE)) {
  }
}

bool Win32TextInputBackend::HandleChar(
    UINT message, WPARAM wparam, std::optional<platform::TextInputEvent>* out) {
  out->reset();
  if (message == WM_UNICHAR) {
    if (wparam == UNICODE_NOCHAR) {
      return true;
    }
    if (!active_) {
      return true;
    }
    const auto code_point = static_cast<std::uint32_t>(wparam);
    if (!IsControl(code_point) && code_point <= 0x10FFFF &&
        !(code_point >= 0xD800 && code_point <= 0xDFFF)) {
      *out = platform::TextInputEvent{CodePointToUtf8(code_point)};
    }
    return true;
  }
  if (!active_) {
    return true;
  }
  const auto unit = static_cast<wchar_t>(wparam & 0xFFFF);
  if (unit >= 0xD800 && unit <= 0xDBFF) {
    pending_high_surrogate_ = unit;
    return true;
  }
  std::uint32_t code_point = unit;
  if (unit >= 0xDC00 && unit <= 0xDFFF) {
    if (pending_high_surrogate_ == 0) {
      return true;
    }
    code_point = 0x10000 + ((static_cast<std::uint32_t>(pending_high_surrogate_) -
                             0xD800) << 10) +
                 (static_cast<std::uint32_t>(unit) - 0xDC00);
  }
  pending_high_surrogate_ = 0;
  if (IsControl(code_point)) {
    return true;
  }
  *out = platform::TextInputEvent{CodePointToUtf8(code_point)};
  return true;
}

bool Win32TextInputBackend::HandleImeComposition(
    LPARAM lparam, std::optional<platform::TextEditingEvent>* editing,
    std::optional<platform::TextInputEvent>* result) {
  editing->reset();
  result->reset();
  if (!active_ || hwnd_ == nullptr) {
    return false;
  }
  ScopedImc imc(hwnd_);
  if (imc.get() == nullptr) {
    return true;
  }
  if ((lparam & GCS_RESULTSTR) != 0) {
    const std::wstring text = ReadCompositionString(imc.get(), GCS_RESULTSTR);
    if (!text.empty()) {
      *result = platform::TextInputEvent{Utf16ToUtf8(text.data(), text.size())};
    }
  }
  if ((lparam & GCS_COMPSTR) != 0) {
    const std::wstring text = ReadCompositionString(imc.get(), GCS_COMPSTR);
    composing_ = !text.empty();
    platform::TextEditingEvent event;
    event.text = Utf16ToUtf8(text.data(), text.size());
    const LONG cursor_units = ImmGetCompositionStringW(imc.get(), GCS_CURSORPOS, nullptr, 0);
    event.start = CodePointsInUtf16Prefix(
        text, cursor_units > 0 ? static_cast<std::size_t>(cursor_units) : 0);
    event.length = 0;
    const LONG attribute_bytes =
        ImmGetCompositionStringW(imc.get(), GCS_COMPATTR, nullptr, 0);
    if (attribute_bytes > 0) {
      std::vector<BYTE> attributes(static_cast<std::size_t>(attribute_bytes));
      ImmGetCompositionStringW(imc.get(), GCS_COMPATTR, attributes.data(),
                               static_cast<DWORD>(attributes.size()));
      std::size_t begin = attributes.size();
      std::size_t end = attributes.size();
      for (std::size_t i = 0; i < attributes.size(); ++i) {
        const bool target = attributes[i] == ATTR_TARGET_CONVERTED ||
                            attributes[i] == ATTR_TARGET_NOTCONVERTED;
        if (target && begin == attributes.size()) {
          begin = i;
        }
        if (!target && begin != attributes.size() && end == attributes.size()) {
          end = i;
        }
      }
      if (begin != attributes.size()) {
        event.start = CodePointsInUtf16Prefix(text, begin);
        event.length = CodePointsInUtf16Prefix(text, end) - event.start;
      }
    }
    *editing = std::move(event);
  } else if ((lparam & GCS_RESULTSTR) == 0) {
    composing_ = false;
    *editing = platform::TextEditingEvent{};
  }
  return true;
}

bool Win32TextInputBackend::HandleImeEndComposition(
    std::optional<platform::TextEditingEvent>* editing) {
  editing->reset();
  if (!active_) {
    return false;
  }
  composing_ = false;
  *editing = platform::TextEditingEvent{};
  return true;
}

}
