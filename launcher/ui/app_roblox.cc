// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include <windows.h>
#include <shobjidl.h>

#include <algorithm>
#include <cstring>
#include <cwchar>

#include "core/fflags_file.h"
#include "core/process.h"
#include "core/util.h"
#include "ui/app.h"

namespace elixir::launcher::ui {

namespace {

std::wstring ReadClipboardText(HWND owner) {
  std::wstring text;
  if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(owner)) return text;
  if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
    if (const auto* chars = static_cast<const wchar_t*>(GlobalLock(data))) {
      text.assign(chars, wcsnlen(chars, GlobalSize(data) / sizeof(wchar_t)));
      GlobalUnlock(data);
    }
  }
  CloseClipboard();
  return text;
}

bool WriteClipboardText(HWND owner, const std::wstring& text) {
  if (!OpenClipboard(owner)) return false;
  EmptyClipboard();
  bool ok = false;
  const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
    if (void* target = GlobalLock(memory)) {
      std::memcpy(target, text.c_str(), bytes);
      GlobalUnlock(memory);
      ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
    }
    if (!ok) GlobalFree(memory);
  }
  CloseClipboard();
  return ok;
}

bool ShowFileDialog(HWND owner, bool save, const wchar_t* title, const wchar_t* default_name, std::wstring* path) {
  ComPtr<IFileDialog> dialog;
  if (FAILED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dialog)))) {
    return false;
  }
  const COMDLG_FILTERSPEC filters[] = {{L"JSON files (*.json)", L"*.json"}, {L"All files (*.*)", L"*.*"}};
  dialog->SetFileTypes(2, filters);
  dialog->SetTitle(title);
  if (save) {
    dialog->SetDefaultExtension(L"json");
    if (default_name != nullptr) dialog->SetFileName(default_name);
  }
  FILEOPENDIALOGOPTIONS flags = 0;
  dialog->GetOptions(&flags);
  dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
  if (FAILED(dialog->Show(owner))) return false;
  ComPtr<IShellItem> item;
  PWSTR chosen = nullptr;
  if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &chosen))) return false;
  *path = chosen;
  CoTaskMemFree(chosen);
  return true;
}

fflags::FlagSet ToFlagSet(const std::vector<std::pair<std::string, std::string>>& flags) {
  fflags::FlagSet set;
  for (const auto& [name, value] : flags) set.Set(name, value);
  return set;
}

}

bool App::RuntimeRunning() {
  if (runtime_.running) return true;
  const ULONGLONG now = GetTickCount64();
  if (runtime_probe_valid_ && now - runtime_probe_at_ < kRuntimeProbeMaxAgeMs) return other_runtime_running_;
  if (options_.runtime_probe) return RuntimeRunningNow();
  if (!runtime_probe_pending_) {
    runtime_probe_pending_ = true;
    RunInBackground([this]() -> std::function<void()> {
      const bool running = IsAnyRuntimeProcessRunning();
      return [this, running] {
        runtime_probe_pending_ = false;
        const bool changed = !runtime_probe_valid_ || running != other_runtime_running_;
        other_runtime_running_ = running;
        runtime_probe_at_ = GetTickCount64();
        runtime_probe_valid_ = true;
        if (changed) Invalidate();
      };
    });
  }
  return other_runtime_running_;
}

bool App::RuntimeRunningNow() {
  other_runtime_running_ = options_.runtime_probe ? options_.runtime_probe() : IsAnyRuntimeProcessRunning();
  runtime_probe_at_ = GetTickCount64();
  runtime_probe_valid_ = true;
  return runtime_.running || other_runtime_running_;
}

bool App::PackageWorkRunning() const {
  return install_.stage == InstallFlow::Stage::kValidating || install_.stage == InstallFlow::Stage::kInstalling ||
         switching_;
}

bool App::fast_flags_dirty() const { return fast_flags_.editor != nullptr && fast_flags_.editor->dirty(); }

void App::SetFastFlagMessage(std::wstring message, int status) {
  fast_flags_.message = std::move(message);
  fast_flags_.message_status = status;
  Invalidate();
}

void App::LoadFastFlags(bool force) {
  const bool first = fast_flags_.editor == nullptr;
  if (first) fast_flags_.editor = std::make_unique<fflags::FlagEditor>();
  const std::wstring path = FflagsPath();
  if (!force && fast_flags_.editor->dirty() && fast_flags_.loaded_path == path) return;
  std::string error;
  if (fast_flags_.editor->Load(std::filesystem::path(path), &error)) {
    fast_flags_.load_error.clear();
  } else {
    fast_flags_.load_error = Utf8ToWide(error);
  }
  fast_flags_.loaded_path = path;
  if (first || fast_flags_.runtime == FastFlagsView::Runtime::kUnknown) RefreshRuntimeFlags();
  Invalidate();
}

bool App::SaveFastFlags() {
  if (!fast_flags_.editor) return false;
  if (!fast_flags_.load_error.empty() && !fast_flags_.editor->dirty()) {
    SetFastFlagMessage(L"Your Fast Flags file can’t be read. Use “Start over” to replace it.", 1);
    return false;
  }
  std::string error;
  if (!fast_flags_.editor->Save(&error)) {
    SetFastFlagMessage(L"Your Fast Flags couldn’t be saved. Make sure your drive has free space.", 2);
    return false;
  }
  fast_flags_.load_error.clear();
  if (fast_flags_.message_status == 2) fast_flags_.message.clear();
  Invalidate();
  return true;
}

void App::OpenFastFlagEditor(bool open) {
  fast_flags_.editor_open = open;
  fast_flags_.adding = false;
  fast_flags_.selected.clear();
  fast_flags_.message.clear();
  fast_flags_.search.clear();
  SetScroll(0.0f);
  ++page_epoch_;
  Invalidate();
}

void App::FastFlagsChanged() {
  if (hwnd_ != nullptr) SetTimer(hwnd_, kFastFlagsSaveTimer, 400, nullptr);
  Invalidate();
}

void App::SaveFastFlagsIfChanged() {
  if (hwnd_ != nullptr) KillTimer(hwnd_, kFastFlagsSaveTimer);
  if (fast_flags_.editor != nullptr && fast_flags_.editor->dirty()) SaveFastFlags();
}

void App::AddFastFlag() {
  if (!fast_flags_.editor) return;
  const std::string name = WideToUtf8(TrimWide(fast_flags_.new_name));
  if (name.empty()) {
    SetFastFlagMessage(L"Type a flag name first.", 1);
    return;
  }
  if (fast_flags_.editor->flags().Contains(name)) {
    SetFastFlagMessage(Utf8ToWide(name) + L" is already in your list. Change its value there.", 1);
    return;
  }
  const fflags::EditResult result = fast_flags_.editor->SetFlag(name, WideToUtf8(fast_flags_.new_value));
  if (!result.ok) {
    SetFastFlagMessage(Utf8ToWide(result.message), 2);
    return;
  }
  std::wstring message;
  if (result.status == fflags::FlagStatus::kInvalidValue) {
    message = Utf8ToWide(name) + L" has a value that isn’t valid: " + Utf8ToWide(result.message) + L".";
  }
  fast_flags_.new_name.clear();
  fast_flags_.new_value.clear();
  fast_flags_.adding = false;
  fast_flags_.motion.Enter(Utf8ToWide(name));
  SetFastFlagMessage(message, 1);
  FastFlagsChanged();
}

void App::ImportFastFlagsText(const std::string& json, bool replace) {
  if (!fast_flags_.editor) LoadFastFlags(true);
  std::set<std::string> before;
  for (const fflags::Flag& flag : fast_flags_.editor->flags().flags()) before.insert(flag.name);
  const fflags::ImportSummary summary =
      fast_flags_.editor->Import(json, replace ? fflags::ImportMode::kReplace : fflags::ImportMode::kMerge);
  if (!summary.ok) {
    fast_flags_.import_error = Utf8ToWide(summary.error);
    SetFastFlagMessage(L"Nothing was imported.", 2);
    return;
  }
  fast_flags_.import_error.clear();
  for (const fflags::Flag& flag : fast_flags_.editor->flags().flags()) {
    if (before.count(flag.name) == 0) fast_flags_.motion.Enter(Utf8ToWide(flag.name));
  }
  SetFastFlagMessage(summary.warnings.empty() ? std::wstring() : Utf8ToWide(summary.warnings.front()), 1);
  FastFlagsChanged();
}

void App::ImportFastFlagsFile(bool replace) {
  if (options_.test_mode) return;
  std::wstring path;
  if (!ShowFileDialog(hwnd_, false, replace ? L"Replace your Fast Flags" : L"Import Fast Flags", nullptr, &path)) {
    return;
  }
  std::string bytes;
  std::wstring error;
  if (!ReadFileBytes(path, &bytes, &error, 16u << 20)) {
    SetFastFlagMessage(L"That file couldn’t be opened.", 2);
    return;
  }
  ImportFastFlagsText(bytes, replace);
}

void App::ExportFastFlags(bool applied_only) {
  if (!fast_flags_.editor || options_.test_mode) return;
  std::wstring path;
  if (!ShowFileDialog(hwnd_, true, applied_only ? L"Export applied Fast Flags" : L"Export Fast Flags",
                      applied_only ? L"Fast Flags (applied).json" : L"Fast Flags.json", &path)) {
    return;
  }
  std::wstring error;
  if (AtomicWriteFile(path, fast_flags_.editor->Export(applied_only), AtomicWriteBackup::kNone, &error)) {
    SetFastFlagMessage({}, 4);
  } else {
    SetFastFlagMessage(L"That file couldn’t be saved. Try another folder.", 2);
  }
}

void App::RefreshRuntimeFlags() {
  if (!PathIsFile(paths_.runtime_exe)) {
    fast_flags_.runtime = FastFlagsView::Runtime::kUnavailable;
    fast_flags_.runtime_note.clear();
    return;
  }
  fast_flags_.runtime = FastFlagsView::Runtime::kLoading;
  const std::wstring runtime = paths_.runtime_exe;
  const std::wstring directory = paths_.exe_dir;
  const std::vector<std::wstring> command = RuntimeCommand(paths_, {L"--print-runtime-flags"});
  RunInBackground([this, runtime, directory, command]() -> std::function<void()> {
    const CaptureResult capture = RunCaptured(runtime, command, directory, {}, nullptr, 10000);
    RuntimeFlagsReport report;
    std::string error;
    bool ok = false;
    if (!capture.started) {
      error = WideToUtf8(capture.error);
    } else if (capture.timed_out || capture.exit_code != 0) {
      error = "it exited with code " + std::to_string(capture.exit_code) +
              (capture.stderr_text.empty() ? std::string() : ": " + TrimAscii(capture.stderr_text.substr(0, 300)));
    } else {
      ok = ParseRuntimeFlagsReport(capture.stdout_text, &report, &error);
    }
    return [this, ok, report] {
      if (ok) {
        fast_flags_.runtime = FastFlagsView::Runtime::kLoaded;
        fast_flags_.runtime_note.clear();
        if (fast_flags_.editor) fast_flags_.editor->SetRuntimeFlags(ToFlagSet(report.mandatory), ToFlagSet(report.defaults));
      } else {
        fast_flags_.runtime = FastFlagsView::Runtime::kUnavailable;
        fast_flags_.runtime_note.clear();
      }
      Invalidate();
    };
  });
}

bool App::GuardFastFlags(std::optional<Page> next_page, bool closing) {
  if (!fast_flags_dirty()) return false;
  if (SaveFastFlags()) return false;
  if (overlay_.open) return true;
  fast_flags_.pending_page = next_page;
  fast_flags_.pending_close = closing;
  Overlay overlay;
  overlay.id = L"fflags.guard";
  overlay.status = 1;
  overlay.title = L"Save your Fast Flags changes?";
  overlay.body = L"If you don’t save, your changes are lost.";
  overlay.actions = {{L"fflags.guard.save", L"Save", ButtonStyle::kStandard},
                     {L"fflags.guard.discard", L"Discard changes", ButtonStyle::kDanger},
                     {L"fflags.guard.cancel", L"Cancel", ButtonStyle::kStandard}};
  ShowOverlay(std::move(overlay));
  return true;
}

void App::MeasureCaches() {
  if (hwnd_ != nullptr && !options_.test_mode && page_ == Page::kSettings) {
    SetTimer(hwnd_, kCacheMeasureTimer, kCacheMeasureIntervalMs, nullptr);
  }
  if (cache_.measuring) return;
  cache_.measuring = true;
  const LauncherPaths paths = paths_;
  RunInBackground([this, paths]() -> std::function<void()> {
    std::vector<CacheLocation> locations = ClientCacheLocations(paths, kRobloxPackage, true);
    return [this, locations = std::move(locations)] {
      bool changed = !cache_.measured || cache_.locations.size() != locations.size();
      for (std::size_t i = 0; !changed && i < locations.size(); ++i) {
        const CacheLocation& before = cache_.locations[i];
        const CacheLocation& now = locations[i];
        changed = before.id != now.id || before.bytes != now.bytes || before.exists != now.exists ||
                  before.redirected != now.redirected;
      }
      cache_.locations = locations;
      cache_.measuring = false;
      cache_.measured = true;
      if (changed) Invalidate();
    };
  });
  if (!cache_.measured) Invalidate();
}

void App::AskClearCaches() {
  if (RuntimeRunningNow()) {
    ShowToast(L"Close Roblox before clearing the cache.");
    return;
  }
  if (PackageWorkRunning()) {
    ShowToast(L"Wait for Roblox to finish installing, then try again.");
    return;
  }
  std::uint64_t total = 0;
  for (const CacheLocation& location : cache_.locations) {
    if (location.exists) total += location.bytes;
  }
  Overlay overlay;
  overlay.id = L"cache.clear";
  overlay.status = 1;
  overlay.title = L"Clear Roblox’s cache?";
  overlay.body = L"This frees " + FormatByteSize(total) + L". Your sign-in, settings and Fast Flags are kept.";
  overlay.actions = {{L"cache.clear.confirm", L"Clear cache", ButtonStyle::kDanger},
                     {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
  ShowOverlay(std::move(overlay));
}

void App::ClearCaches() {
  if (cache_.clearing) return;
  if (PackageWorkRunning()) {
    cache_.message = L"Not cleared. Wait for Roblox to finish installing, then try again.";
    cache_.message_status = 1;
    Invalidate();
    return;
  }
  if (options_.test_mode) {
    cache_.message = L"Not cleared: test mode never deletes files.";
    cache_.message_status = 4;
    return;
  }
  cache_.clearing = true;
  const LauncherPaths paths = paths_;
  const bool running = RuntimeRunningNow();
  RunInBackground([this, paths, running]() -> std::function<void()> {
    const CacheClearResult result = ClearClientCaches(paths, kRobloxPackage, running);
    return [this, result, running] {
      cache_.clearing = false;
      if (result.ok) {
        cache_.message = L"Freed " + FormatByteSize(result.bytes_freed) + L".";
        cache_.message_status = 0;
      } else if (running) {
        cache_.message = L"Not cleared. Close Roblox, then try again.";
        cache_.message_status = 2;
      } else {
        cache_.message =
            L"Freed " + FormatByteSize(result.bytes_freed) + L". Some files were in use and weren’t deleted.";
        cache_.message_status = 2;
      }
      MeasureCaches();
    };
  });
  Invalidate();
}

std::wstring App::RobloxUpdateStatus() const {
  const std::wstring version =
      roblox_update_.version_name.empty() ? std::wstring() : L" " + Utf8ToWide(roblox_update_.version_name);
  const std::wstring percent =
      roblox_update_.percent >= 0 ? L" " + std::to_wstring(std::min(roblox_update_.percent, 100)) + L"%" : std::wstring();
  switch (roblox_update_.phase) {
    case RobloxUpdatePhase::kChecking:
      return roblox_update_.chosen ? L"Getting Roblox" + version + L"…" : std::wstring(L"Looking for the newest Roblox…");
    case RobloxUpdatePhase::kDownloading: return L"Downloading Roblox" + version + L"…" + percent;
    case RobloxUpdatePhase::kVerifying: return L"Checking that Roblox" + version + L" is Roblox’s own file…";
    case RobloxUpdatePhase::kInstalling: return L"Installing Roblox" + version + L"…" + percent;
  }
  return {};
}

void App::LoadRobloxVersions() {
  using Stage = RobloxVersionList::Stage;
  if (roblox_list_.stage == Stage::kLoading) return;
  const auto probe = options_.roblox_version_probe;
  if (options_.test_mode && !probe) return;
  roblox_list_.stage = Stage::kLoading;
  roblox_list_.cancel = std::make_shared<std::atomic<bool>>(false);
  const auto cancel = roblox_list_.cancel;
  const std::wstring pinned = LocateDownloadSources(paths_.exe_dir);
  RunInBackground([this, cancel, probe, pinned]() -> std::function<void()> {
    RobloxReleaseList list = probe ? probe() : WithPinnedFallback(FetchAllRobloxReleases(cancel.get()), pinned);
    return [this, list = std::move(list), cancel]() mutable {
      if (roblox_list_.cancel != cancel) return;
      roblox_list_.cancel.reset();
      std::int64_t selected_code = 0;
      if (roblox_list_.selected >= 0 && roblox_list_.selected < static_cast<int>(roblox_list_.list.releases.size())) {
        selected_code = roblox_list_.list.releases[static_cast<std::size_t>(roblox_list_.selected)].version_code;
      }
      roblox_list_.list = std::move(list);
      roblox_list_.stage = roblox_list_.list.ok ? Stage::kListed : Stage::kFailed;
      roblox_list_.selected = -1;
      for (std::size_t i = 0; i < roblox_list_.list.releases.size(); ++i) {
        if (selected_code != 0 && roblox_list_.list.releases[i].version_code == selected_code) {
          roblox_list_.selected = static_cast<int>(i);
        }
      }
      Invalidate();
    };
  });
  Invalidate();
}

void App::InstallSelectedRobloxVersion() {
  const std::vector<RobloxRelease>& releases = roblox_list_.list.releases;
  if (roblox_list_.selected < 0 || roblox_list_.selected >= static_cast<int>(releases.size())) return;
  if (RobloxUpdateRunning() || PackageWorkRunning() || cache_.clearing || !RobloxUpdatePossible()) return;
  const RobloxRelease release = releases[static_cast<std::size_t>(roblox_list_.selected)];
  std::int64_t newest = 0;
  for (const RobloxRelease& listed : releases) newest = std::max(newest, listed.version_code);
  if (release.version_code != newest && config_.roblox.auto_update) {
    config_.roblox.auto_update = false;
    SaveConfig();
  }
  StartRobloxUpdate( false, &release);
}

void App::CheckRobloxVersion(bool notify) {
  if (roblox_version_.pending) return;
  const auto cancel = std::make_shared<std::atomic<bool>>(false);
  roblox_version_.pending = cancel;
  roblox_version_.error.clear();
  const auto probe = options_.roblox_version_probe;
  const bool offline = options_.test_mode;
  RunInBackground([this, cancel, probe, offline, notify]() -> std::function<void()> {
    RobloxReleaseList list;
    if (probe) {
      list = probe();
    } else if (!offline) {
      list = FetchRobloxVersions(cancel.get());
    }
    return [this, list = std::move(list), notify, cancel] {
      if (roblox_version_.pending != cancel) return;
      roblox_version_.pending.reset();
      roblox_version_.latest.reset();
      if (list.ok && !list.pinned && !list.releases.empty()) {
        roblox_version_.latest = list.releases.front();
        config_.roblox.last_check = UnixNow();
        SaveConfig();
        const auto& latest = *roblox_version_.latest;
        if (config_.installed.package == kRobloxPackage && latest.version_code > config_.installed.version_code) {
          if (config_.roblox.auto_update && RobloxUpdatePossible()) {
            if (notify) ShowToast(L"Updating Roblox to " + Utf8ToWide(latest.version_name) + L"…");
            StartRobloxUpdate( false);
          } else if (notify) {
            ShowToast(L"Roblox " + Utf8ToWide(latest.version_name) + L" is available. Open Installation to update.");
          }
        }
      } else {
        roblox_version_.error = L"Couldn’t check for Roblox updates.";
        const auto now = UnixNow();
        if (list.retry_at > now) {
          roblox_version_.error += L" Try again at " + FormatLocalClockTime(list.retry_at, now) + L".";
        } else {
          roblox_version_.error += L" Try again later.";
        }
      }
      Invalidate();
    };
  });
  Invalidate();
}

void App::SwitchToInstalledVersion(const InstalledApp& next, SwitchDone done) {
  if (runtime_.running || RuntimeRunningNow()) {
    staged_roblox_ = next;
    done(SwitchOutcome::kStaged,
         L"Roblox " + Utf8ToWide(next.version_name) + L" is ready. Elixir switches to it when you close Roblox.");
    return;
  }
  const InstalledApp current = config_.installed;
  const bool replacing = current.present() && current.package == next.package &&
                         !PathsEqual(Utf8ToWide(current.install_dir), Utf8ToWide(next.install_dir));
  const auto commit = [this, next, current](const AppDataSnapshot* snapshot) -> std::wstring {
    const RobloxSourceSettings before = config_.roblox;
    if (snapshot != nullptr) {
      config_.roblox.previous = current;
      config_.roblox.previous_snapshot = WideToUtf8(snapshot->path);
    }
    config_.installed = next;
    SaveConfig();
    if (!save_error_.empty()) {
      config_.installed = current;
      config_.roblox = before;
      return L"Couldn’t switch to Roblox " + Utf8ToWide(next.version_name) + L". Free up disk space and try again.";
    }
    staged_roblox_.reset();
    return {};
  };
  const std::wstring switched = L"Switched to Roblox " + Utf8ToWide(next.version_name) + L".";
  if (!replacing) {
    const std::wstring error = commit(nullptr);
    done(error.empty() ? SwitchOutcome::kSwitched : SwitchOutcome::kFailed, error.empty() ? switched : error);
    return;
  }
  switching_ = true;
  Invalidate();
  const std::wstring data_dir = AndroidAppDataDirectory(paths_, current.package);
  const std::wstring backups = AppBackupsDirectory(BackupsRoot(), next.package);
  RunInBackground([this, data_dir, backups, current, next, commit, switched, done]() -> std::function<void()> {
    const AppDataSnapshot snapshot = SnapshotAppData(
        data_dir, backups, UtcFileStamp() + L"-v" + std::to_wstring(current.version_code), current.version_code);
    return [this, snapshot, backups, current, next, commit, switched, done] {
      switching_ = false;
      if (!snapshot.ok) {
        done(SwitchOutcome::kFailed, L"Couldn’t back up your Roblox data. Free up disk space and try again.");
        Invalidate();
        return;
      }
      if (runtime_.running || RuntimeRunningNow()) {
        DeleteDirectoryTree(snapshot.path);
        staged_roblox_ = next;
        done(SwitchOutcome::kStaged,
             L"Roblox " + Utf8ToWide(next.version_name) + L" is ready. Elixir switches to it when you close Roblox.");
        Invalidate();
        return;
      }
      const std::wstring error = commit(&snapshot);
      if (!error.empty()) {
        done(SwitchOutcome::kFailed, error);
        Invalidate();
        return;
      }
      const std::wstring package_dir = ParentDirectory(Utf8ToWide(next.install_dir));
      const std::vector<std::wstring> keep = {Utf8ToWide(next.install_dir), Utf8ToWide(current.install_dir)};
      const std::int64_t below = next.version_code;
      const std::wstring protect = snapshot.path;
      RunInBackground([package_dir, keep, below, backups, protect]() -> std::function<void()> {
        PruneVersions(package_dir, keep, below);
        PruneSnapshots(backups, 2, {protect});
        return [] {};
      });
      done(SwitchOutcome::kSwitched, switched);
      Invalidate();
    };
  });
}

void App::AskRollBackRoblox() {
  const InstalledApp& previous = config_.roblox.previous;
  if (!previous.present()) return;
  Overlay overlay;
  overlay.id = L"roblox.rollback";
  overlay.status = 1;
  overlay.title = L"Go back to Roblox " + Utf8ToWide(previous.version_name) + L"?";
  const std::wstring data = config_.roblox.previous_snapshot.empty()
                                ? L"Your Roblox data stays as it is."
                                : L"Your Roblox data goes back to how it was before the last update. Your current "
                                  L"data is backed up first.";
  overlay.body = data;
  overlay.actions = {{L"overlay.rollback", L"Roll back", ButtonStyle::kStandard},
                     {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
  ShowOverlay(std::move(overlay));
}

void App::RollBackRoblox() {
  const InstalledApp previous = config_.roblox.previous;
  const InstalledApp current = config_.installed;
  if (!previous.present() || !current.present() || switching_) return;
  if (runtime_.running || RuntimeRunningNow() || PackageWorkRunning()) {
    ShowToast(L"Close Roblox and wait for any install to finish, then try again.");
    return;
  }
  if (!PathIsDirectory(Utf8ToWide(previous.install_dir))) {
    ShowToast(L"Roblox " + Utf8ToWide(previous.version_name) + L" is no longer available. Nothing was changed.");
    return;
  }
  const std::wstring backups = AppBackupsDirectory(BackupsRoot(), current.package);
  const std::wstring snapshot = Utf8ToWide(config_.roblox.previous_snapshot);
  const std::wstring data_dir = AndroidAppDataDirectory(paths_, current.package);
  switching_ = true;
  Invalidate();
  RunInBackground([this, backups, snapshot, data_dir, previous, current]() -> std::function<void()> {
    std::wstring error;
    AppDataSnapshot replaced;
    if (!snapshot.empty()) replaced = RestoreAppData(snapshot, data_dir, backups, &error);
    if (error.empty() && !snapshot.empty() && !replaced.ok) error = replaced.error;
    return [this, error, replaced, previous, current] {
      switching_ = false;
      if (!error.empty()) {
        ShowToast(L"Couldn’t restore your Roblox data. Nothing was changed.");
        Invalidate();
        return;
      }
      const RobloxSourceSettings saved = config_.roblox;
      config_.roblox.previous = current;
      config_.roblox.previous_snapshot = WideToUtf8(replaced.path);
      config_.installed = previous;
      SaveConfig();
      if (!save_error_.empty()) {
        config_.installed = current;
        config_.roblox = saved;
        ShowToast(L"Couldn’t roll back. Free up disk space and try again.");
        Invalidate();
        return;
      }
      ShowToast(L"Back on Roblox " + Utf8ToWide(previous.version_name) + L".");
      Invalidate();
    };
  });
}

bool App::OnRobloxOverlayAction(const std::wstring& id) {
  const auto continue_after_guard = [this] {
    const std::optional<Page> page = fast_flags_.pending_page;
    const bool closing = fast_flags_.pending_close;
    fast_flags_.pending_page.reset();
    fast_flags_.pending_close = false;
    if (closing && hwnd_ != nullptr) {
      PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    } else if (page) {
      Navigate(*page);
    }
  };
  if (id == L"overlay.rollback") {
    CloseOverlay();
    RollBackRoblox();
    return true;
  }
  if (id == L"fflags.guard.save") {
    CloseOverlay();
    if (SaveFastFlags()) {
      continue_after_guard();
    } else {
      fast_flags_.pending_page.reset();
      fast_flags_.pending_close = false;
      ShowToast(fast_flags_.message);
    }
    return true;
  }
  if (id == L"fflags.guard.discard") {
    CloseOverlay();
    fflags::FlagSet mandatory = fast_flags_.editor ? fast_flags_.editor->runtime_mandatory() : fflags::FlagSet{};
    fflags::FlagSet defaults = fast_flags_.editor ? fast_flags_.editor->runtime_defaults() : fflags::FlagSet{};
    fast_flags_.editor = std::make_unique<fflags::FlagEditor>();
    fast_flags_.editor->SetRuntimeFlags(std::move(mandatory), std::move(defaults));
    LoadFastFlags(true);
    SetFastFlagMessage({}, 4);
    continue_after_guard();
    return true;
  }
  if (id == L"fflags.guard.cancel") {
    fast_flags_.pending_page.reset();
    fast_flags_.pending_close = false;
    CloseOverlay();
    return true;
  }
  if (id == L"fflags.import.paste") {
    CloseOverlay();
    const std::wstring text = ReadClipboardText(hwnd_);
    if (TrimWide(text).empty()) {
      SetFastFlagMessage(L"There’s nothing to paste. Copy the JSON first.", 1);
    } else {
      ImportFastFlagsText(WideToUtf8(text), false);
    }
    return true;
  }
  if (id == L"fflags.import.file") {
    CloseOverlay();
    ImportFastFlagsFile(false);
    return true;
  }
  if (id == L"fflags.export.copy") {
    CloseOverlay();
    if (fast_flags_.editor && WriteClipboardText(hwnd_, Utf8ToWide(fast_flags_.editor->Export(false)))) {
      ShowToast(L"Copied.");
    } else {
      ShowToast(L"Couldn’t copy your Fast Flags.");
    }
    return true;
  }
  if (id == L"fflags.export.file") {
    CloseOverlay();
    ExportFastFlags(false);
    return true;
  }
  if (id == L"fflags.reset.confirm") {
    CloseOverlay();
    if (fast_flags_.editor && fast_flags_.editor->ClearAll()) FastFlagsChanged();
    return true;
  }
  if (id == L"fflags.startover.confirm") {
    CloseOverlay();
    if (fast_flags_.editor) {
      fast_flags_.editor->StartOver(std::filesystem::path(FflagsPath()));
      fast_flags_.load_error.clear();
      SetFastFlagMessage({}, 4);
    }
    return true;
  }
  if (id == L"cache.clear.confirm") {
    CloseOverlay();
    ClearCaches();
    return true;
  }
  return false;
}

}
