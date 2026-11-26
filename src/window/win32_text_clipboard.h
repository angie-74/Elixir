// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_WINDOW_WIN32_TEXT_CLIPBOARD_H_
#define ELIXIR_WINDOW_WIN32_TEXT_CLIPBOARD_H_

#include <windows.h>

#include <memory>

#include "mocktail/platform/text_clipboard.h"

namespace elixir::window {

std::unique_ptr<mocktail::platform::TextClipboard> CreateWin32TextClipboard(
    HWND owner = nullptr);

}

#endif
