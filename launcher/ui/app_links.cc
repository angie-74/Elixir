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

void App::RefreshInstanceAccounts() {
  instance_accounts_.clear();
  for (SavedAccount& account : ListSavedAccounts(paths_.accounts_dir)) {
    const AccountUse use = QueryAccountUse(account.id);
    instance_accounts_.push_back(InstanceAccountRow{std::move(account), use});
  }
}

void App::RefreshAccountPictures() {
  constexpr ULONGLONG kCheckEveryMs = 60000;
  const HttpGetFunction get = WebGet();
  if (!get || account_pictures_loading_ || instance_accounts_.empty()) return;
  const bool missing = std::any_of(instance_accounts_.begin(), instance_accounts_.end(),
                                   [](const InstanceAccountRow& row) { return row.account.picture_file.empty(); });
  const ULONGLONG now = GetTickCount64();
  if (!missing && account_pictures_checked_ != 0 && now - account_pictures_checked_ < kCheckEveryMs) return;
  account_pictures_loading_ = true;
  account_pictures_checked_ = now;
  std::vector<SavedAccount> accounts;
  for (const InstanceAccountRow& row : instance_accounts_) accounts.push_back(row.account);
  const std::wstring folder = paths_.accounts_dir;
  RunInBackground([this, folder, accounts, get]() -> std::function<void()> {
    const int changed = elixir::launcher::RefreshAccountPictures(folder, accounts, get);
    return [this, changed] {
      account_pictures_loading_ = false;
      if (changed > 0 && MenuOpen(kInstanceAccountControl)) {
        RefreshInstanceAccounts();
        Invalidate();
      }
    };
  });
}

void App::ChooseInstanceAccount() {
  if (!runtime_.running || runtime_.stopping || opening_window_) return;
  if (MenuOpen(kInstanceAccountControl)) {
    CloseMenu();
    return;
  }
  RefreshInstanceAccounts();
  if (instance_accounts_.empty()) {
    OpenAnotherWindow();
    return;
  }
  SetMenu(kInstanceAccountControl, 0, static_cast<int>(instance_accounts_.size()) + 1);
  RefreshAccountPictures();
}

void App::StartInstanceWith(int row) {
  const int count = static_cast<int>(instance_accounts_.size());
  if (row < 0 || row > count || !MenuOpen(kInstanceAccountControl)) return;
  InstanceAccount account;
  AccountUse use = AccountUse::kNotOpen;
  std::wstring name;
  if (row < count) {
    const SavedAccount& saved = instance_accounts_[static_cast<std::size_t>(row)].account;
    account.sign_in = InstanceSignIn::kSavedAccount;
    account.session_file = saved.session_file;
    use = QueryAccountUse(saved.id);
    name = SavedAccountName(saved);
  } else {
    account.sign_in = InstanceSignIn::kSignedOut;
  }
  CloseMenu();
  if (use == AccountUse::kNotOpen) {
    OpenAnotherWindow(account);
    return;
  }
  pending_instance_account_ = account;
  Overlay overlay;
  overlay.id = L"instance.in-use";
  overlay.status = 1;
  if (use == AccountUse::kInGame) {
    overlay.title = name + L" is in a game";
    overlay.body = L"This account is playing in another Roblox window. If the new window joins a game too, "
                   L"Roblox disconnects the other one.";
  } else {
    overlay.title = name + L" is already open";
    overlay.body = L"This account is signed in in another Roblox window. If both join a game, Roblox "
                   L"disconnects one of them.";
  }
  overlay.actions = {{L"instance.open", L"Open anyway", ButtonStyle::kStandard},
                     {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
  ShowOverlay(std::move(overlay));
}

bool App::OnInstanceOverlayAction(const std::wstring& id) {
  if (id != L"instance.open") return false;
  const InstanceAccount account = pending_instance_account_;
  pending_instance_account_ = {};
  CloseOverlay();
  if (account.sign_in != InstanceSignIn::kKeep) OpenAnotherWindow(account);
  return true;
}

void App::RemoveInstanceAccount(int row) {
  if (row < 0 || row >= static_cast<int>(instance_accounts_.size()) || !MenuOpen(kInstanceAccountControl)) return;
  const SavedAccount account = instance_accounts_[static_cast<std::size_t>(row)].account;
  std::wstring error;
  if (!RemoveSavedAccount(paths_.accounts_dir, account.id, &error)) {
    ShowToast(error);
    return;
  }
  RefreshInstanceAccounts();
  const int count = static_cast<int>(instance_accounts_.size());
  if (count == 0) {
    CloseMenu();
  } else {
    SetMenu(kInstanceAccountControl, std::min(row, count), count + 1);
  }
  ShowToast(L"Removed " + SavedAccountLabel(account) + L".");
}

void App::OpenAnotherWindow(InstanceAccount account) {
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
  RunInBackground([this, config, paths, number, account]() -> std::function<void()> {
    std::wstring error;
    const bool prepared = PrepareInstance(config, paths, number, &error, account);
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
