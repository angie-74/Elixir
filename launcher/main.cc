// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "apk_tool.h"
#include "core/bundle.h"
#include "core/installer.h"
#include "core/instance.h"
#include "core/link_request.h"
#include "core/paths.h"
#include "core/process.h"
#include "core/update.h"
#include "core/util.h"
#include "elixir_runtime/host_main.h"
#include "platform/windows/app_user_model_id.h"
#include "ui/app.h"
#if defined(ELIXIR_HAS_WEBVIEW)
#include "webview/webview_host_windows.h"
#endif

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif

namespace {

using namespace elixir::launcher;

constexpr wchar_t kVersion[] = L"" ELIXIR_VERSION;
constexpr int kInstallCancelled = 1602;
constexpr int kInstallFailed = 1603;
constexpr int kCancelled = 1223;

struct StartFlags {
  bool settings = false;
  bool player = false;
  bool uninstall = false;
  bool quiet = false;
  bool upgrade = false;
};

bool IsFlag(std::wstring_view argument, const wchar_t* flag) {
  if (argument == flag) return true;
  return argument.size() > 1 && argument[0] == L'-' && argument.substr(1) == flag;
}

std::optional<LinkRequest> TakeLinkArgument(int argc, LPWSTR* argv) {
  std::optional<LinkRequest> request;
  for (int i = 1; argv != nullptr && i < argc; ++i) {
    if (std::wstring_view(argv[i]) != kOpenLinkFlag) continue;
    request = LinkRequest{};
    if (argc == 3 && i == 1) {
      std::wstring link = argv[2];
      SecureZeroMemory(argv[2], wcslen(argv[2]) * sizeof(wchar_t));
      WipeLinkFromCommandLine();
      *request = ParseLink(link);
      Wipe(&link);
    } else {
      WipeLinkFromCommandLine();
      for (int j = i + 1; j < argc; ++j) SecureZeroMemory(argv[j], wcslen(argv[j]) * sizeof(wchar_t));
    }
    break;
  }
  return request;
}

int RunRuntime(int argc, LPWSTR* argv) {
  std::vector<wchar_t*> arguments;
  arguments.push_back(argv[0]);
  for (int i = 2; i < argc; ++i) arguments.push_back(argv[i]);
  arguments.push_back(nullptr);
  return ElixirRuntimeHostMain(static_cast<int>(arguments.size()) - 1, arguments.data());
}

int RunApkTool(int argc, LPWSTR* argv) {
  std::vector<std::string> arguments;
  for (int i = 2; i < argc; ++i) arguments.push_back(WideToUtf8(argv[i]));
  std::ios::sync_with_stdio(false);
  const int code = elixir::apk_tool::Run(arguments, std::cout, std::cerr);
  std::cout.flush();
  std::cerr.flush();
  return code;
}

int RunWindow(ui::App& app) {
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (app.TranslateLaunchMenu(&message)) continue;
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return static_cast<int>(message.wParam);
}

ui::PromptResult Ask(ui::PromptSpec prompt) {
  ui::AppOptions options;
  options.mode = ui::AppMode::kPrompt;
  options.prompt = std::move(prompt);
  ui::App app(options);
  if (!app.Create()) return {};
  RunWindow(app);
  return app.prompt_result();
}

bool AskYesNo(const std::wstring& text) {
  ui::PromptSpec prompt;
  prompt.text = text;
  prompt.buttons = {L"Yes", L"No"};
  return Ask(std::move(prompt)).button == 0;
}

void Tell(const std::wstring& text) {
  ui::PromptSpec prompt;
  prompt.text = text;
  prompt.buttons = {L"OK"};
  Ask(std::move(prompt));
}

bool StartProgram(const std::wstring& program, const std::vector<std::wstring>& arguments) {
  std::wstring command = BuildCommandLine(program, arguments);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  const std::wstring directory = ParentDirectory(program);
  if (!CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, directory.c_str(),
                      &startup, &process)) {
    return false;
  }
  AllowSetForegroundWindow(process.dwProcessId);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

std::vector<std::wstring> ArgumentsWithout(int argc, LPWSTR* argv, const wchar_t* dropped) {
  std::vector<std::wstring> arguments;
  for (int i = 1; argv != nullptr && i < argc; ++i) {
    if (IsFlag(argv[i], dropped)) continue;
    if (std::wstring_view(argv[i]) == kWaitFlag) {
      ++i;
      continue;
    }
    arguments.emplace_back(argv[i]);
  }
  return arguments;
}

void WaitForProcess(DWORD pid) {
  if (pid == 0 || pid == GetCurrentProcessId()) return;
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (process == nullptr) return;
  WaitForSingleObject(process, 15000);
  CloseHandle(process);
}

struct NamedLock {
  HANDLE handle = nullptr;
  bool owned = false;
  explicit NamedLock(const wchar_t* name) {
    handle = CreateMutexW(nullptr, TRUE, name);
    owned = handle != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
  }
  ~NamedLock() {
    if (handle == nullptr) return;
    if (owned) ReleaseMutex(handle);
    CloseHandle(handle);
  }
};

int RunUninstall(const InstallEnvironment& environment, const std::wstring& location, bool quiet) {
  if (location.empty()) {
    if (!quiet) Tell(L"You must first install Elixir before uninstalling.");
    return kInstallFailed;
  }
  NamedLock lock(L"Local\\Elixir.Uninstaller");
  if (!lock.owned) {
    if (!quiet) Tell(L"Please wait for uninstallation to finish.");
    return 0;
  }
  bool keep_data = true;
  if (!quiet) {
    ui::PromptSpec prompt;
    prompt.heading = L"Uninstall Elixir";
    prompt.text = L"Uninstalling will remove Elixir from your system, and automatically reconfigure the default "
                  L"Roblox launcher if it’s still installed.\n\n"
                  L"The uninstall process may not be able to fully clean up itself, so you may need to manually "
                  L"clean up leftover files where Elixir was installed.\n\n"
                  L"Elixir was installed at “" +
                  location + L"”.";
    prompt.checkbox = L"Keep my settings and mods";
    prompt.checkbox_note =
        L"They’ll be kept where Elixir was installed, and will automatically be restored on a reinstall.";
    prompt.checked = true;
    prompt.buttons = {L"Uninstall", L"Cancel"};
    const ui::PromptResult result = Ask(std::move(prompt));
    if (result.button != 0) return 0;
    keep_data = result.checked;
  }
  const std::wstring installed = InstalledExecutable(location);
  const std::vector<DWORD> running = ProcessesRunning(installed);
  if (!running.empty()) {
    if (!quiet && IsAnyRuntimeProcessRunning()) {
      ui::PromptSpec prompt;
      prompt.text =
          L"Roblox is currently running, but must be closed before uninstalling Elixir. Would you like to close "
          L"Roblox now?";
      prompt.buttons = {L"OK", L"Cancel"};
      if (Ask(std::move(prompt)).button != 0) return kCancelled;
    }
    StopProcesses(running);
  }
  UninstallRequest request;
  request.location = location;
  request.keep_data = keep_data;
  UninstallElixir(request, environment);
  if (PathsEqual(ExecutablePath(), installed)) {
    StartDelayedDelete(location);
  } else {
    DeleteFileW(installed.c_str());
    RemoveDirectoryW(location.c_str());
  }
  if (!quiet) Tell(L"Elixir has successfully uninstalled");
  return 0;
}

int RunInstall(const InstallEnvironment& environment, const StartFlags& flags, int argc, LPWSTR* argv) {
  NamedLock lock(L"Local\\Elixir.Installer");
  if (!lock.owned) {
    if (!flags.quiet) Tell(L"Please wait for installation to finish.");
    return 0;
  }
  if (flags.quiet) {
    InstallRequest request;
    request.source = ExecutablePath();
    request.location = DefaultInstallLocation(environment);
    request.version = kVersion;
    std::wstring error;
    if (!InstallElixir(request, environment, &error)) return kInstallFailed;
    StartProgram(InstalledExecutable(request.location), ArgumentsWithout(argc, argv, kQuietFlag));
    return 0;
  }
  ui::AppOptions options;
  options.mode = ui::AppMode::kInstaller;
  options.install_environment = environment;
  options.install_source = ExecutablePath();
  ui::InstallerOutcome outcome;
  {
    ui::App app(options);
    if (!app.Create()) {
      MessageBoxW(nullptr, L"Elixir couldn’t open its window.", L"Elixir", MB_ICONERROR | MB_OK);
      return kInstallFailed;
    }
    RunWindow(app);
    outcome = app.installer_outcome();
  }
  if (!outcome.finished) return kInstallCancelled;
  const std::wstring installed = InstalledExecutable(outcome.location);
  if (outcome.action == ui::InstallerAction::kSettings) StartProgram(installed, {kSettingsFlag});
  if (outcome.action == ui::InstallerAction::kLaunch) StartProgram(installed, {kPlayerFlag});
  return 0;
}

enum class UpgradeOutcome { kNotNeeded, kUpgraded, kDeclined, kFailed };

UpgradeOutcome HandleUpgrade(const InstallEnvironment& environment, const std::wstring& location,
                             const StartFlags& flags) {
  const std::wstring running = ExecutablePath();
  const std::wstring installed = InstalledExecutable(location);
  if (!PathIsFile(installed)) {
    return CopyFileW(running.c_str(), installed.c_str(), FALSE) ? UpgradeOutcome::kUpgraded : UpgradeOutcome::kFailed;
  }
  const UpgradeNeed need = CheckUpgrade(running, location, kVersion);
  if (!need.differs) return UpgradeOutcome::kNotNeeded;
  const bool automatic = flags.upgrade || IsAutomaticUpgradeSource(running, location, environment);
  if (need.older && !flags.quiet &&
      !AskYesNo(L"The version of Elixir you’ve launched is older than the version you currently have installed.\n"
                L"Issues may occur and your settings may be altered. A reinstall is recommended.\n"
                L"Are you sure you want to continue?")) {
    return UpgradeOutcome::kDeclined;
  }
  if (!automatic && !flags.quiet &&
      !AskYesNo(L"The version of Elixir you’ve launched is different to the version you currently have installed.\n"
                L"Would you like to upgrade your currently installed version?")) {
    return UpgradeOutcome::kDeclined;
  }
  std::wstring error;
  if (!UpgradeInstalledCopy(running, location, kVersion, environment, &error)) {
    if (!flags.quiet) Tell(error);
    return UpgradeOutcome::kFailed;
  }
  if (!flags.quiet && !automatic) Tell(std::wstring(L"Elixir has been upgraded to v") + kVersion);
  return UpgradeOutcome::kUpgraded;
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show_command) {
  {
    int mode_argc = 0;
    LPWSTR* mode_argv = CommandLineToArgvW(GetCommandLineW(), &mode_argc);
    if (mode_argv != nullptr && mode_argc >= 2) {
      const std::wstring_view mode = mode_argv[1];
      if (mode == kRuntimeModeFlag) return RunRuntime(mode_argc, mode_argv);
      if (mode == kApkToolModeFlag) return RunApkTool(mode_argc, mode_argv);
#if defined(ELIXIR_HAS_WEBVIEW)
      if (mode == elixir::kWebViewModeFlag) return elixir::WebViewHostMain(mode_argc, mode_argv);
#endif
    }
    if (mode_argv != nullptr) LocalFree(mode_argv);
  }
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  SetCurrentProcessExplicitAppUserModelID(elixir::kAppUserModelId);

  UseBuiltInPrograms(ExecutablePath());
  ui::AppOptions options;
  options.paths = CurrentLauncherPaths();
  options.show_command = show_command;
  std::wstring initial_page;
  StartFlags flags;
  bool data_dir_given = false;
  DWORD wait_pid = 0;
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  bool has_link = false;
  for (int i = 1; argv != nullptr && i < argc; ++i) {
    const std::wstring_view argument = argv[i];
    if (argument == kOpenLinkFlag) {
      has_link = true;
      break;
    }
    if (IsFlag(argument, kSettingsFlag)) flags.settings = true;
    if (IsFlag(argument, kPlayerFlag)) flags.player = true;
    if (IsFlag(argument, kUninstallFlag)) flags.uninstall = true;
    if (IsFlag(argument, kQuietFlag)) flags.quiet = true;
    if (IsFlag(argument, kUpgradeFlag)) flags.upgrade = true;
    if (i + 1 >= argc) continue;
    if (argument == L"--data-dir") {
      options.paths = LauncherPathsForDataRoot(options.paths.exe_dir, argv[++i], options.paths.portable);
      data_dir_given = true;
    } else if (argument == L"--page") {
      initial_page = argv[++i];
    } else if (argument == kWaitFlag) {
      wait_pid = wcstoul(argv[++i], nullptr, 10);
    }
  }
  WaitForProcess(wait_pid);

  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return 1;
  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&controls);

  if (!options.paths.portable && !data_dir_given) {
    const InstallEnvironment environment = SystemInstallEnvironment();
    const std::wstring location = RegisteredInstallLocation(environment);
    int code = -1;
    if (flags.uninstall) {
      code = RunUninstall(environment, location, flags.quiet);
    } else if (location.empty()) {
      code = RunInstall(environment, flags, argc, argv);
    } else {
      const std::wstring installed = InstalledExecutable(location);
      if (!PathsEqual(ExecutablePath(), installed)) {
        const UpgradeOutcome upgrade = HandleUpgrade(environment, location, flags);
        if (upgrade != UpgradeOutcome::kDeclined && upgrade != UpgradeOutcome::kFailed &&
            StartProgram(installed, ArgumentsWithout(argc, argv, kUpgradeFlag))) {
          code = 0;
        }
      }
      options.paths = LauncherPathsForDataRoot(options.paths.exe_dir, location, false);
      options.installed = true;
      if (code < 0) EnsureInstalledLinks(location, environment);
    }
    if (code >= 0) {
      if (argv != nullptr) LocalFree(argv);
      CoUninitialize();
      return code;
    }
  }

  std::optional<LinkRequest> link = has_link ? TakeLinkArgument(argc, argv) : std::nullopt;
  if (argv != nullptr) LocalFree(argv);
  SetBundleDirectory(options.paths.bundle_dir);
  {
    std::wstring bundle_error;
    if (HasBundledFiles()) EnsureBundledFiles(options.paths.bundle_dir, &bundle_error);
  }
  options.launch_now = !link && flags.player;
  options.compact_start = !link && initial_page.empty() && !flags.settings && !flags.player;
  ui::Page requested_page{};
  const bool have_page = !initial_page.empty() && ui::PageFromKey(WideToUtf8(initial_page), &requested_page);

  InstanceOwner instance;
  const std::wstring instance_key = InstanceKey(options.paths.data_root);
  if (!instance.Acquire(instance_key)) {
    if (link && link->kind == LinkRequestKind::kInvalid) {
      MessageBoxW(nullptr, L"This Roblox link can’t be opened in Elixir.", L"Elixir", MB_ICONINFORMATION | MB_OK);
    }
    InstanceMessage message{InstanceMessageKind::kActivate, {}};
    if (link || !initial_page.empty()) {
      message = {InstanceMessageKind::kOpenPage, have_page ? ui::PageKey(requested_page) : ""};
    }
    if (link && link->kind == LinkRequestKind::kJoin) {
      message = {InstanceMessageKind::kLink, std::move(link->join)};
      link->join.clear();
    }
    InstanceReply reply = InstanceReply::kDenied;
    std::wstring error;
    const bool sent = SendToInstanceOwner(instance_key, message, 30000, &reply, &error);
    const bool was_link = message.kind == InstanceMessageKind::kLink;
    Wipe(&message.payload);
    if (!sent) {
      MessageBoxW(nullptr, was_link ? L"Elixir is open but didn’t answer. Open the link again." : L"Elixir is already open.",
                  L"Elixir", MB_ICONINFORMATION | MB_OK);
    } else if (was_link && reply != InstanceReply::kAccepted) {
      MessageBoxW(nullptr,
                  reply == InstanceReply::kQueueFull ? L"Elixir is busy. Open the link again in a moment."
                                                     : L"This Roblox link can’t be opened in Elixir.",
                  L"Elixir", MB_ICONINFORMATION | MB_OK);
    }
    CoUninitialize();
    return 0;
  }

  CleanupAfterUpdate(options.paths.exe_dir);
  if (!PathsEqual(options.paths.exe_dir, options.paths.data_root)) CleanupAfterUpdate(options.paths.data_root);

  int exit_code = 0;
  {
    ui::App app(options);
    if (!app.Create()) {
      if (link) Wipe(&link->join);
      MessageBoxW(nullptr, L"Elixir couldn’t open its window.", L"Elixir", MB_ICONERROR | MB_OK);
      CoUninitialize();
      return 1;
    }
    if (link) {
      app.OpenLink(std::move(*link));
      link.reset();
    }
    std::wstring listen_error;
    instance.Listen(
        [&app](const InstanceMessage& message) {
          if (message.kind != InstanceMessageKind::kLink) {
            ui::Page target{};
            if (message.kind == InstanceMessageKind::kOpenPage && !message.payload.empty() &&
                !ui::PageFromKey(message.payload, &target)) return InstanceReply::kInvalid;
            app.PostInstanceMessage(message);
            return InstanceReply::kAccepted;
          }
          std::string join = message.payload;
          std::int64_t place_id = 0;
          if (!ValidateJoinRequest(&join, &place_id)) return InstanceReply::kInvalid;
          app.PostInstanceMessage({InstanceMessageKind::kLink, std::move(join)});
          return InstanceReply::kAccepted;
        },
        &listen_error);
    if (have_page) app.Navigate(requested_page);
    exit_code = RunWindow(app);
    instance.Stop();
  }
  CoUninitialize();
  return exit_code;
}
