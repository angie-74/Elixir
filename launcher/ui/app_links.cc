// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <algorithm>

#include "core/instances.h"
#include "core/process.h"
#include "core/roblox_links.h"
#include "core/util.h"
#include "core/i18n.h"
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
        ShowToast(T(L"This Roblox link can’t be opened in Elixir."));
        break;
    }
  });
}

void App::BringToFront() {
  if (hwnd_ == nullptr) return;
  account_picker_.Close();
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
      ShowToast(T(L"Roblox is closing. Open the link again in a moment."));
      return;
    }
    if (runtime_.in_game) {
      Overlay overlay;
      overlay.id = L"join.leave";
      overlay.status = 1;
      overlay.title = T(L"Leave this experience?");
      overlay.body = T(L"Roblox will leave the experience you’re in and join the one you opened.");
      overlay.actions = {{L"join.confirm", T(L"Join"), ButtonStyle::kStandard},
                         {L"overlay.close", T(L"Stay"), ButtonStyle::kStandard}};
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
  ShowToast(sent ? T(L"Joining the experience…") : T(L"Couldn’t join the experience. Open the link again."));
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
    ShowToast(T(L"Sign in to Roblox to join the experience."));
  } else if (state == "busy") {
    ShowToast(T(L"Roblox is busy. Open the link again in a moment."));
  } else if (state == "rejected") {
    ShowToast(T(L"Roblox couldn’t join that experience."));
  }
  if (state == "in-game" || state == "left") RefreshActivity();
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
  if (account_picker_.open()) {
    if (instance_accounts_.empty()) {
      account_picker_.Close();
    } else {
      account_picker_.SetRows(instance_accounts_);
    }
  }
}

void App::PrefetchAccountPictures() {
  RefreshInstanceAccounts();
  RefreshAccountPictures();
}

void App::RefreshAccountPictures(int more_rounds, bool wait_first) {
  constexpr ULONGLONG kCheckEveryMs = 60000;
  constexpr DWORD kPendingWaitMs = 700;
  const HttpGetFunction get = WebGet();
  if (!get || account_pictures_loading_ || instance_accounts_.empty()) return;
  const bool missing = std::any_of(instance_accounts_.begin(), instance_accounts_.end(),
                                   [](const InstanceAccountRow& row) { return row.account.picture_file.empty(); });
  const ULONGLONG now = GetTickCount64();
  if (!wait_first && !missing && account_pictures_checked_ != 0 && now - account_pictures_checked_ < kCheckEveryMs) {
    return;
  }
  account_pictures_loading_ = true;
  account_pictures_checked_ = now;
  std::vector<SavedAccount> accounts;
  for (const InstanceAccountRow& row : instance_accounts_) accounts.push_back(row.account);
  const std::wstring folder = paths_.accounts_dir;
  RunInBackground([this, folder, accounts, get, more_rounds, wait_first]() -> std::function<void()> {
    if (wait_first) Sleep(kPendingWaitMs);
    bool pending = false;
    const int changed = elixir::launcher::RefreshAccountPictures(folder, accounts, get, &pending);
    return [this, changed, pending, more_rounds] {
      account_pictures_loading_ = false;
      if (changed > 0) {
        RefreshInstanceAccounts();
        Invalidate();
      }
      if (pending && more_rounds > 0) RefreshAccountPictures(more_rounds - 1, true);
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

bool App::PlayAccountChoice(int* focus) {
  if (runtime_.running || !PathIsFile(paths_.runtime_exe) || !config_.installed.present() || switching_) return false;
  RefreshInstanceAccounts();
  const int count = static_cast<int>(instance_accounts_.size());
  const std::wstring current = SignedInAccountId(paths_.data_root);
  const bool unknown = current.empty() && HasSavedSignIn(paths_.data_root);
  const bool another = count == 1 && !unknown && instance_accounts_[0].account.id != current;
  if (count < 2 && !another) return false;
  *focus = 0;
  for (int i = 0; i < count; ++i) {
    if (instance_accounts_[static_cast<std::size_t>(i)].account.id == current) *focus = i;
  }
  return true;
}

void App::ChoosePlayAccount() {
  if (runtime_.running) return;
  if (MenuOpen(kPlayAccountControl)) {
    CloseMenu();
    return;
  }
  int focus = 0;
  if (!PlayAccountChoice(&focus)) {
    Play();
    return;
  }
  SetMenu(kPlayAccountControl, focus, static_cast<int>(instance_accounts_.size()) + 1);
  RefreshAccountPictures();
}

void App::OfferLaunchAccounts(int focus) {
  const HWND button = GetDlgItem(launch_menu_.hwnd(), kLaunchMenuPlayId);
  AccountPicker::Request request;
  request.owner = launch_menu_.hwnd();
  request.anchor = button;
  request.rows = instance_accounts_;
  request.focus = focus;
  request.keyboard = launch_by_keyboard_ ||
                     (button != nullptr && (SendMessageW(button, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) == 0);
  request.motion = motion_enabled_ && !options_.test_mode;
  request.offscreen = options_.offscreen;
  request.accent = state_.control_accent_rgb;
  request.on_open = [this](int row) { StartAccount(row, AccountTarget::kLaunchMenu); };
  request.on_remove = [this](int row) { RemoveLaunchAccount(row); };
  if (!account_picker_.Show(std::move(request))) {
    ShowWindow(launch_menu_.hwnd(), SW_HIDE);
    PlayFromMenu();
    return;
  }
  RefreshAccountPictures();
}

void App::PlayWith(const InstanceAccount& account, const std::wstring& account_id) {
  if (runtime_.running) return;
  std::wstring error;
  if (!PrepareMainAccount(paths_, account, account_id, &error)) {
    ShowToast(TF(L"Couldn’t switch accounts: {0}.", error));
    return;
  }
  Play();
}

void App::PlayFromMenuWith(const InstanceAccount& account, const std::wstring& account_id) {
  if (runtime_.running) return;
  std::wstring error;
  if (!PrepareMainAccount(paths_, account, account_id, &error)) {
    BringToFront();
    ShowToast(TF(L"Couldn’t switch accounts: {0}.", error));
    return;
  }
  ShowWindow(launch_menu_.hwnd(), SW_HIDE);
  PlayFromMenu();
}

void App::StartInstanceWith(int row) {
  if (row < 0 || row > static_cast<int>(instance_accounts_.size()) || !AccountMenuOpen()) return;
  const AccountTarget target = MenuOpen(kPlayAccountControl) ? AccountTarget::kPlay : AccountTarget::kNewInstance;
  CloseMenu();
  StartAccount(row, target);
}

void App::StartAccount(int row, AccountTarget target) {
  const int count = static_cast<int>(instance_accounts_.size());
  if (row < 0 || row > count) return;
  InstanceAccount account;
  AccountUse use = AccountUse::kNotOpen;
  std::wstring name;
  std::wstring id;
  if (row < count) {
    const SavedAccount& saved = instance_accounts_[static_cast<std::size_t>(row)].account;
    account.sign_in = InstanceSignIn::kSavedAccount;
    account.session_file = saved.session_file;
    use = QueryAccountUse(saved.id);
    name = SavedAccountName(saved);
    id = saved.id;
  } else {
    account.sign_in = InstanceSignIn::kSignedOut;
  }
  if (target == AccountTarget::kLaunchMenu) account_picker_.Close();
  if (use == AccountUse::kNotOpen) {
    if (target == AccountTarget::kLaunchMenu) {
      PlayFromMenuWith(account, id);
    } else if (target == AccountTarget::kPlay) {
      PlayWith(account, id);
    } else {
      OpenAnotherWindow(account);
    }
    return;
  }
  pending_instance_account_ = account;
  pending_account_id_ = id;
  pending_account_for_play_ = target != AccountTarget::kNewInstance;
  Overlay overlay;
  overlay.id = L"instance.in-use";
  overlay.status = 1;
  if (use == AccountUse::kInGame) {
    overlay.title = TF(L"{0} is in a game", name);
    overlay.body = T(L"This account is playing in another Roblox window. If the new window joins a game too, "
                   L"Roblox disconnects the other one.");
  } else {
    overlay.title = TF(L"{0} is already open", name);
    overlay.body = T(L"This account is signed in in another Roblox window. If both join a game, Roblox "
                   L"disconnects one of them.");
  }
  overlay.actions = {{L"instance.open", T(L"Open anyway"), ButtonStyle::kStandard},
                     {L"overlay.close", T(L"Cancel"), ButtonStyle::kStandard}};
  ShowOverlay(std::move(overlay));
  if (target == AccountTarget::kLaunchMenu) BringToFront();
}

bool App::OnInstanceOverlayAction(const std::wstring& id) {
  if (id != L"instance.open") return false;
  const InstanceAccount account = pending_instance_account_;
  const std::wstring account_id = pending_account_id_;
  const bool for_play = pending_account_for_play_;
  pending_instance_account_ = {};
  pending_account_id_.clear();
  pending_account_for_play_ = false;
  CloseOverlay();
  if (account.sign_in == InstanceSignIn::kKeep) return true;
  if (for_play) {
    PlayWith(account, account_id);
  } else {
    OpenAnotherWindow(account);
  }
  return true;
}

void App::RemoveInstanceAccount(int row) {
  if (row < 0 || row >= static_cast<int>(instance_accounts_.size()) || !AccountMenuOpen()) return;
  const std::wstring control = menu_control_;
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
    SetMenu(control, std::min(row, count), count + 1);
  }
  ShowToast(TF(L"Removed {0}.", SavedAccountLabel(account)));
}

void App::RemoveLaunchAccount(int row) {
  if (row < 0 || row >= static_cast<int>(instance_accounts_.size()) || !account_picker_.open()) return;
  const std::wstring id = instance_accounts_[static_cast<std::size_t>(row)].account.id;
  std::wstring error;
  if (!RemoveSavedAccount(paths_.accounts_dir, id, &error)) {
    BringToFront();
    ShowToast(error);
    return;
  }
  RefreshInstanceAccounts();
  account_picker_.Focus(std::min(row, static_cast<int>(instance_accounts_.size())));
}

void App::OpenAnotherWindow(InstanceAccount account) {
  if (!runtime_.running || runtime_.stopping || opening_window_) return;
  const int number = FreeWindowNumber();
  if (number == 0) {
    ShowToast(TF(L"Elixir opens up to {0} Roblox instances.", kMaxRobloxWindows));
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
        ShowToast(TF(L"Couldn’t open another Roblox instance: {0}.", error));
        return;
      }
      const RuntimeLaunch launch = LaunchInstance(config, paths, number);
      if (!launch.ok) {
        ShowToast(launch.error.empty() ? T(L"Couldn’t open another Roblox instance.")
                                       : TF(L"Couldn’t open another Roblox window. {0}", launch.error));
        return;
      }
      extra_windows_.push_back(ExtraWindow{number, launch.process});
      ShowToast(TF(L"Opening Roblox instance {0}…", number));
    };
  });
}

}
