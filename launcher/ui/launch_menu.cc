// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "ui/launch_menu.h"

#include <dwmapi.h>

#include <algorithm>
#include <utility>

#include "resource.h"
#include "ui/lucide.h"
#include "ui/ui.h"

namespace elixir::launcher::ui {
namespace {

constexpr int kTitleId = 1010;
constexpr int kVersionId = 1011;
constexpr int kClientWidth = 486;
constexpr int kClientHeight = 150;
constexpr int kStackThreshold = 370;
constexpr DWORD kWindowStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
constexpr DWORD kExStyle = WS_EX_APPWINDOW;
constexpr COLORREF kSurface = RGB(32, 32, 32);
constexpr COLORREF kForeground = RGB(242, 242, 242);

#pragma pack(push, 2)
struct EmptyDialog {
  DLGTEMPLATE header;
  WORD menu = 0, window_class = 0, title = 0;
};
#pragma pack(pop)

int Px(int dips, UINT dpi) { return MulDiv(dips, dpi, 96); }

D2D1_COLOR_F D2DColor(COLORREF rgb) {
  return D2D1::ColorF(GetRValue(rgb) / 255.0f, GetGValue(rgb) / 255.0f, GetBValue(rgb) / 255.0f);
}

void SetFont(HFONT& font, int dips, int weight, UINT dpi, const wchar_t* family) {
  HFONT replacement = CreateFontW(-Px(dips, dpi), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, family);
  if (replacement != nullptr) {
    if (font != nullptr) DeleteObject(font);
    font = replacement;
  }
}

void SetIcon(HICON& icon, int pixels) {
  HICON replacement = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ELIXIR),
                                                     IMAGE_ICON, pixels, pixels, LR_DEFAULTCOLOR));
  if (replacement != nullptr) {
    if (icon != nullptr) DestroyIcon(icon);
    icon = replacement;
  }
}

}

LaunchMenu::~LaunchMenu() {
  if (hwnd_ != nullptr) DestroyWindow(hwnd_);
  for (auto font : {title_font_, version_font_, button_font_}) {
    if (font != nullptr) DeleteObject(font);
  }
  for (auto icon : {brand_icon_, small_icon_, large_icon_}) {
    if (icon != nullptr) DestroyIcon(icon);
  }
  if (background_ != nullptr) DeleteObject(background_);
}

bool LaunchMenu::Create(std::function<void(LaunchMenuAction)> on_action, int show_command) {
  if (hwnd_ != nullptr || !on_action) return false;
  on_action_ = std::move(on_action);
  if (background_ == nullptr) background_ = CreateSolidBrush(kSurface);
  if (background_ == nullptr) return false;
  alignas(DWORD) EmptyDialog dialog{};
  dialog.header.style = kWindowStyle;
  dialog.header.dwExtendedStyle = kExStyle;
  dialog.header.cx = 200;
  dialog.header.cy = 80;
  if (!CreateDialogIndirectParamW(GetModuleHandleW(nullptr), &dialog.header, nullptr, DialogProc,
                                  reinterpret_cast<LPARAM>(this))) return false;
  if (!GetDlgItem(hwnd_, kTitleId) || !GetDlgItem(hwnd_, kVersionId) ||
      !GetDlgItem(hwnd_, kLaunchMenuPlayId) || !GetDlgItem(hwnd_, kLaunchMenuConfigureId)) {
    DestroyWindow(hwnd_);
    return false;
  }

  POINT cursor{};
  GetCursorPos(&cursor);
  MONITORINFO monitor{sizeof(monitor)};
  if (!GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor)) {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &monitor.rcWork, 0);
  }
  const RECT work = monitor.rcWork;
  const int center_x = (work.right + work.left) / 2;
  const int center_y = (work.bottom + work.top) / 2;
  SetWindowPos(hwnd_, nullptr, center_x, center_y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
  SetDpi(GetDpiForWindow(hwnd_));
  const int work_width = work.right - work.left;
  const int work_height = work.bottom - work.top;
  const int client_height = work_width < Px(kStackThreshold + 16, dpi_) ? 204 : kClientHeight;
  RECT frame{0, 0, Px(kClientWidth, dpi_), Px(client_height, dpi_)};
  AdjustWindowRectExForDpi(&frame, kWindowStyle, FALSE, kExStyle, dpi_);
  const int width = std::min(work_width, static_cast<int>(frame.right - frame.left));
  const int height = std::min(work_height, static_cast<int>(frame.bottom - frame.top));
  SetWindowPos(hwnd_, nullptr, center_x - width / 2, center_y - height / 2, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
  Layout();
  ShowWindow(hwnd_, show_command == SW_SHOWMAXIMIZED ? SW_SHOWNORMAL : show_command);
  return true;
}

bool LaunchMenu::Translate(MSG* message) const {
  if (!hwnd_ || !message || (message->hwnd != hwnd_ && !IsChild(hwnd_, message->hwnd))) return false;
  return IsDialogMessageW(hwnd_, message) != FALSE;
}

INT_PTR CALLBACK LaunchMenu::DialogProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  auto* menu = reinterpret_cast<LaunchMenu*>(GetWindowLongPtrW(hwnd, DWLP_USER));
  if (message == WM_INITDIALOG) {
    menu = reinterpret_cast<LaunchMenu*>(lparam);
    menu->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(menu));
  }
  if (menu == nullptr) return FALSE;
  if (message == WM_NCDESTROY) {
    SetWindowLongPtrW(hwnd, DWLP_USER, 0);
    menu->hwnd_ = nullptr;
    return FALSE;
  }
  return menu->OnMessage(message, wparam, lparam);
}

INT_PTR LaunchMenu::OnMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_INITDIALOG: {
      SetWindowTextW(hwnd_, L"Elixir");
      const auto child = [this](int id, const wchar_t* cls, const wchar_t* text, DWORD style) {
        return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, hwnd_,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
      };
      child(kTitleId, L"STATIC", L"Elixir", SS_LEFT | SS_NOPREFIX);
      child(kVersionId, L"STATIC", L"Version " ELIXIR_VERSION, SS_LEFT | SS_NOPREFIX);
      child(kLaunchMenuPlayId, L"BUTTON", L"Launch Roblox", WS_TABSTOP | WS_GROUP | BS_DEFPUSHBUTTON);
      child(kLaunchMenuConfigureId, L"BUTTON", L"Configure settings", WS_TABSTOP | BS_PUSHBUTTON);
      SendMessageW(hwnd_, DM_SETDEFID, kLaunchMenuPlayId, 0);
      SetAppearance();
      return TRUE;
    }
    case WM_SIZE: Layout(); return TRUE;
    case WM_DPICHANGED: {
      SetDpi(HIWORD(wparam));
      const auto& r = *reinterpret_cast<const RECT*>(lparam);
      SetWindowPos(hwnd_, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
      Layout();
      return TRUE;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:
      SetAppearance();
      RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
      return FALSE;
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      SetTextColor(dc, high_contrast_ ? GetSysColor(COLOR_WINDOWTEXT) :
          GetDlgCtrlID(reinterpret_cast<HWND>(lparam)) == kVersionId ? RGB(175, 175, 175) : kForeground);
      SetBkMode(dc, TRANSPARENT);
      return reinterpret_cast<INT_PTR>(high_contrast_ ? GetSysColorBrush(COLOR_WINDOW) : background_);
    }
    case WM_ERASEBKGND: return TRUE;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(hwnd_, &paint);
      DrawBackground(dc);
      EndPaint(hwnd_, &paint);
      return TRUE;
    }
    case WM_PRINTCLIENT: DrawBackground(reinterpret_cast<HDC>(wparam)); return TRUE;
    case WM_NOTIFY: {
      const auto* header = reinterpret_cast<NMHDR*>(lparam);
      if (!high_contrast_ && header->code == NM_CUSTOMDRAW &&
          (header->idFrom == kLaunchMenuPlayId || header->idFrom == kLaunchMenuConfigureId)) {
        const auto& draw = *reinterpret_cast<const NMCUSTOMDRAW*>(lparam);
        if (draw.dwDrawStage == CDDS_PREPAINT && PrepareIcons()) {
          DrawAction(draw);
          SetWindowLongPtrW(hwnd_, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
          return TRUE;
        }
      }
      return FALSE;
    }
    case WM_CLOSE:
    case WM_COMMAND: {
      LaunchMenuAction action;
      const int id = LOWORD(wparam);
      if (message == WM_CLOSE || id == IDCANCEL) action = LaunchMenuAction::kClose;
      else if (id == kLaunchMenuPlayId && HIWORD(wparam) == BN_CLICKED) action = LaunchMenuAction::kLaunch;
      else if (id == kLaunchMenuConfigureId && HIWORD(wparam) == BN_CLICKED) action = LaunchMenuAction::kConfigure;
      else return FALSE;
      auto callback = on_action_;
      callback(action);
      return TRUE;
    }
  }
  return FALSE;
}

void LaunchMenu::SetAppearance() {
  HIGHCONTRASTW contrast{sizeof(contrast)};
  high_contrast_ = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
                   (contrast.dwFlags & HCF_HIGHCONTRASTON);
  const BOOL dark = !high_contrast_;
  if (FAILED(DwmSetWindowAttribute(hwnd_, 20 , &dark, sizeof(dark)))) {
    DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));
  }
}

void LaunchMenu::SetDpi(UINT dpi) {
  dpi_ = dpi ? dpi : 96;
  SetFont(title_font_, 25, FW_SEMIBOLD, dpi_, L"Segoe UI");
  SetFont(version_font_, 12, FW_NORMAL, dpi_, L"Segoe UI");
  SetFont(button_font_, 14, FW_NORMAL, dpi_, L"Segoe UI");
  SendDlgItemMessageW(hwnd_, kTitleId, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
  SendDlgItemMessageW(hwnd_, kVersionId, WM_SETFONT, reinterpret_cast<WPARAM>(version_font_), TRUE);
  for (int id : {kLaunchMenuPlayId, kLaunchMenuConfigureId}) {
    SendDlgItemMessageW(hwnd_, id, WM_SETFONT, reinterpret_cast<WPARAM>(button_font_), TRUE);
  }
  SetIcon(brand_icon_, Px(60, dpi_));
  SetIcon(small_icon_, GetSystemMetricsForDpi(SM_CXSMICON, dpi_));
  SetIcon(large_icon_, GetSystemMetricsForDpi(SM_CXICON, dpi_));
  SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon_));
  SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon_));
}

void LaunchMenu::Layout() {
  RECT client{};
  GetClientRect(hwnd_, &client);
  const int width = MulDiv(client.right, 96, dpi_);
  const int height = MulDiv(client.bottom, 96, dpi_);
  const bool stacked = width < kStackThreshold;
  const int group_width = 161;
  const int brand_left = stacked ? (width - group_width) / 2 : (width * 44 / 100 - group_width) / 2;
  const int brand_top = stacked ? 10 : (height - 60) / 2;
  const auto px = [this](int d) { return Px(d, dpi_); };
  brand_bounds_ = {px(brand_left), px(brand_top), px(brand_left + 60), px(brand_top + 60)};
  const auto move = [&](int id, int x, int y, int w, int h) {
    SetWindowPos(GetDlgItem(hwnd_, id), nullptr, px(x), px(y), px(w), px(h), SWP_NOZORDER | SWP_NOACTIVATE);
  };
  move(kTitleId, brand_left + 72, brand_top + 5, 92, 33);
  move(kVersionId, brand_left + 73, brand_top + 39, 98, 20);
  const int left = stacked ? 16 : width * 44 / 100;
  const int row_top = stacked ? 82 : (height - 104) / 2;
  const int row_width = std::max(0, width - left - 16);
  move(kLaunchMenuPlayId, left, row_top, row_width, 48);
  move(kLaunchMenuConfigureId, left, row_top + 56, row_width, 48);
  InvalidateRect(hwnd_, nullptr, TRUE);
}

void LaunchMenu::DrawBackground(HDC dc) const {
  RECT client{};
  GetClientRect(hwnd_, &client);
  FillRect(dc, &client, high_contrast_ ? GetSysColorBrush(COLOR_WINDOW) : background_);
  if (brand_icon_) DrawIconEx(dc, brand_bounds_.left, brand_bounds_.top, brand_icon_,
                              brand_bounds_.right - brand_bounds_.left, brand_bounds_.bottom - brand_bounds_.top,
                              0, nullptr, DI_NORMAL);
}

bool LaunchMenu::PrepareIcons() {
  if (!icon_factory_) {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, icon_factory_.GetAddressOf()))) return false;
    const auto build = [this](wchar_t glyph, ID2D1PathGeometry** result) {
      return lucide::BuildGeometry(icon_factory_.Get(), lucide::IconFor(glyph).svg, result);
    };
    if (!build(glyph::kChevronRight, chevron_.GetAddressOf())) return false;
    const auto style = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                   D2D1_LINE_JOIN_ROUND);
    icon_factory_->CreateStrokeStyle(style, nullptr, 0, icon_stroke_.GetAddressOf());
  }
  if (!icon_stroke_) return false;
  if (!icon_target_) {
    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f);
    if (FAILED(icon_factory_->CreateDCRenderTarget(&properties, icon_target_.GetAddressOf()))) return false;
    if (FAILED(icon_target_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), icon_brush_.GetAddressOf()))) {
      icon_target_.Reset();
      return false;
    }
  }
  return true;
}

void LaunchMenu::DrawIconGeometry(HDC dc, ID2D1Geometry* geometry, int left, int center_y, int size,
                                  COLORREF foreground, COLORREF background) {
  RECT square{left, center_y - size / 2, left + size, center_y - size / 2 + size};
  if (!icon_target_ || FAILED(icon_target_->BindDC(dc, &square))) return;
  icon_target_->BeginDraw();
  icon_target_->Clear(D2DColor(background));
  const float scale = static_cast<float>(size) / 24.0f;
  icon_target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale));
  icon_brush_->SetColor(D2DColor(foreground));
  icon_target_->DrawGeometry(geometry, icon_brush_.Get(), 2.0f, icon_stroke_.Get());
  if (FAILED(icon_target_->EndDraw())) {
    icon_brush_.Reset();
    icon_target_.Reset();
  }
}

void LaunchMenu::DrawAction(const NMCUSTOMDRAW& draw) {
  const int saved = SaveDC(draw.hdc);
  RECT rect = draw.rc;
  FillRect(draw.hdc, &rect, background_);
  const bool pressed = (draw.uItemState & CDIS_SELECTED) != 0;
  const bool hot = (draw.uItemState & CDIS_HOT) != 0;
  SelectObject(draw.hdc, GetStockObject(DC_PEN));
  SelectObject(draw.hdc, GetStockObject(DC_BRUSH));
  SetDCPenColor(draw.hdc, RGB(64, 64, 64));
  const COLORREF fill = pressed ? RGB(41, 41, 41) : hot ? RGB(54, 54, 54) : RGB(45, 45, 45);
  SetDCBrushColor(draw.hdc, fill);
  RoundRect(draw.hdc, rect.left, rect.top, rect.right, rect.bottom, Px(10, dpi_), Px(10, dpi_));
  SetBkMode(draw.hdc, TRANSPARENT);
  SetTextColor(draw.hdc, kForeground);
  const int center_y = (rect.top + rect.bottom) / 2;
  RECT text = rect;
  text.left = rect.left + Px(16, dpi_);
  text.right -= Px(12, dpi_);
  wchar_t label[48]{};
  GetWindowTextW(draw.hdr.hwndFrom, label, 48);
  SelectObject(draw.hdc, button_font_);
  SIZE label_size{};
  const int arrow_size = Px(16, dpi_);
  const int arrow_left = text.right - arrow_size;
  if (GetTextExtentPoint32W(draw.hdc, label, lstrlenW(label), &label_size) &&
      text.left + label_size.cx + Px(12, dpi_) <= arrow_left) {
    DrawIconGeometry(draw.hdc, chevron_.Get(), arrow_left, center_y, arrow_size, RGB(184, 184, 184), fill);
    text.right = arrow_left - Px(8, dpi_);
  }
  DrawTextW(draw.hdc, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
  if ((draw.uItemState & CDIS_FOCUS) && !(SendMessageW(draw.hdr.hwndFrom, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
    InflateRect(&rect, -Px(4, dpi_), -Px(4, dpi_));
    DrawFocusRect(draw.hdc, &rect);
  }
  RestoreDC(draw.hdc, saved);
}

}
