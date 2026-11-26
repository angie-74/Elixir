// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_WEBVIEW_WEBVIEW_HOST_WINDOWS_H_
#define ELIXIR_WEBVIEW_WEBVIEW_HOST_WINDOWS_H_

namespace elixir {

inline constexpr wchar_t kWebViewModeFlag[] = L"--elixir-webview";
inline constexpr wchar_t kWebViewControlFlag[] = L"--control";
inline constexpr char kWebViewDataDirEnvironment[] = "ELIXIR_WEBVIEW_DATA_DIR";

int WebViewHostMain(int argc, wchar_t** argv);

}

#endif
