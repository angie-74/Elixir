// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_LAUNCH_PROGRESS_H_
#define ELIXIR_LAUNCHER_UI_LAUNCH_PROGRESS_H_

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

#include "ui/renderer.h"
#include "ui/ui.h"

namespace elixir::launcher::ui {

inline constexpr float kLaunchProgressWidth = 500.0f;
inline constexpr float kLaunchProgressHeight = 280.0f;

class LaunchProgress {
 public:
  struct View {
    std::wstring message;
    std::wstring version;
    float progress = -1.0f;
    bool cancel_enabled = true;
  };

  LaunchProgress() = default;
  ~LaunchProgress();
  LaunchProgress(const LaunchProgress&) = delete;
  LaunchProgress& operator=(const LaunchProgress&) = delete;

  bool Show(View view, std::function<void()> on_cancel, bool offscreen = false);
  void Update(View view);
  void Close();
  bool open() const { return hwnd_ != nullptr; }
  HWND hwnd() const { return hwnd_; }
  const View& view() const { return view_; }
  RECT cancel_rect() const;
  const std::vector<ControlRecord>& controls() const { return records_; }

 private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnMessage(UINT message, WPARAM wparam, LPARAM lparam);
  void ApplyBackdrop(bool system_backdrop);
  void Paint();
  void Build(Ui& ui, float width, float height);
  bool OverCancel(POINT client) const;
  void Cancel();
  void ShowTaskbarProgress();

  HWND hwnd_ = nullptr;
  UINT dpi_ = 96;
  View view_;
  std::function<void()> on_cancel_;
  bool cancelled_ = false;
  bool backdrop_ = false;
  bool renderer_ok_ = false;
  bool renderer_ready_ = false;
  Renderer renderer_;
  UiState state_;
  Animator animator_;
  std::vector<ControlRecord> records_;
  D2D1_RECT_F cancel_{};
  ULONGLONG opened_at_ = 0;
  bool tracking_mouse_ = false;
};

}

#endif
