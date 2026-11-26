// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_ACCESSIBILITY_H_
#define ELIXIR_LAUNCHER_UI_ACCESSIBILITY_H_

#include <windows.h>
#include <oleacc.h>

#include <atomic>
#include <vector>

#include "ui/ui.h"

namespace elixir::launcher::ui {

class AccessibilityHost {
 public:
  virtual ~AccessibilityHost() = default;
  virtual HWND AccessibleWindow() const = 0;
  virtual const std::vector<ControlRecord>& AccessibleControls() const = 0;
  virtual float AccessibleScale() const = 0;
  virtual int AccessibleFocusIndex() const = 0;
  virtual void AccessibleActivate(int index) = 0;
  virtual void AccessibleFocus(int index) = 0;
};

class AccessibleRoot final : public IAccessible {
 public:
  explicit AccessibleRoot(AccessibilityHost* host);
  void Detach() { host_ = nullptr; }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override;
  HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override;
  HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override;
  HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) override;
  HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** parent) override;
  HRESULT STDMETHODCALLTYPE get_accChildCount(long* count) override;
  HRESULT STDMETHODCALLTYPE get_accChild(VARIANT child, IDispatch** dispatch) override;
  HRESULT STDMETHODCALLTYPE get_accName(VARIANT child, BSTR* name) override;
  HRESULT STDMETHODCALLTYPE get_accValue(VARIANT child, BSTR* value) override;
  HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT child, BSTR* description) override;
  HRESULT STDMETHODCALLTYPE get_accRole(VARIANT child, VARIANT* role) override;
  HRESULT STDMETHODCALLTYPE get_accState(VARIANT child, VARIANT* state) override;
  HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT child, BSTR* help) override;
  HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* file, VARIANT child, long* topic) override;
  HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT child, BSTR* shortcut) override;
  HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* child) override;
  HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* children) override;
  HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT child, BSTR* action) override;
  HRESULT STDMETHODCALLTYPE accSelect(long flags, VARIANT child) override;
  HRESULT STDMETHODCALLTYPE accLocation(long* left, long* top, long* width, long* height, VARIANT child) override;
  HRESULT STDMETHODCALLTYPE accNavigate(long direction, VARIANT start, VARIANT* end) override;
  HRESULT STDMETHODCALLTYPE accHitTest(long x, long y, VARIANT* child) override;
  HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT child) override;
  HRESULT STDMETHODCALLTYPE put_accName(VARIANT child, BSTR name) override;
  HRESULT STDMETHODCALLTYPE put_accValue(VARIANT child, BSTR value) override;

 private:
  ~AccessibleRoot() = default;
  long Resolve(const VARIANT& child) const;
  const ControlRecord* Control(long id) const;

  std::atomic<ULONG> references_{1};
  AccessibilityHost* host_;
};

}

#endif
