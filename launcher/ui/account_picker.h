// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_ACCOUNT_PICKER_H_
#define ELIXIR_LAUNCHER_UI_ACCOUNT_PICKER_H_

#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ui/accessibility.h"
#include "ui/account_list.h"
#include "ui/renderer.h"
#include "ui/ui.h"

namespace elixir::launcher::ui {

inline constexpr wchar_t kAccountPickerControl[] = L"launch.accounts";
inline constexpr wchar_t kAccountPickerWindowClass[] = L"ElixirAccountPicker";

class AccountPicker final : public AccessibilityHost {
 public:
  struct Request {
    HWND owner = nullptr;
    HWND anchor = nullptr;
    std::vector<AccountRow> rows;
    int focus = 0;
    bool keyboard = false;
    bool motion = true;
    bool offscreen = false;
    RECT bounds{};
    std::uint32_t accent = kDefaultControlAccentRgb;
    std::function<void(int)> on_open;
    std::function<void(int)> on_remove;
  };

  AccountPicker() = default;
  ~AccountPicker() override;
  AccountPicker(const AccountPicker&) = delete;
  AccountPicker& operator=(const AccountPicker&) = delete;

  bool Show(Request request);
  void SetRows(std::vector<AccountRow> rows);
  void Close();
  bool open() const { return hwnd_ != nullptr; }
  double OpenForMs() const;
  HWND hwnd() const { return hwnd_; }
  bool Translate(const MSG* message);
  bool Activate(const std::wstring& id);
  void Focus(int row) { MoveFocus(row, state_.focus_visible); }
  void PaintNow();
  const std::vector<ControlRecord>& controls() const { return records_; }
  const std::wstring& focused_control() const { return state_.focused; }
  bool focus_visible() const { return state_.focus_visible; }
  float scroll() const { return scroll_; }
  const RECT& placement() const { return placement_; }

  HWND AccessibleWindow() const override { return hwnd_; }
  const std::vector<ControlRecord>& AccessibleControls() const override { return records_; }
  float AccessibleScale() const override { return scale(); }
  int AccessibleFocusIndex() const override;
  void AccessibleActivate(int index) override;
  void AccessibleFocus(int index) override;

 private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK OwnerProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id,
                                    DWORD_PTR data);
  LRESULT OnMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  bool OwnerDismisses(UINT message, WPARAM wparam, LPARAM lparam) const;
  RECT Placement();
  int SlideOffset() const;
  void Paint();
  AccountListChoice Build(Ui& ui);
  void Act(const AccountListChoice& choice);
  const ControlRecord* HitTest(POINT client) const;
  void SetHovered(const std::wstring& id);
  void MoveFocus(int row, bool visible);
  void NotifyFocus();
  void ScrollBy(float delta);
  void Advance();
  void UpdateTimer();
  float scale() const { return static_cast<float>(dpi_) / 96.0f; }

  HWND hwnd_ = nullptr;
  HWND owner_ = nullptr;
  HWND anchor_ = nullptr;
  RECT owner_rect_{};
  RECT bounds_{};
  RECT placement_{};
  bool owner_visible_ = false;
  bool watching_ = false;
  UINT dpi_ = 96;
  std::vector<AccountRow> rows_;
  std::function<void(int)> on_open_;
  std::function<void(int)> on_remove_;
  bool motion_ = true;
  bool offscreen_ = false;
  bool renderer_ready_ = false;
  bool renderer_ok_ = false;
  Renderer renderer_;
  UiState state_;
  Animator animator_;
  std::vector<ControlRecord> records_;
  std::wstring pending_activation_;
  int focus_ = 0;
  float scroll_ = 0.0f;
  float visible_height_ = 0.0f;
  POINT origin_{};
  int slide_from_ = 0;
  double opened_at_ = 0.0;
  bool appearing_ = false;
  bool moving_ = false;
  bool timer_ = false;
  bool tracking_mouse_ = false;
  AccessibleRoot* accessible_ = nullptr;
};

}

#endif
