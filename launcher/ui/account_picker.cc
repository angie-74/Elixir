// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "ui/account_picker.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "ui/theme.h"

namespace elixir::launcher::ui {

namespace {

constexpr UINT kActivateMessage = WM_APP + 1;
constexpr UINT_PTR kFrameTimer = 1;
constexpr UINT_PTR kOwnerSubclassId = 0xE11C;
constexpr DWORD kStyle = WS_POPUP;
constexpr DWORD kExStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED;
constexpr float kWheelStep = 64.0f;
constexpr float kLeastAnchorWidth = 200.0f;

double NowMs() {
  static const double frequency = [] {
    LARGE_INTEGER value{};
    QueryPerformanceFrequency(&value);
    return static_cast<double>(value.QuadPart);
  }();
  LARGE_INTEGER counter{};
  QueryPerformanceCounter(&counter);
  return static_cast<double>(counter.QuadPart) * 1000.0 / frequency;
}

std::wstring RowId(int row) { return std::wstring(kAccountPickerControl) + L"#" + std::to_wstring(row); }

}

AccountPicker::~AccountPicker() { Close(); }

bool AccountPicker::Show(Request request) {
  Close();
  if (request.owner == nullptr || request.anchor == nullptr || !request.on_open) return false;
  if (!renderer_ready_) {
    renderer_ok_ = renderer_.Initialize();
    renderer_ready_ = true;
  }
  if (!renderer_ok_) return false;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &AccountPicker::WindowProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kAccountPickerWindowClass;
    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    registered = true;
  }
  renderer_.SyncLanguage();
  owner_ = request.owner;
  anchor_ = request.anchor;
  bounds_ = request.bounds;
  rows_ = std::move(request.rows);
  on_open_ = std::move(request.on_open);
  on_remove_ = std::move(request.on_remove);
  motion_ = request.motion;
  offscreen_ = request.offscreen;
  dpi_ = GetDpiForWindow(owner_);
  if (dpi_ == 0) dpi_ = 96;
  state_ = UiState{};
  state_.control_accent_rgb = request.accent;
  state_.focus_visible = request.keyboard;
  animator_ = Animator{};
  focus_ = std::clamp(request.focus, 0, static_cast<int>(rows_.size()));
  state_.focused = RowId(focus_);
  const RECT place = Placement();
  scroll_ = AccountListReveal(focus_, rows_.size(), 0.0f, visible_height_);
  appearing_ = motion_;
  moving_ = false;
  opened_at_ = NowMs();
  hwnd_ = CreateWindowExW(kExStyle, kAccountPickerWindowClass, L"", kStyle, place.left, place.top + SlideOffset(),
                          place.right - place.left, place.bottom - place.top, owner_, nullptr,
                          GetModuleHandleW(nullptr), this);
  if (hwnd_ == nullptr) {
    Close();
    return false;
  }
  const BOOL dark = TRUE;
  if (FAILED(DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark)))) {
    DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));
  }
  const int corners = 3;
  DwmSetWindowAttribute(hwnd_, 33, &corners, sizeof(corners));
  const COLORREF border = 0xFFFFFFFE;
  DwmSetWindowAttribute(hwnd_, 34, &border, sizeof(border));
  SetLayeredWindowAttributes(hwnd_, 0, appearing_ ? 0 : 255, LWA_ALPHA);
  GetWindowRect(owner_, &owner_rect_);
  owner_visible_ = IsWindowVisible(owner_) != FALSE;
  watching_ = SetWindowSubclass(owner_, &AccountPicker::OwnerProc, kOwnerSubclassId,
                                reinterpret_cast<DWORD_PTR>(this)) != FALSE;
  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd_);
  UpdateTimer();
  NotifyFocus();
  return true;
}

void AccountPicker::SetRows(std::vector<AccountRow> rows) {
  if (hwnd_ == nullptr) return;
  rows_ = std::move(rows);
  focus_ = std::clamp(focus_, 0, static_cast<int>(rows_.size()));
  state_.focused = RowId(focus_);
  const RECT place = Placement();
  scroll_ = AccountListReveal(focus_, rows_.size(), scroll_, visible_height_);
  SetWindowPos(hwnd_, nullptr, place.left, place.top + SlideOffset(), place.right - place.left,
               place.bottom - place.top, SWP_NOZORDER | SWP_NOACTIVATE);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void AccountPicker::Close() {
  if (watching_ && owner_ != nullptr) RemoveWindowSubclass(owner_, &AccountPicker::OwnerProc, kOwnerSubclassId);
  watching_ = false;
  if (hwnd_ != nullptr) DestroyWindow(hwnd_);
  owner_ = nullptr;
  anchor_ = nullptr;
  on_open_ = nullptr;
  on_remove_ = nullptr;
  rows_.clear();
  records_.clear();
  pending_activation_.clear();
  appearing_ = false;
  moving_ = false;
}

double AccountPicker::OpenForMs() const { return hwnd_ != nullptr ? NowMs() - opened_at_ : 0.0; }

void AccountPicker::PaintNow() {
  if (hwnd_ != nullptr) RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

bool AccountPicker::Translate(const MSG* message) {
  if (hwnd_ == nullptr || message == nullptr) return false;
  if (message->hwnd != owner_ && !IsChild(owner_, message->hwnd)) return false;
  if (message->message == WM_MOUSEWHEEL) {
    if (AccountListHeight(rows_.size()) <= visible_height_) return false;
    ScrollBy(-static_cast<float>(GET_WHEEL_DELTA_WPARAM(message->wParam)) / WHEEL_DELTA * kWheelStep);
    return true;
  }
  if (message->message == WM_SYSKEYDOWN && (message->wParam == VK_UP || message->wParam == VK_DOWN)) {
    Close();
    return true;
  }
  if (message->message != WM_KEYDOWN) return false;
  const bool repeated = (message->lParam & (LPARAM{1} << 30)) != 0;
  const int last = static_cast<int>(rows_.size());
  switch (message->wParam) {
    case VK_DOWN: MoveFocus(std::min(focus_ + 1, last), true); return true;
    case VK_UP: MoveFocus(std::max(focus_ - 1, 0), true); return true;
    case VK_HOME: MoveFocus(0, true); return true;
    case VK_END: MoveFocus(last, true); return true;
    case VK_RETURN:
    case VK_SPACE:
      if (!repeated) Act(AccountListChoice{AccountListChoice::Kind::kOpen, focus_});
      return true;
    case VK_DELETE:
      if (!repeated && focus_ < last) Act(AccountListChoice{AccountListChoice::Kind::kRemove, focus_});
      return true;
    case VK_ESCAPE:
    case VK_F4: Close(); return true;
    case VK_TAB: Close(); return false;
    default: return false;
  }
}

bool AccountPicker::Activate(const std::wstring& id) {
  if (hwnd_ == nullptr) return false;
  const Interaction interaction{Interaction::Type::kActivate, id, 0.0, 0, {}};
  AccountListChoice choice;
  {
    Ui ui(&renderer_, &state_, nullptr, &interaction);
    choice = Build(ui);
  }
  if (choice.kind == AccountListChoice::Kind::kNone) return false;
  Act(choice);
  return true;
}

int AccountPicker::AccessibleFocusIndex() const {
  for (std::size_t i = 0; i < records_.size(); ++i) {
    if (records_[i].id == state_.focused) return static_cast<int>(i);
  }
  return -1;
}

void AccountPicker::AccessibleActivate(int index) {
  if (hwnd_ == nullptr || index < 0 || static_cast<std::size_t>(index) >= records_.size()) return;
  pending_activation_ = records_[static_cast<std::size_t>(index)].id;
  PostMessageW(hwnd_, kActivateMessage, 0, 0);
}

void AccountPicker::AccessibleFocus(int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= records_.size()) return;
  const std::wstring& id = records_[static_cast<std::size_t>(index)].id;
  for (int row = 0; row <= static_cast<int>(rows_.size()); ++row) {
    if (id == RowId(row)) {
      MoveFocus(row, true);
      return;
    }
  }
}

LRESULT CALLBACK AccountPicker::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  AccountPicker* self = nullptr;
  if (message == WM_NCCREATE) {
    self = static_cast<AccountPicker*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    self->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<AccountPicker*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (self == nullptr) return DefWindowProcW(hwnd, message, wparam, lparam);
  const LRESULT result = self->OnMessage(hwnd, message, wparam, lparam);
  if (message == WM_NCDESTROY) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    self->hwnd_ = nullptr;
    self->renderer_.DiscardTarget();
    self->timer_ = false;
    self->tracking_mouse_ = false;
  }
  return result;
}

LRESULT CALLBACK AccountPicker::OwnerProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id,
                                          DWORD_PTR data) {
  auto* self = reinterpret_cast<AccountPicker*>(data);
  if (message == WM_NCDESTROY) {
    RemoveWindowSubclass(hwnd, &AccountPicker::OwnerProc, id);
    if (self != nullptr) {
      self->watching_ = false;
      self->Close();
    }
  } else if (self != nullptr && self->OwnerDismisses(message, wparam, lparam)) {
    self->Close();
  }
  return DefSubclassProc(hwnd, message, wparam, lparam);
}

bool AccountPicker::OwnerDismisses(UINT message, WPARAM wparam, LPARAM lparam) const {
  switch (message) {
    case WM_ACTIVATE: return LOWORD(wparam) == WA_INACTIVE;
    case WM_ACTIVATEAPP: return wparam == FALSE;
    case WM_PARENTNOTIFY: {
      const UINT event = LOWORD(wparam);
      if (event != WM_LBUTTONDOWN && event != WM_RBUTTONDOWN && event != WM_MBUTTONDOWN && event != WM_XBUTTONDOWN) {
        return false;
      }
      const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      return ChildWindowFromPointEx(owner_, point, CWP_SKIPINVISIBLE) != anchor_;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
    case WM_ENTERSIZEMOVE:
    case WM_SYSCOMMAND:
    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
    case WM_CLOSE:
      return true;
    case WM_WINDOWPOSCHANGED: {
      RECT now{};
      GetWindowRect(owner_, &now);
      return !EqualRect(&now, &owner_rect_) || (owner_visible_ && !IsWindowVisible(owner_));
    }
    default:
      return false;
  }
}

LRESULT AccountPicker::OnMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
      Paint();
      return 0;
    case WM_SIZE:
      renderer_.Resize(LOWORD(lparam), HIWORD(lparam));
      return 0;
    case WM_DPICHANGED:
      return 0;
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_TIMER:
      if (wparam == kFrameTimer) Advance();
      return 0;
    case WM_MOUSEMOVE: {
      const ControlRecord* hit = HitTest(POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
      SetHovered(hit != nullptr ? hit->id : std::wstring());
      if (!tracking_mouse_) {
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
        tracking_mouse_ = TrackMouseEvent(&track) != FALSE;
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      tracking_mouse_ = false;
      if (GetCapture() != hwnd) SetHovered(std::wstring());
      return 0;
    case WM_LBUTTONDOWN: {
      const ControlRecord* hit = HitTest(POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
      if (hit == nullptr) return 0;
      state_.pressed = hit->id;
      state_.hovered = hit->id;
      state_.focus_visible = false;
      SetCapture(hwnd);
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_LBUTTONUP: {
      const std::wstring pressed = std::move(state_.pressed);
      state_.pressed.clear();
      if (GetCapture() == hwnd) ReleaseCapture();
      InvalidateRect(hwnd, nullptr, FALSE);
      const ControlRecord* hit = HitTest(POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
      if (!pressed.empty() && hit != nullptr && hit->id == pressed) Activate(pressed);
      return 0;
    }
    case WM_CAPTURECHANGED:
      if (!state_.pressed.empty()) {
        state_.pressed.clear();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    case WM_MOUSEWHEEL:
      ScrollBy(-static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA * kWheelStep);
      return 0;
    case kActivateMessage: {
      const std::wstring id = std::move(pending_activation_);
      pending_activation_.clear();
      if (!id.empty()) Activate(id);
      return 0;
    }
    case WM_GETOBJECT:
      if (static_cast<LONG>(lparam) == static_cast<LONG>(OBJID_CLIENT)) {
        if (accessible_ == nullptr) accessible_ = new AccessibleRoot(this);
        return LresultFromObject(IID_IAccessible, wparam, accessible_);
      }
      break;
    case WM_DESTROY:
      KillTimer(hwnd, kFrameTimer);
      if (accessible_ != nullptr) {
        accessible_->Detach();
        accessible_->Release();
        accessible_ = nullptr;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

RECT AccountPicker::Placement() {
  RECT anchor{};
  GetWindowRect(anchor_, &anchor);
  RECT work = bounds_;
  if (IsRectEmpty(&work)) {
    MONITORINFO monitor{sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &monitor)) {
      work = monitor.rcWork;
    } else {
      SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    }
  }
  const float s = scale();
  const auto px = [s](float dips) { return static_cast<int>(std::lround(dips * s)); };
  Ui ui(&renderer_, &state_, nullptr, nullptr);
  const int anchor_px = static_cast<int>(anchor.right - anchor.left);
  const float anchor_width = static_cast<float>(anchor_px) / s;
  const bool as_wide_as_anchor = anchor_width >= kLeastAnchorWidth && anchor_width <= kAccountListMostWidth;
  const float least_width = as_wide_as_anchor ? anchor_width : kAccountListLeastWidth;
  const int width = std::max(px(AccountListWidth(ui, rows_, least_width)), as_wide_as_anchor ? anchor_px : 0);
  const int wanted = px(AccountListHeight(rows_.size()));
  const int least = px(AccountListHeight(1));
  const int gap = px(kAccountListGap);
  const int room_below = static_cast<int>(work.bottom) - gap - (static_cast<int>(anchor.bottom) + gap);
  const int room_above = static_cast<int>(anchor.top) - gap - (static_cast<int>(work.top) + gap);
  bool below = true;
  int height = wanted;
  if (wanted > room_below) {
    below = wanted > room_above && room_above <= room_below;
    height = std::min(wanted, std::max(below ? room_below : room_above, least));
  }
  int x = std::min(static_cast<int>(anchor.right) - width, static_cast<int>(work.right) - gap - width);
  x = std::max(x, static_cast<int>(work.left) + gap);
  int y = below ? static_cast<int>(anchor.bottom) + gap : static_cast<int>(anchor.top) - gap - height;
  placement_ = RECT{x, y, x + width, y + height};
  if (offscreen_) {
    x = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) + 64;
    y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  }
  visible_height_ = static_cast<float>(height) / s;
  origin_ = POINT{x, y};
  slide_from_ = (below ? -1 : 1) * px(theme::kMenuSlide);
  return RECT{x, y, x + width, y + height};
}

int AccountPicker::SlideOffset() const {
  if (!appearing_) return 0;
  const float t = std::clamp(static_cast<float>((NowMs() - opened_at_) / theme::kPopupMs), 0.0f, 1.0f);
  const float rest = 1.0f - t;
  return static_cast<int>(std::lround(rest * rest * rest * static_cast<float>(slide_from_)));
}

void AccountPicker::Paint() {
  PAINTSTRUCT paint{};
  BeginPaint(hwnd_, &paint);
  if (renderer_ok_ && renderer_.EnsureTarget(hwnd_, dpi_)) {
    animator_.BeginFrame(NowMs(), motion_);
    state_.animator = &animator_;
    ID2D1RenderTarget* target = renderer_.target();
    target->BeginDraw();
    target->SetTransform(D2D1::Matrix3x2F::Identity());
    target->Clear(theme::kDialog);
    records_.clear();
    Ui ui(&renderer_, &state_, &records_, nullptr);
    Build(ui);
    ui.Flush();
    if (target->EndDraw() == D2DERR_RECREATE_TARGET) {
      renderer_.DiscardTarget();
      InvalidateRect(hwnd_, nullptr, FALSE);
    }
    state_.animator = nullptr;
    moving_ = animator_.EndFrame();
  }
  EndPaint(hwnd_, &paint);
  UpdateTimer();
}

AccountListChoice AccountPicker::Build(Ui& ui) {
  RECT client{};
  GetClientRect(hwnd_, &client);
  const D2D1_RECT_F panel = D2D1::RectF(0.0f, 0.0f, static_cast<float>(client.right) / scale(),
                                        static_cast<float>(client.bottom) / scale());
  return BuildAccountList(ui, kAccountPickerControl, panel, rows_, scroll_);
}

void AccountPicker::Act(const AccountListChoice& choice) {
  if (choice.kind == AccountListChoice::Kind::kOpen) {
    const std::function<void(int)> on_open = on_open_;
    if (on_open) on_open(choice.row);
  } else if (choice.kind == AccountListChoice::Kind::kRemove) {
    const std::function<void(int)> on_remove = on_remove_;
    if (on_remove) on_remove(choice.row);
  }
}

const ControlRecord* AccountPicker::HitTest(POINT client) const {
  const float x = static_cast<float>(client.x) / scale();
  const float y = static_cast<float>(client.y) / scale();
  for (auto it = records_.rbegin(); it != records_.rend(); ++it) {
    if (it->kind == ControlKind::kStatic || it->offscreen || !it->enabled) continue;
    if (x >= it->rect.left && x < it->rect.right && y >= it->rect.top && y < it->rect.bottom) return &*it;
  }
  return nullptr;
}

void AccountPicker::SetHovered(const std::wstring& id) {
  if (state_.hovered == id) return;
  state_.hovered = id;
  if (hwnd_ != nullptr) InvalidateRect(hwnd_, nullptr, FALSE);
}

void AccountPicker::MoveFocus(int row, bool visible) {
  if (hwnd_ == nullptr) return;
  focus_ = std::clamp(row, 0, static_cast<int>(rows_.size()));
  state_.focused = RowId(focus_);
  state_.focus_visible = visible;
  scroll_ = AccountListReveal(focus_, rows_.size(), scroll_, visible_height_);
  PaintNow();
  NotifyFocus();
}

void AccountPicker::NotifyFocus() {
  if (hwnd_ == nullptr) return;
  const int index = AccessibleFocusIndex();
  if (index >= 0) NotifyWinEvent(EVENT_OBJECT_FOCUS, hwnd_, OBJID_CLIENT, index + 1);
}

void AccountPicker::ScrollBy(float delta) {
  if (hwnd_ == nullptr) return;
  const float max_scroll = std::max(0.0f, AccountListHeight(rows_.size()) - visible_height_);
  const float scroll = std::clamp(scroll_ + delta, 0.0f, max_scroll);
  if (scroll == scroll_) return;
  scroll_ = scroll;
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void AccountPicker::Advance() {
  if (hwnd_ == nullptr) return;
  if (appearing_) {
    const float t = std::clamp(static_cast<float>((NowMs() - opened_at_) / theme::kPopupMs), 0.0f, 1.0f);
    const float rest = 1.0f - t;
    const float shown = 1.0f - rest * rest * rest;
    const int offset = SlideOffset();
    appearing_ = t < 1.0f;
    SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(std::lround(shown * 255.0f)), LWA_ALPHA);
    SetWindowPos(hwnd_, nullptr, origin_.x, origin_.y + (appearing_ ? offset : 0), 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
  if (moving_) InvalidateRect(hwnd_, nullptr, FALSE);
  UpdateTimer();
}

void AccountPicker::UpdateTimer() {
  if (hwnd_ == nullptr) return;
  const bool wanted = appearing_ || moving_;
  if (wanted == timer_) return;
  if (wanted) {
    SetTimer(hwnd_, kFrameTimer, USER_TIMER_MINIMUM, nullptr);
  } else {
    KillTimer(hwnd_, kFrameTimer);
  }
  timer_ = wanted;
}

}
