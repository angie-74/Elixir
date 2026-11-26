// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "ui/accessibility.h"

#include <cmath>
#include <string>

namespace elixir::launcher::ui {

namespace {

constexpr HRESULT kDisconnected = static_cast<HRESULT>(0x80040201L);

HRESULT ReturnString(const std::wstring& text, BSTR* out) {
  if (out == nullptr) return E_POINTER;
  *out = nullptr;
  if (text.empty()) return S_FALSE;
  *out = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
  return *out != nullptr ? S_OK : E_OUTOFMEMORY;
}

long RoleOf(ControlKind kind) {
  switch (kind) {
    case ControlKind::kButton: return ROLE_SYSTEM_PUSHBUTTON;
    case ControlKind::kComboBox: return ROLE_SYSTEM_COMBOBOX;
    case ControlKind::kNav: return ROLE_SYSTEM_PAGETAB;
    case ControlKind::kToggle: return ROLE_SYSTEM_CHECKBUTTON;
    case ControlKind::kSegment: return ROLE_SYSTEM_RADIOBUTTON;
    case ControlKind::kSlider: return ROLE_SYSTEM_SLIDER;
    case ControlKind::kTextField: return ROLE_SYSTEM_TEXT;
    case ControlKind::kListItem: return ROLE_SYSTEM_LISTITEM;
    case ControlKind::kRadio: return ROLE_SYSTEM_RADIOBUTTON;
    case ControlKind::kLink: return ROLE_SYSTEM_LINK;
    case ControlKind::kStatic: return ROLE_SYSTEM_STATICTEXT;
    case ControlKind::kTextView: return ROLE_SYSTEM_TEXT;
  }
  return ROLE_SYSTEM_CLIENT;
}

const wchar_t* DefaultActionOf(ControlKind kind) {
  switch (kind) {
    case ControlKind::kButton: return L"Press";
    case ControlKind::kNav: return L"Switch";
    case ControlKind::kToggle: return L"Toggle";
    case ControlKind::kSegment:
    case ControlKind::kRadio:
    case ControlKind::kListItem: return L"Select";
    case ControlKind::kTextField: return L"Edit";
    case ControlKind::kLink: return L"Jump";
    default: return L"";
  }
}

}

AccessibleRoot::AccessibleRoot(AccessibilityHost* host) : host_(host) {}

HRESULT AccessibleRoot::QueryInterface(REFIID riid, void** object) {
  if (object == nullptr) return E_POINTER;
  if (riid == IID_IUnknown || riid == IID_IDispatch || riid == IID_IAccessible) {
    *object = static_cast<IAccessible*>(this);
    AddRef();
    return S_OK;
  }
  *object = nullptr;
  return E_NOINTERFACE;
}

ULONG AccessibleRoot::AddRef() { return ++references_; }

ULONG AccessibleRoot::Release() {
  const ULONG remaining = --references_;
  if (remaining == 0) delete this;
  return remaining;
}

HRESULT AccessibleRoot::GetTypeInfoCount(UINT* count) {
  if (count == nullptr) return E_POINTER;
  *count = 0;
  return S_OK;
}
HRESULT AccessibleRoot::GetTypeInfo(UINT, LCID, ITypeInfo**) { return E_NOTIMPL; }
HRESULT AccessibleRoot::GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) { return E_NOTIMPL; }
HRESULT AccessibleRoot::Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) {
  return E_NOTIMPL;
}

long AccessibleRoot::Resolve(const VARIANT& child) const {
  if (host_ == nullptr || child.vt != VT_I4) return -1;
  if (child.lVal == CHILDID_SELF) return 0;
  const long count = static_cast<long>(host_->AccessibleControls().size());
  return child.lVal >= 1 && child.lVal <= count ? child.lVal : -1;
}

const ControlRecord* AccessibleRoot::Control(long id) const {
  if (host_ == nullptr || id <= 0) return nullptr;
  return &host_->AccessibleControls()[static_cast<std::size_t>(id - 1)];
}

HRESULT AccessibleRoot::get_accParent(IDispatch** parent) {
  if (parent == nullptr) return E_POINTER;
  *parent = nullptr;
  if (host_ == nullptr) return kDisconnected;
  IAccessible* window = nullptr;
  if (FAILED(CreateStdAccessibleObject(host_->AccessibleWindow(), OBJID_WINDOW, IID_IAccessible,
                                       reinterpret_cast<void**>(&window)))) {
    return S_FALSE;
  }
  const HRESULT hr = window->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(parent));
  window->Release();
  return hr;
}

HRESULT AccessibleRoot::get_accChildCount(long* count) {
  if (count == nullptr) return E_POINTER;
  *count = 0;
  if (host_ == nullptr) return kDisconnected;
  *count = static_cast<long>(host_->AccessibleControls().size());
  return S_OK;
}

HRESULT AccessibleRoot::get_accChild(VARIANT child, IDispatch** dispatch) {
  if (dispatch == nullptr) return E_POINTER;
  *dispatch = nullptr;
  return Resolve(child) > 0 ? S_FALSE : E_INVALIDARG;
}

HRESULT AccessibleRoot::get_accName(VARIANT child, BSTR* name) {
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  if (id == 0) return ReturnString(L"Elixir", name);
  return ReturnString(Control(id)->name, name);
}

HRESULT AccessibleRoot::get_accValue(VARIANT child, BSTR* value) {
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  if (id == 0) return ReturnString(L"", value);
  return ReturnString(Control(id)->value, value);
}

HRESULT AccessibleRoot::get_accDescription(VARIANT child, BSTR* description) {
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  if (id == 0) return ReturnString(L"Elixir launcher", description);
  return ReturnString(Control(id)->description, description);
}

HRESULT AccessibleRoot::get_accRole(VARIANT child, VARIANT* role) {
  if (role == nullptr) return E_POINTER;
  VariantInit(role);
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  role->vt = VT_I4;
  role->lVal = id == 0 ? ROLE_SYSTEM_CLIENT : RoleOf(Control(id)->kind);
  return S_OK;
}

HRESULT AccessibleRoot::get_accState(VARIANT child, VARIANT* state) {
  if (state == nullptr) return E_POINTER;
  VariantInit(state);
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  state->vt = VT_I4;
  if (id == 0) {
    state->lVal = STATE_SYSTEM_FOCUSABLE;
    if (GetFocus() == host_->AccessibleWindow() && host_->AccessibleFocusIndex() < 0) state->lVal |= STATE_SYSTEM_FOCUSED;
    return S_OK;
  }
  const ControlRecord* control = Control(id);
  long value = 0;
  if (control->kind == ControlKind::kStatic) {
    value |= STATE_SYSTEM_READONLY;
  } else if (control->focusable) {
    value |= STATE_SYSTEM_FOCUSABLE;
  }
  if (control->kind == ControlKind::kTextView) value |= STATE_SYSTEM_READONLY;
  if (!control->enabled) value |= STATE_SYSTEM_UNAVAILABLE;
  if (host_->AccessibleFocusIndex() == id - 1 && GetFocus() == host_->AccessibleWindow()) value |= STATE_SYSTEM_FOCUSED;
  switch (control->kind) {
    case ControlKind::kComboBox:
      value |= STATE_SYSTEM_HASPOPUP | (control->expanded ? STATE_SYSTEM_EXPANDED : STATE_SYSTEM_COLLAPSED);
      break;
    case ControlKind::kToggle:
    case ControlKind::kSegment:
    case ControlKind::kRadio:
      if (control->checked) value |= STATE_SYSTEM_CHECKED;
      break;
    case ControlKind::kNav:
    case ControlKind::kListItem:
      value |= STATE_SYSTEM_SELECTABLE;
      if (control->checked) value |= STATE_SYSTEM_SELECTED;
      break;
    case ControlKind::kLink: value |= STATE_SYSTEM_LINKED; break;
    default: break;
  }
  if (control->offscreen) value |= STATE_SYSTEM_OFFSCREEN;
  state->lVal = value;
  return S_OK;
}

HRESULT AccessibleRoot::get_accHelp(VARIANT, BSTR* help) {
  if (help != nullptr) *help = nullptr;
  return S_FALSE;
}

HRESULT AccessibleRoot::get_accHelpTopic(BSTR* file, VARIANT, long* topic) {
  if (file != nullptr) *file = nullptr;
  if (topic != nullptr) *topic = 0;
  return S_FALSE;
}

HRESULT AccessibleRoot::get_accKeyboardShortcut(VARIANT child, BSTR* shortcut) {
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  if (id == 0) return ReturnString(L"", shortcut);
  return ReturnString(Control(id)->shortcut, shortcut);
}

HRESULT AccessibleRoot::get_accFocus(VARIANT* child) {
  if (child == nullptr) return E_POINTER;
  VariantInit(child);
  if (host_ == nullptr) return kDisconnected;
  if (GetFocus() != host_->AccessibleWindow()) return S_FALSE;
  child->vt = VT_I4;
  const int index = host_->AccessibleFocusIndex();
  child->lVal = index >= 0 ? index + 1 : CHILDID_SELF;
  return S_OK;
}

HRESULT AccessibleRoot::get_accSelection(VARIANT* children) {
  if (children == nullptr) return E_POINTER;
  VariantInit(children);
  return S_FALSE;
}

HRESULT AccessibleRoot::get_accDefaultAction(VARIANT child, BSTR* action) {
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  if (id == 0) return ReturnString(L"", action);
  if (Control(id)->kind == ControlKind::kComboBox) return ReturnString(Control(id)->expanded ? L"Close" : L"Open", action);
  return ReturnString(DefaultActionOf(Control(id)->kind), action);
}

HRESULT AccessibleRoot::accSelect(long flags, VARIANT child) {
  const long id = Resolve(child);
  if (id <= 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  if (flags & SELFLAG_TAKEFOCUS) {
    host_->AccessibleFocus(static_cast<int>(id - 1));
    return S_OK;
  }
  return S_FALSE;
}

HRESULT AccessibleRoot::accLocation(long* left, long* top, long* width, long* height, VARIANT child) {
  if (left == nullptr || top == nullptr || width == nullptr || height == nullptr) return E_POINTER;
  *left = *top = *width = *height = 0;
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  HWND hwnd = host_->AccessibleWindow();
  RECT client{};
  GetClientRect(hwnd, &client);
  POINT origin{0, 0};
  ClientToScreen(hwnd, &origin);
  if (id == 0) {
    *left = origin.x;
    *top = origin.y;
    *width = client.right;
    *height = client.bottom;
    return S_OK;
  }
  const float scale = host_->AccessibleScale();
  const D2D1_RECT_F& rect = Control(id)->full_rect;
  *left = origin.x + std::lround(rect.left * scale);
  *top = origin.y + std::lround(rect.top * scale);
  *width = std::lround((rect.right - rect.left) * scale);
  *height = std::lround((rect.bottom - rect.top) * scale);
  return S_OK;
}

HRESULT AccessibleRoot::accNavigate(long direction, VARIANT start, VARIANT* end) {
  if (end == nullptr) return E_POINTER;
  VariantInit(end);
  const long id = Resolve(start);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  const long count = static_cast<long>(host_->AccessibleControls().size());
  long target = 0;
  switch (direction) {
    case NAVDIR_FIRSTCHILD: target = id == 0 && count > 0 ? 1 : 0; break;
    case NAVDIR_LASTCHILD: target = id == 0 && count > 0 ? count : 0; break;
    case NAVDIR_NEXT: case NAVDIR_DOWN: case NAVDIR_RIGHT: target = id > 0 && id < count ? id + 1 : 0; break;
    case NAVDIR_PREVIOUS: case NAVDIR_UP: case NAVDIR_LEFT: target = id > 1 ? id - 1 : 0; break;
    default: return E_INVALIDARG;
  }
  if (target == 0) return S_FALSE;
  end->vt = VT_I4;
  end->lVal = target;
  return S_OK;
}

HRESULT AccessibleRoot::accHitTest(long x, long y, VARIANT* child) {
  if (child == nullptr) return E_POINTER;
  VariantInit(child);
  if (host_ == nullptr) return kDisconnected;
  HWND hwnd = host_->AccessibleWindow();
  POINT point{x, y};
  ScreenToClient(hwnd, &point);
  RECT client{};
  GetClientRect(hwnd, &client);
  if (!PtInRect(&client, point)) return S_FALSE;
  const float scale = host_->AccessibleScale();
  const float dx = static_cast<float>(point.x) / scale;
  const float dy = static_cast<float>(point.y) / scale;
  const auto& controls = host_->AccessibleControls();
  child->vt = VT_I4;
  child->lVal = CHILDID_SELF;
  for (std::size_t i = controls.size(); i-- > 0;) {
    const D2D1_RECT_F& rect = controls[i].rect;
    if (!controls[i].offscreen && dx >= rect.left && dx < rect.right && dy >= rect.top && dy < rect.bottom) {
      child->lVal = static_cast<long>(i) + 1;
      break;
    }
  }
  return S_OK;
}

HRESULT AccessibleRoot::accDoDefaultAction(VARIANT child) {
  const long id = Resolve(child);
  if (id < 0) return host_ == nullptr ? kDisconnected : E_INVALIDARG;
  if (id == 0) return DISP_E_MEMBERNOTFOUND;
  const ControlRecord* control = Control(id);
  if (control->kind == ControlKind::kStatic || !control->enabled) return DISP_E_MEMBERNOTFOUND;
  host_->AccessibleActivate(static_cast<int>(id - 1));
  return S_OK;
}

HRESULT AccessibleRoot::put_accName(VARIANT, BSTR) { return E_NOTIMPL; }
HRESULT AccessibleRoot::put_accValue(VARIANT, BSTR) { return E_NOTIMPL; }

}
