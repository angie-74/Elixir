// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <algorithm>

#include "core/instances.h"
#include "core/process.h"
#include "core/roblox_links.h"
#include "core/util.h"
#include "ui/app.h"

namespace elixir::launcher::ui {

void App::PostInstanceMessage(InstanceMessage message) {
  PostToWindow(hwnd_, this, [this, message = std::move(message)]() mutable {
    if (message.kind == InstanceMessageKind::kActivate && IsWindowVisible(launch_menu_.hwnd())) {
      HWND menu = launch_menu_.hwnd();
      ShowWindow(menu, IsIconic(menu) ? SW_RESTORE : SW_SHOW);
      SetForegroundWindow(menu);
      return;
    }
    BringToFront();
    if (message.kind == InstanceMessageKind::kLink) OnLinkRequest(std::move(message.payload));
    if (message.kind == InstanceMessageKind::kOpenPage && !message.payload.empty()) {
      Page target{};
      if (PageFromKey(message.payload, &target)) Navigate(target);
    }
  });
}

void App::OpenLink(LinkRequest request) {
  PostToWindow(hwnd_, this, [this, request = std::move(request)]() mutable {
    switch (request.kind) {
      case LinkRequestKind::kJoin:
        OnLinkRequest(std::move(request.join));
        break;
      case LinkRequestKind::kOpenApp:
        break;
      case LinkRequestKind::kInvalid:
        ShowToast(L"This Roblox link can’t be opened in Elixir.");
        break;
    }
  });
}

void App::BringToFront() {
  if (hwnd_ == nullptr) return;
  if (launch_menu_.hwnd()) ShowWindow(launch_menu_.hwnd(), SW_HIDE);
  play_from_menu_ = false;
  StartLauncherServices();
  ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
  SetForegroundWindow(hwnd_);
}

void App::OnLinkRequest(std::string launch_request) {
  DropPendingJoin();
  pending_join_ = PendingJoin{std::move(launch_request), GetTickCount64()};
  SecureZeroMemory(launch_request.data(), launch_request.capacity());
  if (runtime_.running) {
    if (runtime_.stopping) {
      DropPendingJoin();
      ShowToast(L"Roblox is closing. Open the link again in a moment.");
      return;
    }
    if (runtime_.in_game) {
      Overlay overlay;
      overlay.id = L"join.leave";
      overlay.status = 1;
      overlay.title = L"Leave this experience?";
      overlay.body = L"Roblox will leave the experience you’re in and join the one you opened.";
      overlay.actions = {{L"join.confirm", L"Join", ButtonStyle::kStandard},
                         {L"overlay.close", L"Stay", ButtonStyle::kStandard}};
      ShowOverlay(std::move(overlay));
      return;
    }
    SendPendingJoin();
    return;
  }
  Play();
}

bool App::OnLinkOverlayAction(const std::wstring& id) {
  if (id != L"join.confirm") return false;
  SendPendingJoin();
  CloseOverlay();
  return true;
}

void App::SendPendingJoin() {
  if (!pending_join_ || !runtime_.running || !runtime_.connected || runtime_.stopping) return;
  PendingJoin join = std::move(*pending_join_);
  pending_join_.reset();
  if (GetTickCount64() - join.received > kPendingJoinMaxAgeMs) {
    SecureZeroMemory(join.request.data(), join.request.capacity());
    return;
  }
  const bool sent = SendRuntimeJoin(runtime_.ipc_pipe, join.request);
  SecureZeroMemory(join.request.data(), join.request.capacity());
  ShowToast(sent ? L"Joining the experience…" : L"Couldn’t join the experience. Open the link again.");
}

void App::DropPendingJoin() {
  if (!pending_join_) return;
  SecureZeroMemory(pending_join_->request.data(), pending_join_->request.capacity());
  pending_join_.reset();
}

void App::OnJoinEvent(const std::string& state) {
  if (state == "in-game") {
    runtime_.in_game = true;
  } else if (state == "left") {
    runtime_.in_game = false;
  } else if (state == "waiting-for-sign-in") {
    ShowToast(L"Sign in to Roblox to join the experience.");
  } else if (state == "busy") {
    ShowToast(L"Roblox is busy. Open the link again in a moment.");
  } else if (state == "rejected") {
    ShowToast(L"Roblox couldn’t join that experience.");
  }
}

int App::FreeWindowNumber() {
  for (auto it = extra_windows_.begin(); it != extra_windows_.end();) {
    if (WaitForSingleObject(it->process, 0) == WAIT_OBJECT_0) {
      CloseHandle(it->process);
      it = extra_windows_.erase(it);
    } else {
      ++it;
    }
  }
  for (int number = 2; number <= kMaxRobloxWindows; ++number) {
    const bool ours = std::any_of(extra_windows_.begin(), extra_windows_.end(),
                                  [number](const ExtraWindow& window) { return window.number == number; });
    if (!ours && !DataFolderInUse(InstancePaths(paths_, number).data_root)) return number;
  }
  return 0;
}

void App::OpenAnotherWindow() {
  if (!runtime_.running || runtime_.stopping || opening_window_) return;
  const int number = FreeWindowNumber();
  if (number == 0) {
    ShowToast(L"Elixir opens up to " + std::to_wstring(kMaxRobloxWindows) + L" Roblox instances.");
    return;
  }
  opening_window_ = true;
  Invalidate();
  const LauncherConfig config = config_;
  const LauncherPaths paths = paths_;
  RunInBackground([this, config, paths, number]() -> std::function<void()> {
    std::wstring error;
    const bool prepared = PrepareInstance(config, paths, number, &error);
    return [this, config, paths, number, prepared, error] {
      opening_window_ = false;
      if (!prepared) {
        ShowToast(L"Couldn’t open another Roblox instance: " + error + L".");
        return;
      }
      const RuntimeLaunch launch = LaunchInstance(config, paths, number);
      if (!launch.ok) {
        ShowToast(launch.error.empty() ? std::wstring(L"Couldn’t open another Roblox instance.")
                                       : L"Couldn’t open another Roblox window. " + launch.error);
        return;
      }
      extra_windows_.push_back(ExtraWindow{number, launch.process});
      ShowToast(L"Opening Roblox instance " + std::to_wstring(number) + L"…");
    };
  });
}

}
