// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "window/win32_text_clipboard.h"

#include <cstring>
#include <limits>
#include <string>

#include "window/win32_game_window.h"

namespace elixir::window {

namespace {

using mocktail::Status;
using mocktail::StatusCode;

HWND ClipboardOwner(HWND explicit_owner) {
  if (explicit_owner != nullptr) {
    return explicit_owner;
  }
  CurrentGameWindowRef window;
  return window ? window->hwnd_any_thread() : nullptr;
}

bool OpenClipboardWithRetry(HWND owner) {
  for (int attempt = 0; attempt < 10; ++attempt) {
    if (OpenClipboard(owner)) {
      return true;
    }
    Sleep(5);
  }
  return false;
}

class Win32TextClipboard final : public mocktail::platform::TextClipboard {
 public:
  explicit Win32TextClipboard(HWND owner) : owner_(owner) {}

  Status ReadText(std::string* text) override {
    if (text == nullptr) {
      return Status::Error(StatusCode::kInvalidArgument, "clipboard output is required");
    }
    text->clear();
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
      return Status::Ok();
    }
    if (!OpenClipboardWithRetry(ClipboardOwner(owner_))) {
      return Status::Error(StatusCode::kUnavailable, "clipboard is busy");
    }
    Status status = Status::Ok();
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr) {
      const auto* wide = static_cast<const wchar_t*>(GlobalLock(data));
      if (wide != nullptr) {
        const SIZE_T capacity = GlobalSize(data) / sizeof(wchar_t);
        SIZE_T length = 0;
        while (length < capacity && wide[length] != L'\0') {
          ++length;
        }
        const int bytes =
            length > static_cast<SIZE_T>(std::numeric_limits<int>::max())
                ? 0
                : WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length), nullptr, 0,
                                      nullptr, nullptr);
        if (length > static_cast<SIZE_T>(std::numeric_limits<int>::max())) {
          status = Status::Error(StatusCode::kInvalidArgument, "clipboard text is too large");
        } else if (bytes > 0) {
          text->resize(static_cast<std::size_t>(bytes));
          WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length), text->data(),
                              bytes, nullptr, nullptr);
        }
        GlobalUnlock(data);
      } else {
        status = Status::Error(StatusCode::kPlatformError, "cannot lock clipboard text");
      }
    }
    CloseClipboard();
    return status;
  }

  Status WriteText(const std::string& text) override {
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      return Status::Error(StatusCode::kInvalidArgument, "clipboard text is too large");
    }
    const int units = text.empty() ? 0
                                   : MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                         text.data(),
                                                         static_cast<int>(text.size()),
                                                         nullptr, 0);
    if (!text.empty() && units <= 0) {
      return Status::Error(StatusCode::kInvalidArgument, "clipboard text is not UTF-8");
    }
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (static_cast<SIZE_T>(units) + 1) * sizeof(wchar_t));
    if (memory == nullptr) {
      return Status::Error(StatusCode::kPlatformError, "cannot allocate clipboard text");
    }
    auto* wide = static_cast<wchar_t*>(GlobalLock(memory));
    if (wide == nullptr) {
      GlobalFree(memory);
      return Status::Error(StatusCode::kPlatformError, "cannot lock clipboard text");
    }
    if (units > 0) {
      MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide, units);
    }
    wide[units] = L'\0';
    GlobalUnlock(memory);
    if (!OpenClipboardWithRetry(ClipboardOwner(owner_))) {
      GlobalFree(memory);
      return Status::Error(StatusCode::kUnavailable, "clipboard is busy");
    }
    EmptyClipboard();
    const bool stored = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
    CloseClipboard();
    if (!stored) {
      GlobalFree(memory);
      return Status::Error(StatusCode::kPlatformError, "cannot store clipboard text");
    }
    return Status::Ok();
  }

 private:
  HWND owner_ = nullptr;
};

}

std::unique_ptr<mocktail::platform::TextClipboard> CreateWin32TextClipboard(HWND owner) {
  return std::make_unique<Win32TextClipboard>(owner);
}

}
