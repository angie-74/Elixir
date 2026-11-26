// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/roblox_update.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>

#include "core/json.h"
#include "core/util.h"

namespace elixir::launcher {

namespace {

std::wstring Long(const std::wstring& path) {
  if (path.size() < 240 || path.rfind(L"\\\\?\\", 0) == 0) return path;
  if (path.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + path.substr(2);
  return L"\\\\?\\" + path;
}

std::wstring LowerName(std::wstring_view name) {
  std::wstring out(name);
  for (wchar_t& c : out) c = static_cast<wchar_t>(std::towlower(c));
  return out;
}

bool AllDigits(std::wstring_view text) {
  return !text.empty() && std::all_of(text.begin(), text.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

bool CopyAndFlush(const std::wstring& from, const std::wstring& to, std::wstring* error) {
  if (!CopyFileExW(Long(from).c_str(), Long(to).c_str(), nullptr, nullptr, nullptr, COPY_FILE_FAIL_IF_EXISTS)) {
    *error = WithErrorCause(L"A file couldn’t be copied.", GetLastError());
    return false;
  }
  HANDLE file = CreateFileW(Long(to).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    *error = WithErrorCause(L"A file couldn’t be copied.", GetLastError());
    return false;
  }
  const bool flushed = FlushFileBuffers(file) != FALSE;
  const DWORD flush_error = GetLastError();
  CloseHandle(file);
  if (!flushed) {
    *error = WithErrorCause(L"A file couldn’t be copied.", flush_error);
    return false;
  }
  return true;
}

struct CopyStats {
  std::uint64_t files = 0;
  std::uint64_t bytes = 0;
  std::uint64_t skipped_links = 0;
};

bool CopyTree(const std::wstring& from, const std::wstring& to, CopyStats* stats, std::wstring* error) {
  if (!CreateDirectoryW(Long(to).c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
    *error = WithErrorCause(L"A folder couldn’t be created.", GetLastError());
    return false;
  }
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileW(Long(JoinPath(from, L"*")).c_str(), &data);
  if (find == INVALID_HANDLE_VALUE) {
    const DWORD code = GetLastError();
    if (code == ERROR_FILE_NOT_FOUND) return true;
    *error = WithErrorCause(L"A folder couldn’t be read.", code);
    return false;
  }
  bool ok = true;
  do {
    const std::wstring name = data.cFileName;
    if (name == L"." || name == L"..") continue;
    const std::wstring source = JoinPath(from, name);
    const std::wstring target = JoinPath(to, name);
    if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      ++stats->skipped_links;
      continue;
    }
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (IsCacheFolderName(name)) continue;
      ok = CopyTree(source, target, stats, error);
    } else {
      ok = CopyAndFlush(source, target, error);
      if (ok) {
        ++stats->files;
        stats->bytes += (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
      }
    }
  } while (ok && FindNextFileW(find, &data));
  FindClose(find);
  return ok;
}

bool MoveFolder(const std::wstring& from, const std::wstring& to, std::wstring* error) {
  if (MoveFileExW(Long(from).c_str(), Long(to).c_str(), MOVEFILE_WRITE_THROUGH)) return true;
  *error = WithErrorCause(L"A folder couldn’t be moved.", GetLastError());
  return false;
}

std::vector<std::wstring> ChildFolders(const std::wstring& directory) {
  std::vector<std::wstring> names;
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileW(Long(JoinPath(directory, L"*")).c_str(), &data);
  if (find == INVALID_HANDLE_VALUE) return names;
  do {
    const std::wstring name = data.cFileName;
    if (name == L"." || name == L"..") continue;
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      names.push_back(name);
    }
  } while (FindNextFileW(find, &data));
  FindClose(find);
  return names;
}

bool EndsWith(std::wstring_view text, std::wstring_view suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}

bool NewerRobloxRelease(const RobloxReleaseList& list, std::int64_t installed_code, RobloxRelease* out) {
  if (!list.ok) return false;
  const RobloxRelease* best = nullptr;
  for (const RobloxRelease& release : list.releases) {
    if (release.version_code > installed_code && (best == nullptr || release.version_code > best->version_code)) {
      best = &release;
    }
  }
  if (best == nullptr) return false;
  if (out != nullptr) *out = *best;
  return true;
}

RobloxUpdateSteps RealRobloxUpdateSteps(const RobloxUpdateRequest& request) {
  RobloxUpdateSteps steps;
  steps.list = [](const std::atomic<bool>* cancel) { return FetchRobloxReleases(cancel); };
  steps.download = [request](const RobloxRelease& release, const RobloxProgressCallback& progress,
                             const std::atomic<bool>* cancel) {
    return DownloadRobloxRelease(release, request.downloads_dir, progress, cancel);
  };
  steps.verify = [request](const std::vector<std::wstring>& files, const ApkProgressCallback& progress,
                           const std::atomic<bool>* cancel) {
    SigningPins pins;
    std::wstring error;
    if (!LoadSigningPins(LocateSigningPins(request.exe_dir), &pins, &error)) {
      ApkResult failed;
      failed.error = L"Elixir’s list of Roblox signing certificates is missing. Reinstall Elixir to fix this.";
      failed.log = "signing pins: " + WideToUtf8(error) + "\n";
      return failed;
    }
    return VerifyRobloxPackage(request.component, files, pins, request.work_dir, progress, cancel);
  };
  steps.install = [request](const ApkMetadata& verified, const ApkProgressCallback& progress,
                            const std::atomic<bool>* cancel) {
    ApkComponent pinned = request.component;
    const std::wstring pins = LocateSigningPins(request.exe_dir);
    if (!pins.empty()) pinned.trust_manifest = pins;
    return InstallApk(pinned, verified, request.apps_dir, progress, cancel);
  };
  steps.discard = [request](const std::wstring& directory) {
    DeleteRobloxDownload(directory, request.downloads_dir);
  };
  return steps;
}

RobloxUpdateOutcome UpdateRoblox(const RobloxUpdateRequest& request, const RobloxUpdateSteps& steps,
                                 const RobloxUpdateProgressCallback& progress, const std::atomic<bool>* cancel) {
  using Result = RobloxUpdateOutcome::Result;
  RobloxUpdateOutcome outcome;
  const auto cancelled = [cancel] { return cancel != nullptr && cancel->load(); };
  const auto report = [&progress](RobloxUpdatePhase phase, const std::string& version, int percent) {
    if (!progress) return;
    RobloxUpdateProgress now;
    now.phase = phase;
    now.version_name = version;
    now.percent = percent;
    progress(now);
  };
  const auto fail = [&outcome](std::wstring error) {
    outcome.result = Result::kFailed;
    outcome.error = std::move(error);
    return outcome;
  };
  const auto stop = [&outcome] {
    outcome.result = Result::kCancelled;
    return outcome;
  };

  report(RobloxUpdatePhase::kChecking, request.target ? request.target->version_name : std::string(), -1);
  RobloxRelease release;
  if (request.target) {
    release = *request.target;
    if (release.version_code == request.installed_code) {
      outcome.result = Result::kUpToDate;
      return outcome;
    }
  } else {
    const RobloxReleaseList list = steps.list(cancel);
    if (cancelled()) return stop();
    if (!list.ok || list.pinned) {
      return fail(list.error.empty() ? std::wstring(L"Couldn’t get the list of Roblox versions.") : list.error);
    }
    if (!NewerRobloxRelease(list, request.installed_code, &release)) {
      outcome.result = Result::kUpToDate;
      return outcome;
    }
  }
  outcome.release = release;
  const std::string& version = release.version_name;

  report(RobloxUpdatePhase::kDownloading, version, 0);
  const std::uint64_t total = release.TotalSize();
  RobloxDownloadResult download = steps.download(
      release,
      [&](const RobloxDownloadProgress& file) {
        std::uint64_t before = 0;
        for (int i = 0; i + 1 < file.file && static_cast<std::size_t>(i) < release.files.size(); ++i) {
          before += release.files[static_cast<std::size_t>(i)].size;
        }
        const std::uint64_t done = std::min(total, before + file.done);
        report(RobloxUpdatePhase::kDownloading, version, total != 0 ? static_cast<int>(done * 100 / total) : -1);
      },
      cancel);
  outcome.log += download.log;
  if (download.cancelled || cancelled()) return stop();
  if (!download.ok) {
    return fail(download.error.empty() ? std::wstring(L"Couldn’t download Roblox. Check your connection and try again.")
                                       : download.error);
  }
  std::vector<std::wstring> files;
  for (const DownloadedFile& file : download.files) files.push_back(file.path);

  report(RobloxUpdatePhase::kVerifying, version, -1);
  const ApkResult verified = steps.verify(
      files, [&](const ApkProgress& step) { report(RobloxUpdatePhase::kVerifying, version, step.percent); }, cancel);
  outcome.log += verified.log;
  if (cancelled()) return stop();
  if (!verified.ok) {
    steps.discard(download.directory);
    return fail(verified.error.empty()
                    ? std::wstring(L"The downloaded Roblox files didn’t pass Elixir’s checks, so they were deleted.")
                    : verified.error);
  }
  if (verified.metadata.version_code != release.version_code) {
    steps.discard(download.directory);
    outcome.log += "the download is versionCode " + std::to_string(verified.metadata.version_code) +
                   ", the list named " + std::to_string(release.version_code) + "\n";
    return fail(L"The download wasn’t the Roblox version it was listed as, so it was deleted.");
  }

  report(RobloxUpdatePhase::kInstalling, version, -1);
  const ApkResult installed = steps.install(
      verified.metadata, [&](const ApkProgress& step) { report(RobloxUpdatePhase::kInstalling, version, step.percent); },
      cancel);
  outcome.log += installed.log;
  if (!installed.ok) {
    if (cancelled()) return stop();
    return fail(installed.error.empty() ? std::wstring(L"Couldn’t install the new Roblox version.") : installed.error);
  }
  steps.discard(download.directory);
  outcome.result = Result::kInstalled;
  outcome.metadata = installed.metadata;
  return outcome;
}

bool IsCacheFolderName(std::wstring_view name) {
  const std::wstring lower = LowerName(name);
  return lower == L"cache" || lower == L"code_cache";
}

std::wstring AppBackupsDirectory(const std::wstring& backups_root, std::string_view package) {
  std::wstring safe;
  for (const char c : package) {
    const bool allowed =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
    safe.push_back(allowed ? static_cast<wchar_t>(c) : L'_');
  }
  while (!safe.empty() && safe.front() == L'.') safe.erase(safe.begin());
  if (safe.empty()) safe = L"unknown";
  return JoinPath(backups_root, safe);
}

AppDataSnapshot SnapshotAppData(const std::wstring& data_dir, const std::wstring& backups_dir,
                                const std::wstring& name, std::int64_t version_code) {
  AppDataSnapshot snapshot;
  if (!EnsureDirectory(backups_dir, &snapshot.error)) return snapshot;
  const std::wstring final_path = JoinPath(backups_dir, name);
  const std::wstring partial = final_path + L".partial";
  if (PathIsDirectory(final_path)) {
    snapshot.error = L"A backup with this name already exists. Try again in a moment.";
    return snapshot;
  }
  DeleteDirectoryTree(partial);
  CopyStats stats;
  bool copied = true;
  const DWORD source = GetFileAttributesW(Long(data_dir).c_str());
  if (source == INVALID_FILE_ATTRIBUTES) {
    copied = CreateDirectoryW(Long(partial).c_str(), nullptr) != FALSE;
    if (!copied) snapshot.error = WithErrorCause(L"A folder couldn’t be created.", GetLastError());
  } else if (!(source & FILE_ATTRIBUTE_DIRECTORY) || (source & FILE_ATTRIBUTE_REPARSE_POINT)) {
    snapshot.error = L"Your Roblox data isn’t in a normal folder, so it wasn’t copied.";
    copied = false;
  } else {
    copied = CopyTree(data_dir, partial, &stats, &snapshot.error);
  }
  if (copied) {
    Json record = Json::MakeObject();
    record.Set("source", WideToUtf8(data_dir));
    record.Set("created_at", UtcIsoTimestamp());
    record.Set("version_code", version_code);
    record.Set("files", static_cast<std::int64_t>(stats.files));
    record.Set("bytes", static_cast<std::int64_t>(stats.bytes));
    record.Set("skipped_links", static_cast<std::int64_t>(stats.skipped_links));
    copied = AtomicWriteFile(JoinPath(partial, L"snapshot.json"), record.Dump(2), AtomicWriteBackup::kNone,
                             &snapshot.error) &&
             MoveFolder(partial, final_path, &snapshot.error);
  }
  if (!copied) {
    DeleteDirectoryTree(partial);
    return snapshot;
  }
  snapshot.ok = true;
  snapshot.path = final_path;
  snapshot.files = stats.files;
  snapshot.bytes = stats.bytes;
  snapshot.skipped_links = stats.skipped_links;
  return snapshot;
}

AppDataSnapshot RestoreAppData(const std::wstring& snapshot, const std::wstring& data_dir,
                               const std::wstring& backups_dir, std::wstring* error) {
  AppDataSnapshot before;
  if (!PathIsFile(JoinPath(snapshot, L"snapshot.json"))) {
    *error = L"That copy of your Roblox data is incomplete.";
    return before;
  }
  before = SnapshotAppData(data_dir, backups_dir, UtcFileStamp() + L"-before-restore", 0);
  if (!before.ok) {
    *error = L"Your current Roblox data couldn’t be backed up first. " + before.error;
    return before;
  }
  const std::wstring restoring = data_dir + L".restoring";
  DeleteDirectoryTree(restoring);
  CopyStats stats;
  if (!CopyTree(snapshot, restoring, &stats, error)) {
    DeleteDirectoryTree(restoring);
    return before;
  }
  DeleteFileW(Long(JoinPath(restoring, L"snapshot.json")).c_str());
  const std::wstring replaced = data_dir + L".replaced-" + UtcFileStamp();
  const bool had_data = PathIsDirectory(data_dir);
  if (had_data && !MoveFolder(data_dir, replaced, error)) {
    DeleteDirectoryTree(restoring);
    return before;
  }
  if (!MoveFolder(restoring, data_dir, error)) {
    if (had_data) MoveFileExW(Long(replaced).c_str(), Long(data_dir).c_str(), MOVEFILE_WRITE_THROUGH);
    DeleteDirectoryTree(restoring);
    return before;
  }
  if (had_data) DeleteDirectoryTree(replaced);
  return before;
}

void RecoverInterruptedDataWork(const std::wstring& backups_dir, const std::wstring& data_dir) {
  for (const std::wstring& name : ChildFolders(backups_dir)) {
    if (EndsWith(name, L".partial")) DeleteDirectoryTree(JoinPath(backups_dir, name));
  }
  const std::wstring parent = ParentDirectory(data_dir);
  const std::wstring leaf = FileNameOf(data_dir);
  DeleteDirectoryTree(data_dir + L".restoring");
  for (const std::wstring& name : ChildFolders(parent)) {
    if (name.rfind(leaf + L".replaced-", 0) != 0) continue;
    const std::wstring path = JoinPath(parent, name);
    if (!PathIsDirectory(data_dir)) {
      MoveFileExW(Long(path).c_str(), Long(data_dir).c_str(), MOVEFILE_WRITE_THROUGH);
    } else {
      DeleteDirectoryTree(path);
    }
  }
}

void PruneSnapshots(const std::wstring& backups_dir, std::size_t keep, const std::vector<std::wstring>& protect) {
  std::vector<std::wstring> complete;
  for (const std::wstring& name : ChildFolders(backups_dir)) {
    if (EndsWith(name, L".partial")) continue;
    if (!PathIsFile(JoinPath(JoinPath(backups_dir, name), L"snapshot.json"))) continue;
    complete.push_back(name);
  }
  std::sort(complete.begin(), complete.end());
  const std::size_t remove = complete.size() > keep ? complete.size() - keep : 0;
  for (std::size_t i = 0; i < remove; ++i) {
    const std::wstring path = JoinPath(backups_dir, complete[i]);
    const bool protected_path =
        std::any_of(protect.begin(), protect.end(), [&](const std::wstring& p) { return PathsEqual(p, path); });
    if (!protected_path) DeleteDirectoryTree(path);
  }
}

void PruneVersions(const std::wstring& package_dir, const std::vector<std::wstring>& keep, std::int64_t below_code) {
  for (const std::wstring& name : ChildFolders(package_dir)) {
    if (!AllDigits(name) || name.size() > 18) continue;
    if (std::stoll(name) >= below_code) continue;
    const std::wstring path = JoinPath(package_dir, name);
    const bool kept = std::any_of(keep.begin(), keep.end(), [&](const std::wstring& k) { return PathsEqual(k, path); });
    if (!kept) DeleteDirectoryTree(path);
  }
}

}
