// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_LAUNCH_MENU_H_
#define ELIXIR_LAUNCHER_UI_LAUNCH_MENU_H_

#include <windows.h>
#include <commctrl.h>
#include <d2d1.h>
#include <wrl/client.h>

#include <functional>

namespace elixir::launcher::ui {

enum class LaunchMenuAction { kLaunch, kConfigure, kClose };
inline constexpr int kLaunchMenuPlayId = 1001;
inline constexpr int kLaunchMenuConfigureId = 1002;

class LaunchMenu {
 public:
  LaunchMenu() = default;
  ~LaunchMenu();
  LaunchMenu(const LaunchMenu&) = delete;
  LaunchMenu& operator=(const LaunchMenu&) = delete;

  bool Create(std::function<void(LaunchMenuAction)> on_action, int show_command = SW_SHOWNORMAL);
  HWND hwnd() const { return hwnd_; }
  bool Translate(MSG* message) const;

 private:
  static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  INT_PTR OnMessage(UINT message, WPARAM wparam, LPARAM lparam);
  void SetAppearance();
  void SetDpi(UINT dpi);
  void Layout();
  void DrawBackground(HDC dc) const;
  bool PrepareIcons();
  void DrawIconGeometry(HDC dc, ID2D1Geometry* geometry, int left, int center_y, int size,
                        COLORREF foreground, COLORREF background);
  void DrawAction(const NMCUSTOMDRAW& draw);

  HWND hwnd_ = nullptr;
  std::function<void(LaunchMenuAction)> on_action_;
  UINT dpi_ = 96;
  bool high_contrast_ = false;
  HFONT title_font_ = nullptr;
  HFONT version_font_ = nullptr;
  HFONT button_font_ = nullptr;
  Microsoft::WRL::ComPtr<ID2D1Factory> icon_factory_;
  Microsoft::WRL::ComPtr<ID2D1PathGeometry> chevron_;
  Microsoft::WRL::ComPtr<ID2D1StrokeStyle> icon_stroke_;
  Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> icon_target_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> icon_brush_;
  HICON brand_icon_ = nullptr;
  HICON small_icon_ = nullptr;
  HICON large_icon_ = nullptr;
  HBRUSH background_ = nullptr;
  RECT brand_bounds_{};
};

}
#endif
