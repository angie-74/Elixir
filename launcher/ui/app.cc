// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "ui/app.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <windowsx.h>
#undef PathIsDirectory

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <thread>

#include "core/diagnostics.h"
#include "core/fflags_file.h"
#include "core/json.h"
#include "core/process.h"
#include "core/sha256.h"
#include "core/util.h"
#include "resource.h"
#include "ui/theme.h"

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif

namespace elixir::launcher::ui {

namespace {

constexpr wchar_t kWindowClass[] = L"ElixirLauncherWindow";
constexpr UINT kTaskMessage = WM_APP + 1;
constexpr UINT_PTR kAnimationTimer = 1;

std::uint32_t ReadWindowsControlAccent() {
  BYTE palette[32] = {};
  DWORD size = sizeof(palette);
  constexpr int kAccentOffset = 4;
  if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
                   L"AccentPalette", RRF_RT_REG_BINARY, nullptr, palette, &size) == ERROR_SUCCESS &&
      size >= kAccentOffset + 4) {
    const BYTE* color = palette + kAccentOffset;
    return (static_cast<std::uint32_t>(color[0]) << 16) | (static_cast<std::uint32_t>(color[1]) << 8) | color[2];
  }

  DWORD colorization = 0;
  BOOL opaque = FALSE;
  if (FAILED(DwmGetColorizationColor(&colorization, &opaque))) return kDefaultControlAccentRgb;
  const auto light2 = [colorization](unsigned shift) {
    const auto channel = (colorization >> shift) & 0xFFu;
    return ((channel + 256u) / 2u) << shift;
  };
  return light2(16) | light2(8) | light2(0);
}

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
constexpr UINT_PTR kToastTimer = 2;
constexpr int kEditorId = 100;
constexpr wchar_t kSettingsNotSaved[] = L"Make sure your drive has free space, then try again.";

std::wstring SettingsSaveError(const std::wstring& cause) {
  return cause.empty() ? std::wstring(kSettingsNotSaved) : cause + L" Try again.";
}

struct Task {
  App* target;
  std::function<void()> run;
};

void PostTask(HWND hwnd, App* target, std::function<void()> run) {
  auto* task = new Task{target, std::move(run)};
  if (hwnd == nullptr || !PostMessageW(hwnd, kTaskMessage, 0, reinterpret_cast<LPARAM>(task))) delete task;
}

COLORREF ToColorRef(std::uint32_t rgb) {
  return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

bool Contains(const D2D1_RECT_F& rect, D2D1_POINT_2F point, float slack_x = 0.0f) {
  return point.x >= rect.left - slack_x && point.x < rect.right + slack_x && point.y >= rect.top &&
         point.y < rect.bottom;
}

bool IsTabStop(const ControlRecord& record) {
  if (!record.focusable || !record.enabled || record.kind == ControlKind::kStatic) return false;
  return true;
}

HICON LoadAppIcon(int size) {
  return static_cast<HICON>(
      LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ELIXIR), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
}

}

const wchar_t* PageTitle(Page page) {
  switch (page) {
    case Page::kInstall: return L"Installation";
    case Page::kFastFlags: return L"Fast Flags";
    case Page::kGlobalSettings: return L"Global Settings";
    case Page::kIntegrations: return L"Integrations";
    case Page::kSettings: return L"Settings";
    case Page::kCrashLogs: return L"Crash logs";
    case Page::kUpdates: return L"Updates";
    case Page::kAbout: return L"About";
    case Page::kMods: return L"Mods";
    case Page::kShortcuts: return L"Shortcuts";
  }
  return L"";
}

const char* PageKey(Page page) {
  switch (page) {
    case Page::kInstall: return "install";
    case Page::kFastFlags: return "fast-flags";
    case Page::kGlobalSettings: return "global-settings";
    case Page::kIntegrations: return "integrations";
    case Page::kSettings: return "settings";
    case Page::kCrashLogs: return "crash-logs";
    case Page::kUpdates: return "updates";
    case Page::kAbout: return "about";
    case Page::kMods: return "mods";
    case Page::kShortcuts: return "shortcuts";
  }
  return "install";
}

SIZE MinimumWindowSize(UINT dpi, const RECT& work_area) {
  RECT frame{0, 0, static_cast<LONG>(theme::kMinWindowWidth * dpi / 96.0f),
             static_cast<LONG>(theme::kMinWindowHeight * dpi / 96.0f)};
  AdjustWindowRectExForDpi(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
  const LONG work_width = std::max<LONG>(1, work_area.right - work_area.left);
  const LONG work_height = std::max<LONG>(1, work_area.bottom - work_area.top);
  return SIZE{std::min(frame.right - frame.left, work_width), std::min(frame.bottom - frame.top, work_height)};
}

bool PageFromKey(std::string_view key, Page* page) {
  for (int i = 0; i < kPageCount; ++i) {
    if (key == PageKey(static_cast<Page>(i))) {
      *page = static_cast<Page>(i);
      return true;
    }
  }
  return false;
}

App::App(AppOptions options)
    : options_(std::move(options)), paths_(options_.paths), store_(paths_.config_file) {
  if (options_.mode != AppMode::kLauncher) {
    RefreshControlAccent();
    RefreshMotionSetting();
    installer_.location = DefaultInstallLocation(options_.install_environment);
    installer_.data_found = ExistingDataPresent(installer_.location);
    installer_.prompt_checked = options_.prompt.checked;
    prompt_result_.checked = options_.prompt.checked;
    return;
  }
  std::wstring error;
  if (!EnsureLauncherDirectories(paths_, &error)) save_error_ = SettingsSaveError(error);
  load_result_ = store_.Load();
  config_ = load_result_.config;
  load_notice_ = load_result_.outcome == LoadOutcome::kRecoveredFromBackup ||
                 load_result_.outcome == LoadOutcome::kResetToDefaults || !load_result_.saved;
  config_.graphics.present_mode = PresentMode::kFifo;
  config_.performance.high_priority = false;
  config_.performance.disable_power_throttling = true;
  config_.performance.timer_resolution_1ms = true;
  config_.performance.mmcss = true;
  RefreshControlAccent();
  RefreshMotionSetting();
  apk_ = LocateApkComponent(paths_.exe_dir);
  if (!options_.test_mode) PageFromKey(config_.last_page, &page_);
  CleanupApkWorkDirectory(JoinPath(paths_.data_root, L"tmp"));
}

App::~App() {
  StopFrameClock();
  if (install_.cancel) *install_.cancel = true;
  if (update_.cancel) *update_.cancel = true;
  if (roblox_version_.pending) *roblox_version_.pending = true;
  if (roblox_update_.cancel) *roblox_update_.cancel = true;
  if (roblox_list_.cancel) *roblox_list_.cancel = true;
  if (hwnd_ != nullptr) DestroyWindow(hwnd_);
  if (runtime_.process != nullptr) CloseHandle(runtime_.process);
  if (editor_font_ != nullptr) DeleteObject(editor_font_);
  if (field_brush_ != nullptr) DeleteObject(field_brush_);
}

bool App::Create() {
  renderer_ok_ = renderer_.Initialize();
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &App::WindowProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hIcon = LoadAppIcon(GetSystemMetrics(SM_CXICON));
    window_class.hIconSm = LoadAppIcon(GetSystemMetrics(SM_CXSMICON));
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    registered = true;
  }

  RECT work{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  const UINT dpi = GetDpiForSystem();
  const DWORD style = WindowStyleForMode();
  const SIZE wanted = WindowSizeForMode(dpi, style);
  RECT frame{0, 0, wanted.cx, wanted.cy};
  int width = frame.right - frame.left;
  int height = frame.bottom - frame.top;
  width = std::min(width, static_cast<int>(work.right - work.left));
  height = std::min(height, static_cast<int>(work.bottom - work.top));
  int x = work.left + ((work.right - work.left) - width) / 2;
  int y = work.top + ((work.bottom - work.top) - height) / 2;
  if (options_.offscreen) {
    x = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) + 64;
    y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    width = frame.right - frame.left;
    height = frame.bottom - frame.top;
  }
  const wchar_t* title = options_.mode == AppMode::kInstaller ? L"Elixir Installer" : L"Elixir";
  hwnd_ = CreateWindowExW(0, kWindowClass, title, style, x, y, width, height, nullptr, nullptr,
                          GetModuleHandleW(nullptr), this);
  if (hwnd_ == nullptr) return false;
  if (options_.mode != AppMode::kLauncher) {
    ShowWindow(hwnd_, options_.offscreen ? SW_SHOWNOACTIVATE : SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    if (!options_.offscreen) SetForegroundWindow(hwnd_);
    return true;
  }
  if (options_.launch_now) {
    StartLauncherServices();
    if (!overlay_.open) {
      if (!config_.installed.present() && RobloxUpdatePossible()) {
        play_after_roblox_install_ = true;
        page_ = Page::kInstall;
        StartRobloxUpdate(false);
      } else {
        PlayFromMenu();
        return true;
      }
    }
    if (!runtime_.running) {
      if (overlay_.open) play_from_menu_ = false;
      ShowWindow(hwnd_, SW_SHOWNORMAL);
      UpdateWindow(hwnd_);
      SetForegroundWindow(hwnd_);
    }
    return true;
  }
  if (options_.compact_start) {
    return launch_menu_.Create([this](LaunchMenuAction action) { ChooseLaunchAction(action); }, options_.show_command);
  }
  ShowWindow(hwnd_, options_.offscreen ? SW_SHOWNOACTIVATE : options_.show_command);
  UpdateWindow(hwnd_);
  StartLauncherServices();
  return true;
}

DWORD App::WindowStyleForMode() const {
  if (options_.mode == AppMode::kPrompt) return WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
  return WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
}

SIZE App::WindowSizeForMode(UINT dpi, DWORD style) const {
  if (options_.mode == AppMode::kInstaller) return InstallerWindowSize(dpi);
  if (options_.mode == AppMode::kPrompt) return PromptWindowSize(dpi);
  RECT frame{0, 0, static_cast<LONG>(theme::kWindowWidth * dpi / 96.0f),
             static_cast<LONG>(theme::kWindowHeight * dpi / 96.0f)};
  AdjustWindowRectExForDpi(&frame, style, FALSE, 0, dpi);
  return SIZE{frame.right - frame.left, frame.bottom - frame.top};
}

void App::StartLauncherServices() {
  if (services_started_) return;
  services_started_ = true;
  RefreshAudioDevices();
  RefreshCrashLogs();
  if (page_ == Page::kFastFlags) LoadFastFlags(true);
  if (page_ == Page::kGlobalSettings || page_ == Page::kFastFlags) RefreshGlobalSettings();
  if (page_ == Page::kSettings) MeasureCaches();
  if (page_ == Page::kMods) RefreshMods();
  if (page_ == Page::kShortcuts) RefreshShortcuts();
  if (!options_.test_mode) {
    if ((options_.installed && config_.updates.auto_update) || page_ == Page::kUpdates) CheckForUpdates();
    const std::wstring staging = JoinPath(paths_.updates_dir, L"staging");
    if (!StartsWithI(ExecutablePath(), staging)) DeleteDirectoryTree(staging);
    RecoverInterruptedDataWork(AppBackupsDirectory(BackupsRoot(), kRobloxPackage),
                               AndroidAppDataDirectory(paths_, kRobloxPackage));
    UpdateGameShortcutsNow(false);
    if ((config_.roblox.check_for_new_version || config_.roblox.auto_update) &&
        config_.installed.package == kRobloxPackage) {
      CheckRobloxVersion( true);
    }
  }
}

void App::ChooseLaunchAction(LaunchMenuAction action) {
  if (action == LaunchMenuAction::kClose) {
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    return;
  }
  if (action == LaunchMenuAction::kConfigure) {
    BringToFront();
    return;
  }
  ShowWindow(launch_menu_.hwnd(), SW_HIDE);
  StartLauncherServices();
  if (overlay_.open) {
    BringToFront();
    return;
  }
  PlayFromMenu();
}

void App::PlayFromMenu() {
  if (UpdateBeforePlaying()) return;
  play_from_menu_ = true;
  Play();
  if (!runtime_.running) {
    if (overlay_.open) play_from_menu_ = false;
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
  }
}

LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  App* app = nullptr;
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    app = static_cast<App*>(create->lpCreateParams);
    app->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
  } else {
    app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (app == nullptr) return DefWindowProcW(hwnd, message, wparam, lparam);
  const LRESULT result = app->HandleMessage(message, wparam, lparam);
  if (message == WM_NCDESTROY) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    app->hwnd_ = nullptr;
  }
  return result;
}

LRESULT CALLBACK App::EditorProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id,
                                 DWORD_PTR data) {
  App* app = reinterpret_cast<App*>(data);
  switch (message) {
    case WM_KEYDOWN:
      if (wparam == VK_RETURN) {
        if (app->state_.editing == L"nav.search") app->search_enter_ = true;
        app->EndEdit(true);
        return 0;
      }
      if (wparam == VK_ESCAPE) {
        if (app->state_.editing == L"nav.search") {
          app->ClearSearch();
          SetWindowTextW(hwnd, L"");
          app->EndEdit(false);
          app->Invalidate();
          return 0;
        }
        SetWindowTextW(hwnd, app->editing_original_.c_str());
        app->EndEdit(false);
        return 0;
      }
      if (wparam == VK_TAB) {
        const bool backwards = GetKeyState(VK_SHIFT) < 0;
        app->EndEdit(true);
        app->MoveFocus(!backwards);
        return 0;
      }
      break;
    case WM_CHAR:
      if (wparam == L'\r' || wparam == L'\t' || wparam == 27) return 0;
      break;
    case WM_NCDESTROY:
      RemoveWindowSubclass(hwnd, &App::EditorProc, id);
      break;
    default:
      break;
  }
  return DefSubclassProc(hwnd, message, wparam, lparam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CREATE:
      dpi_ = GetDpiForWindow(hwnd_);
      ApplyBackdrop();
      CreateNativeChildren();
      if (options_.mode == AppMode::kLauncher) DragAcceptFiles(hwnd_, TRUE);
      if (dpi_ != GetDpiForSystem() && !options_.offscreen) {
        const SIZE wanted = WindowSizeForMode(dpi_, WindowStyleForMode());
        LONG width = wanted.cx;
        LONG height = wanted.cy;
        MONITORINFO monitor{sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitor)) {
          width = std::min(width, monitor.rcWork.right - monitor.rcWork.left);
          height = std::min(height, monitor.rcWork.bottom - monitor.rcWork.top);
        }
        SetWindowPos(hwnd_, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_DWMCOLORIZATIONCOLORCHANGED:
      RefreshControlAccent();
      break;
    case WM_SETTINGCHANGE:
      RefreshMotionSetting();
      if (lparam != 0 && std::wstring_view(reinterpret_cast<const wchar_t*>(lparam)) == L"ImmersiveColorSet") {
        RefreshControlAccent();
      }
      break;
    case WM_PAINT:
      Paint();
      return 0;
    case WM_SIZE:
      renderer_.Resize(LOWORD(lparam), HIWORD(lparam));
      Invalidate();
      return 0;
    case WM_DPICHANGED: {
      dpi_ = HIWORD(wparam);
      renderer_.SetDpi(dpi_);
      UpdateNativeFonts();
      const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
      SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                   suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
      Invalidate();
      return 0;
    }
    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
      const UINT dpi = hwnd_ != nullptr ? GetDpiForWindow(hwnd_) : 96;
      MONITORINFO monitor{sizeof(monitor)};
      RECT work{0, 0, 1 << 15, 1 << 15};
      if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitor)) work = monitor.rcWork;
      SIZE minimum = MinimumWindowSize(dpi, work);
      if (options_.mode == AppMode::kInstaller) {
        const SIZE installer = InstallerWindowSize(dpi);
        minimum.cx = std::min<LONG>(installer.cx, work.right - work.left);
        minimum.cy = std::min<LONG>(installer.cy * 3 / 4, work.bottom - work.top);
      } else if (options_.mode == AppMode::kPrompt) {
        return DefWindowProcW(hwnd_, message, wparam, lparam);
      }
      info->ptMinTrackSize.x = minimum.cx;
      info->ptMinTrackSize.y = minimum.cy;
      return 0;
    }
    case WM_MOUSEMOVE:
      OnMouseMove(ToDips(lparam));
      return 0;
    case WM_MOUSELEAVE:
      tracking_mouse_ = false;
      if (!state_.hovered.empty()) {
        state_.hovered.clear();
        Invalidate();
      }
      return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
      OnMouseDown(ToDips(lparam));
      return 0;
    case WM_LBUTTONUP:
      OnMouseUp(ToDips(lparam));
      return 0;
    case WM_CAPTURECHANGED:
      if (reinterpret_cast<HWND>(lparam) != hwnd_) {
        drag_slider_.clear();
        if (!state_.pressed.empty()) {
          state_.pressed.clear();
          Invalidate();
        }
      }
      return 0;
    case WM_MOUSEWHEEL: {
      if (overlay_.open) return 0;
      if (menu_focus_ >= 0) CloseMenu();
      const float notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
      POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      ScreenToClient(hwnd_, &point);
      const ControlRecord* hit =
          HitTest(D2D1::Point2F(static_cast<float>(point.x) / scale(), static_cast<float>(point.y) / scale()));
      if (hit != nullptr && hit->kind == ControlKind::kTextView &&
          ((notches < 0 && hit->slider_value < hit->slider_max) || (notches > 0 && hit->slider_value > 0))) {
        Dispatch(Interaction{Interaction::Type::kStep, hit->id, 0.0, static_cast<int>(std::lround(-notches * 3.0f)), {}});
      } else {
        UINT lines = 3;
        if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0)) lines = 3;
        const float per_notch = lines == WHEEL_PAGESCROLL ? (viewport_.bottom - viewport_.top) * 0.85f
                                                          : static_cast<float>(lines) * 64.0f / 3.0f;
        ScrollBy(-notches * per_notch);
      }
      return 0;
    }
    case WM_SETCURSOR:
      if (LOWORD(lparam) == HTCLIENT) {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(hwnd_, &point);
        const ControlRecord* hit =
            HitTest(D2D1::Point2F(static_cast<float>(point.x) / scale(), static_cast<float>(point.y) / scale()));
        LPCWSTR cursor = IDC_ARROW;
        if (hit != nullptr && hit->enabled && hit->kind == ControlKind::kLink) cursor = IDC_HAND;
        if (hit != nullptr && hit->enabled && hit->kind == ControlKind::kTextField) cursor = IDC_IBEAM;
        SetCursor(LoadCursorW(nullptr, cursor));
        return TRUE;
      }
      break;
    case WM_KEYDOWN:
      OnKeyDown(wparam);
      return 0;
    case WM_SYSKEYDOWN:
      if ((wparam == VK_DOWN || wparam == VK_UP) && (FocusedIsComboBox() || menu_focus_ >= 0)) {
        OnKeyDown(VK_F4);
        return 0;
      }
      break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
      if (message == WM_KILLFOCUS) {
        state_.pressed.clear();
        if (menu_focus_ >= 0) CloseMenu();
      }
      Invalidate();
      return 0;
    case WM_ACTIVATE:
      if (LOWORD(wparam) != WA_INACTIVE) {
        if (page_ == Page::kCrashLogs) RefreshCrashLogs();
        if (page_ == Page::kFastFlags) LoadFastFlags(false);
        if (page_ == Page::kGlobalSettings || page_ == Page::kFastFlags) RefreshGlobalSettings();
        if (page_ == Page::kSettings) MeasureCaches();
        if (page_ == Page::kMods) RefreshMods();
        if (page_ == Page::kShortcuts) RefreshShortcuts();
      }
      Invalidate();
      break;
    case WM_TIMER:
      if (wparam == kAnimationTimer) {
        InvalidateRect(hwnd_, nullptr, FALSE);
      } else if (wparam == kToastTimer) {
        KillTimer(hwnd_, kToastTimer);
        toast_.clear();
        Invalidate();
      } else if (wparam == kFastFlagsSaveTimer) {
        SaveFastFlagsIfChanged();
      } else if (wparam == kConfigSaveTimer) {
        SaveConfigNow();
      } else if (wparam == kLaunchProgressTimer) {
        CloseLaunchProgress();
      } else if (wparam == kUpdateWaitTimer) {
        KillTimer(hwnd_, kUpdateWaitTimer);
        if (update_.play_when_checked) {
          update_.play_when_checked = false;
          CloseLaunchProgress();
          PlayFromMenu();
        }
      } else if (wparam == kCacheMeasureTimer) {
        if (page_ != Page::kSettings) {
          KillTimer(hwnd_, kCacheMeasureTimer);
        } else if (!IsIconic(hwnd_)) {
          MeasureCaches();
        }
      }
      return 0;
    case kTaskMessage: {
      std::unique_ptr<Task> task(reinterpret_cast<Task*>(lparam));
      if (task->target == this && task->run) task->run();
      return 0;
    }
    case WM_COMMAND:
      if (reinterpret_cast<HWND>(lparam) == editor_ && HIWORD(wparam) == EN_KILLFOCUS) EndEdit(true);
      if (reinterpret_cast<HWND>(lparam) == editor_ && HIWORD(wparam) == EN_CHANGE && state_.editing == L"nav.search") {
        const int length = GetWindowTextLengthW(editor_);
        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
        GetWindowTextW(editor_, text.data(), length + 1);
        text.resize(static_cast<std::size_t>(length));
        search_text_ = text;
        UpdateSearch(search_text_);
        Invalidate();
      }
      return 0;
    case WM_DROPFILES: {
      HDROP drop = reinterpret_cast<HDROP>(wparam);
      wchar_t file[MAX_PATH * 4] = {};
      const bool got = DragQueryFileW(drop, 0, file, static_cast<UINT>(std::size(file))) > 0;
      DragFinish(drop);
      if (!got || overlay_.open) return 0;
      const std::wstring extension = ExtensionOf(file);
      if (extension != L".apk" && extension != L".apkm" && extension != L".xapk") {
        ShowToast(L"That file isn’t Roblox.");
      } else if (!apk_.configured || !apk_.found) {
        Navigate(Page::kInstall);
        ShowToast(L"Part of Elixir is missing. Reinstall Elixir, then try again.");
      } else {
        ValidatePackage(file);
      }
      return 0;
    }
    case WM_CTLCOLOREDIT:
      if (reinterpret_cast<HWND>(lparam) == editor_) {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, ToColorRef(theme::kTextRgb));
        SetBkColor(dc, ToColorRef(theme::kFieldRgb));
        return reinterpret_cast<LRESULT>(field_brush_);
      }
      break;
    case WM_GETOBJECT:
      if (static_cast<LONG>(lparam) == static_cast<LONG>(OBJID_CLIENT)) {
        if (accessible_ == nullptr) accessible_ = new AccessibleRoot(this);
        return LresultFromObject(IID_IAccessible, wparam, accessible_);
      }
      break;
    case WM_CLOSE:
      if (options_.mode == AppMode::kInstaller && !ConfirmInstallerClose()) return 0;
      if (options_.mode == AppMode::kLauncher && GuardFastFlags(std::nullopt, true)) return 0;
      DestroyWindow(hwnd_);
      return 0;
    case WM_DESTROY:
      if (services_started_) {
        config_.last_page = PageKey(page_);
        SaveConfigNow();
      }
      for (ExtraWindow& window : extra_windows_) CloseHandle(window.process);
      extra_windows_.clear();
      KillTimer(hwnd_, kAnimationTimer);
      StopFrameClock();
      animation_interval_ = 0;
      animation_timer_ = false;
      KillTimer(hwnd_, kToastTimer);
      SaveFastFlagsIfChanged();
      if (accessible_ != nullptr) {
        accessible_->Detach();
        accessible_->Release();
        accessible_ = nullptr;
      }
      if (!options_.test_mode) PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void App::ApplyBackdrop() {
  BOOL dark = TRUE;
  if (FAILED(DwmSetWindowAttribute(hwnd_, 20 , &dark, sizeof(dark)))) {
    DwmSetWindowAttribute(hwnd_, 19 , &dark, sizeof(dark));
  }
  HIGHCONTRASTW contrast{};
  contrast.cbSize = sizeof(contrast);
  SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
  const bool high_contrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
  const DWORD build = CollectWindowsInfo().build;
  mica_ = false;
  if (options_.backdrop == Backdrop::kAuto && !high_contrast) {
    if (build >= 22621) {
      const int backdrop = 2;
      mica_ = SUCCEEDED(DwmSetWindowAttribute(hwnd_, 38 , &backdrop, sizeof(backdrop)));
    } else if (build >= 22000) {
      const BOOL mica = TRUE;
      mica_ = SUCCEEDED(DwmSetWindowAttribute(hwnd_, 1029, &mica, sizeof(mica)));
    }
  }
  if (mica_) {
    const MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);
  } else {
    const COLORREF caption = ToColorRef(theme::kWindowRgb);
    DwmSetWindowAttribute(hwnd_, 35 , &caption, sizeof(caption));
  }
  const int corners = 2;
  DwmSetWindowAttribute(hwnd_, 33 , &corners, sizeof(corners));
}

void App::CreateNativeChildren() {
  HINSTANCE instance = GetModuleHandleW(nullptr);
  editor_ = CreateWindowExW(WS_EX_LAYERED, WC_EDITW, L"", WS_CHILD | ES_AUTOHSCROLL | ES_LEFT, 0, 0, 10, 10, hwnd_,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditorId)), instance, nullptr);
  if (editor_ != nullptr) SetLayeredWindowAttributes(editor_, 0, 255, LWA_ALPHA);
  SetWindowSubclass(editor_, &App::EditorProc, 1, reinterpret_cast<DWORD_PTR>(this));
  SendMessageW(editor_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
  SendMessageW(editor_, EM_SETLIMITTEXT, 2048, 0);
  field_brush_ = CreateSolidBrush(ToColorRef(theme::kFieldRgb));
  UpdateNativeFonts();
}

void App::UpdateNativeFonts() {
  const auto make_font = [&](const std::wstring& family, const wchar_t* fallback, int tenth_points) {
    const int height = -MulDiv(tenth_points, static_cast<int>(dpi_), 960);
    HFONT font = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, family.c_str());
    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, font);
    wchar_t face[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, face);
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);
    if (_wcsicmp(face, family.c_str()) != 0) {
      DeleteObject(font);
      font = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, fallback);
    }
    return font;
  };
  if (editor_font_ != nullptr) DeleteObject(editor_font_);
  editor_font_ = make_font(renderer_.text_family().empty() ? std::wstring(L"Segoe UI") : renderer_.text_family(),
                           L"Segoe UI", 140);
  if (editor_ != nullptr) SendMessageW(editor_, WM_SETFONT, reinterpret_cast<WPARAM>(editor_font_), TRUE);
}

D2D1_POINT_2F App::ToDips(LPARAM lparam) const {
  return D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lparam)) / scale(),
                       static_cast<float>(GET_Y_LPARAM(lparam)) / scale());
}

void App::Invalidate() {
  if (hwnd_ != nullptr) InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::PaintNow() {
  if (hwnd_ != nullptr) RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void App::Paint() {
  PAINTSTRUCT paint{};
  BeginPaint(hwnd_, &paint);
  bool animate = false;
  if (renderer_ok_ && renderer_.EnsureTarget(hwnd_, dpi_)) {
    if (animation_timer_) state_.animation = static_cast<float>(GetTickCount64() - animation_start_) / 1000.0f;
    animator_.BeginFrame(NowMs(), motion_enabled_ && !options_.test_mode);
    state_.animator = &animator_;
    AdvanceScroll();
    ID2D1RenderTarget* target = renderer_.target();
    target->BeginDraw();
    target->SetTransform(D2D1::Matrix3x2F::Identity());
    target->Clear(mica_ ? D2D1::ColorF(0, 0, 0, 0) : theme::kWindow);
    records_.clear();
    Ui ui(&renderer_, &state_, &records_, nullptr);
    BuildFrame(ui);
    ui.Flush();
    const HRESULT hr = target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
      renderer_.DiscardTarget();
      InvalidateRect(hwnd_, nullptr, FALSE);
    }
    regions_ = ui.native_regions();
    animate = ui.animating();
    state_.animator = nullptr;
    const bool gliding = scroll_[static_cast<int>(page_)] != scroll_target_[static_cast<int>(page_)];
    motion_active_ = animator_.EndFrame() || gliding;
  }
  EndPaint(hwnd_, &paint);
  if (!search_anchor_.empty()) {
    const std::wstring anchor = std::move(search_anchor_);
    search_anchor_.clear();
    for (const ControlRecord& record : records_) {
      if (record.id == anchor || record.id.rfind(anchor + L"#", 0) == 0 ||
          (anchor.back() == L'.' && record.id.rfind(anchor, 0) == 0)) {
        ScrollIntoView(record.full_rect);
        SetFocusTo(record.id, true);
        break;
      }
    }
  }
  PositionNativeChildren();
  UpdateAnimationTimer(animate && !IsIconic(hwnd_), motion_active_ && !IsIconic(hwnd_));
  if (!deferred_.empty()) Post([this] { RunDeferred(); });
}

void App::SetScroll(float scroll) {
  scroll_[static_cast<int>(page_)] = std::max(0.0f, scroll);
  scroll_target_[static_cast<int>(page_)] = scroll_[static_cast<int>(page_)];
  Invalidate();
}

bool App::RenderOffscreen(UINT dpi, float width, float height, const std::wstring& png_path, OffscreenFrame* frame,
                          std::wstring* error) {
  const auto fail = [&](const wchar_t* what, HRESULT hr) {
    if (error != nullptr) *error = std::wstring(what) + L": " + HResultText(hr);
    return false;
  };
  if (!renderer_ok_ || renderer_.factory() == nullptr || renderer_.wic() == nullptr) {
    return fail(L"Direct2D/WIC unavailable", E_FAIL);
  }
  const float s = static_cast<float>(dpi) / 96.0f;
  const UINT width_px = static_cast<UINT>(std::lround(width * s));
  const UINT height_px = static_cast<UINT>(std::lround(height * s));
  ComPtr<IWICBitmap> bitmap;
  HRESULT hr = renderer_.wic()->CreateBitmap(width_px, height_px, GUID_WICPixelFormat32bppPBGRA,
                                             WICBitmapCacheOnLoad, &bitmap);
  if (FAILED(hr)) return fail(L"CreateBitmap", hr);
  ComPtr<ID2D1RenderTarget> target;
  hr = renderer_.factory()->CreateWicBitmapRenderTarget(
      bitmap.Get(),
      D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                   D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                   static_cast<float>(dpi), static_cast<float>(dpi)),
      &target);
  if (FAILED(hr)) return fail(L"CreateWicBitmapRenderTarget", hr);
  target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

  const UINT saved_dpi = dpi_;
  const bool saved_mica = mica_;
  const D2D1_RECT_F saved_viewport = viewport_;
  const auto saved_heights = content_height_;
  const auto saved_scroll = scroll_;
  const auto saved_scroll_target = scroll_target_;
  const float saved_viewer_scroll = crash_.scroll;
  dpi_ = dpi;
  mica_ = false;
  frame_size_override_ = D2D1::SizeF(width, height);
  renderer_.BeginOffscreen(target.Get(), static_cast<float>(dpi));
  OffscreenFrame local;
  OffscreenFrame& out = frame != nullptr ? *frame : local;
  out = OffscreenFrame{};
  out.width = width;
  out.height = height;
  for (int pass = 0; pass < 2; ++pass) {
    out.controls.clear();
    out.layout = UiLayoutLog{};
    target->BeginDraw();
    target->SetTransform(D2D1::Matrix3x2F::Identity());
    target->Clear(theme::kWindow);
    Ui ui(&renderer_, &state_, &out.controls, nullptr);
    ui.SetLayoutLog(&out.layout);
    BuildFrame(ui);
    ui.Flush();
    hr = target->EndDraw();
  }
  out.viewport = viewport_;
  renderer_.EndOffscreen();
  frame_size_override_ = D2D1::SizeF(0.0f, 0.0f);
  dpi_ = saved_dpi;
  mica_ = saved_mica;
  viewport_ = saved_viewport;
  content_height_ = saved_heights;
  scroll_ = saved_scroll;
  scroll_target_ = saved_scroll_target;
  crash_.scroll = saved_viewer_scroll;
  if (FAILED(hr)) return fail(L"EndDraw", hr);
  if (png_path.empty()) return true;

  ComPtr<IWICStream> stream;
  ComPtr<IWICBitmapEncoder> encoder;
  ComPtr<IWICBitmapFrameEncode> encoded;
  if (FAILED(hr = renderer_.wic()->CreateStream(&stream)) ||
      FAILED(hr = stream->InitializeFromFilename(png_path.c_str(), GENERIC_WRITE)) ||
      FAILED(hr = renderer_.wic()->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
      FAILED(hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
      FAILED(hr = encoder->CreateNewFrame(&encoded, nullptr)) || FAILED(hr = encoded->Initialize(nullptr)) ||
      FAILED(hr = encoded->SetResolution(dpi, dpi)) || FAILED(hr = encoded->WriteSource(bitmap.Get(), nullptr)) ||
      FAILED(hr = encoded->Commit()) || FAILED(hr = encoder->Commit())) {
    return fail(L"PNG encoding", hr);
  }
  return true;
}

void App::UpdateAnimationTimer(bool progress, bool motion) {
  const bool vsync = motion;
  if (vsync) RequestFrame();
  const UINT interval = vsync ? 0 : motion ? USER_TIMER_MINIMUM : progress ? 33 : 0;
  if (interval != animation_interval_) {
    if (interval == 0) {
      KillTimer(hwnd_, kAnimationTimer);
    } else {
      SetTimer(hwnd_, kAnimationTimer, interval, nullptr);
    }
    animation_interval_ = interval;
  }
  const bool active = vsync || interval != 0;
  if (active && !animation_timer_) animation_start_ = GetTickCount64();
  animation_timer_ = active;
}

void App::RequestFrame() {
  if (frame_event_ == nullptr) frame_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!frame_clock_run_.exchange(true)) {
    const HWND hwnd = hwnd_;
    const HANDLE event = frame_event_;
    frame_clock_ = std::thread([this, hwnd, event] {
      while (frame_clock_run_.load()) {
        WaitForSingleObject(event, INFINITE);
        if (!frame_clock_run_.load()) break;
        DwmFlush();
        PostMessageW(hwnd, WM_TIMER, kAnimationTimer, 0);
      }
    });
  }
  SetEvent(frame_event_);
}

void App::StopFrameClock() {
  if (frame_clock_run_.exchange(false)) {
    SetEvent(frame_event_);
    if (frame_clock_.joinable()) frame_clock_.join();
  }
  if (frame_event_ != nullptr) {
    CloseHandle(frame_event_);
    frame_event_ = nullptr;
  }
}

void App::RefreshMotionSetting() {
  BOOL enabled = TRUE;
  if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) enabled = TRUE;
  if (motion_enabled_ != (enabled != FALSE)) {
    motion_enabled_ = enabled != FALSE;
    Invalidate();
  }
}

void App::RefreshControlAccent() {
  const auto accent = options_.control_accent_probe ? options_.control_accent_probe() : ReadWindowsControlAccent();
  if (accent == state_.control_accent_rgb) return;
  state_.control_accent_rgb = accent;
  Invalidate();
}

void App::ShowToast(std::wstring text) {
  toast_ = std::move(text);
  if (hwnd_ != nullptr) SetTimer(hwnd_, kToastTimer, 3500, nullptr);
  Invalidate();
}

const ControlRecord* App::HitTest(D2D1_POINT_2F point) const {
  for (auto it = records_.rbegin(); it != records_.rend(); ++it) {
    if (it->kind == ControlKind::kStatic || it->offscreen) continue;
    if (Contains(it->rect, point, it->kind == ControlKind::kSlider ? 12.0f : 0.0f)) return &*it;
  }
  return nullptr;
}

const ControlRecord* App::FindRecord(const std::wstring& id) const {
  if (id.empty()) return nullptr;
  for (const ControlRecord& record : records_) {
    if (record.id == id) return &record;
  }
  return nullptr;
}

void App::Dispatch(const Interaction& interaction) {
  {
    Ui ui(&renderer_, &state_, nullptr, &interaction);
    BuildFrame(ui);
    if (!ui.requested_focus().empty()) state_.focused = ui.requested_focus();
  }
  RunDeferred();
  Invalidate();
}

void App::RunDeferred() {
  std::vector<std::function<void()>> actions;
  actions.swap(deferred_);
  for (auto& action : actions) action();
  if (!actions.empty()) Invalidate();
}

bool App::Activate(const std::wstring& id) {
  const ControlRecord* record = FindRecord(id);
  if (record != nullptr) {
    const ControlRecord copy = *record;
    if (IsTabStop(copy)) state_.focused = copy.id;
    ActivateRecord(copy);
    return true;
  }
  Dispatch(Interaction{Interaction::Type::kActivate, id, 0.0, 0, {}});
  return false;
}

bool App::EnterText(const std::wstring& id, const std::wstring& text, bool enter) {
  const ControlRecord* record = FindRecord(id);
  if (record == nullptr || record->kind != ControlKind::kTextField || !record->enabled) return false;
  if (id == L"nav.search") {
    search_text_ = text;
    UpdateSearch(search_text_);
    search_enter_ = enter;
  }
  Dispatch(Interaction{Interaction::Type::kCommitText, id, 0.0, 0, text});
  return true;
}

void App::ActivateRecord(const ControlRecord& record) {
  if (!record.enabled) return;
  switch (record.kind) {
    case ControlKind::kTextField: BeginEdit(record); break;
    case ControlKind::kStatic:
    case ControlKind::kSlider: break;
    default: Dispatch(Interaction{Interaction::Type::kActivate, record.id, 0.0, 0, {}}); break;
  }
}

void App::SliderFromPoint(const ControlRecord& record, float x) {
  const float width = record.full_rect.right - record.full_rect.left;
  if (width <= 0) return;
  const double fraction = std::clamp((x - record.full_rect.left) / width, 0.0f, 1.0f);
  Dispatch(Interaction{Interaction::Type::kSetValue, record.id,
                       record.slider_min + fraction * (record.slider_max - record.slider_min), 0, {}});
}

void App::OnMouseMove(D2D1_POINT_2F point) {
  if (!tracking_mouse_) {
    TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd_, 0};
    tracking_mouse_ = TrackMouseEvent(&track) != FALSE;
  }
  if (!drag_slider_.empty()) {
    if (const ControlRecord* slider = FindRecord(drag_slider_)) SliderFromPoint(*slider, point.x);
    return;
  }
  const ControlRecord* hit = HitTest(point);
  const std::wstring hovered = hit != nullptr && hit->enabled ? hit->id : std::wstring();
  if (hovered != state_.hovered) {
    state_.hovered = hovered;
    Invalidate();
  }
}

void App::OnMouseDown(D2D1_POINT_2F point) {
  animator_.Finish(L"page." + std::to_wstring(page_epoch_));
  if (!state_.editing.empty()) EndEdit(true);
  if (GetFocus() != hwnd_) SetFocus(hwnd_);
  const ControlRecord* hit = HitTest(point);
  state_.focus_visible = false;
  if (menu_focus_ >= 0 &&
      (hit == nullptr || (hit->id != menu_control_ && hit->id.rfind(menu_control_ + L"#", 0) != 0))) {
    CloseMenu();
    hit = HitTest(point);
  }
  if (hit == nullptr || !hit->enabled) {
    Invalidate();
    return;
  }
  state_.pressed = hit->id;
  if (IsTabStop(*hit)) state_.focused = hit->id;
  SetCapture(hwnd_);
  if (hit->kind == ControlKind::kSlider) {
    drag_slider_ = hit->id;
    const ControlRecord copy = *hit;
    SliderFromPoint(copy, point.x);
  }
  Invalidate();
}

void App::OnMouseUp(D2D1_POINT_2F point) {
  const std::wstring pressed = state_.pressed;
  const bool was_dragging = !drag_slider_.empty();
  drag_slider_.clear();
  state_.pressed.clear();
  if (GetCapture() == hwnd_) ReleaseCapture();
  if (!was_dragging && !pressed.empty()) {
    const ControlRecord* hit = HitTest(point);
    if (hit != nullptr && hit->id == pressed) {
      const ControlRecord copy = *hit;
      ActivateRecord(copy);
    }
  }
  Invalidate();
}

void App::SetFocusTo(const std::wstring& id, bool visible, bool scroll_into_view) {
  state_.focused = id;
  state_.focus_visible = visible;
  for (std::size_t i = 0; i < records_.size(); ++i) {
    if (records_[i].id != id) continue;
    const D2D1_RECT_F& rect = records_[i].full_rect;
    if (scroll_into_view && rect.left >= viewport_.left && rect.top < 1e6f) ScrollIntoView(rect);
    if (hwnd_ != nullptr) NotifyWinEvent(EVENT_OBJECT_FOCUS, hwnd_, OBJID_CLIENT, static_cast<LONG>(i + 1));
    break;
  }
  Invalidate();
}

void App::SetMenu(const std::wstring& control, int option, int count) {
  if (option < 0 || count <= 0) {
    if (menu_control_ == control) CloseMenu();
    return;
  }
  if (menu_focus_ >= 0 && menu_control_ != control) CloseMenu();
  const bool opening = menu_focus_ < 0;
  if (opening) SetFocusTo(control, state_.focus_visible);
  menu_control_ = control;
  menu_count_ = count;
  menu_focus_ = std::min(option, count - 1);
  if (opening) {
    PaintNow();
    NotifyWinEvent(EVENT_OBJECT_REORDER, hwnd_, OBJID_CLIENT, CHILDID_SELF);
  }
  SetFocusTo(control + L"#" + std::to_wstring(menu_focus_), state_.focus_visible, false);
}

void App::CloseMenu() {
  if (menu_focus_ < 0) return;
  const std::wstring control = menu_control_;
  menu_focus_ = -1;
  menu_count_ = 0;
  menu_control_.clear();
  PaintNow();
  NotifyWinEvent(EVENT_OBJECT_REORDER, hwnd_, OBJID_CLIENT, CHILDID_SELF);
  SetFocusTo(control, state_.focus_visible, false);
}

bool App::FocusedIsComboBox() const {
  if (state_.focused.empty()) return false;
  for (const ControlRecord& record : records_) {
    if (record.id == state_.focused) return record.kind == ControlKind::kComboBox;
  }
  return false;
}

int App::MenuPanel(Ui& ui, const std::wstring& control, const D2D1_RECT_F& button,
                   const std::vector<std::wstring>& options, int selected) {
  constexpr float row_height = 32.0f;
  constexpr float padding = 4.0f;
  const float menu_height = row_height * static_cast<float>(options.size()) + 2.0f * padding;
  const float below = button.bottom + padding;
  const float top = below + menu_height <= viewport_.bottom
                        ? below
                        : std::max(viewport_.top + padding, std::min(button.top - padding, viewport_.bottom) - menu_height);
  const D2D1_RECT_F panel = D2D1::RectF(button.left, top, button.right, top + menu_height);
  ui.Fill(panel, theme::kDialog, theme::kControlRadius);
  ui.Stroke(panel, theme::kControlBorder, theme::kControlRadius);
  int chosen = -1;
  for (int i = 0; i < static_cast<int>(options.size()); ++i) {
    const std::wstring id = control + L"#" + std::to_wstring(i);
    const float row_top = top + padding + static_cast<float>(i) * row_height;
    const D2D1_RECT_F row = D2D1::RectF(panel.left + padding, row_top, panel.right - padding, row_top + row_height);
    if (ui.ComboOption(id, row, options[static_cast<std::size_t>(i)], i == selected)) chosen = i;
  }
  return chosen;
}

void App::MoveFocus(bool forward) {
  std::vector<std::size_t> stops;
  for (std::size_t i = 0; i < records_.size(); ++i) {
    const ControlRecord& record = records_[i];
    if (!IsTabStop(record)) continue;
    if (record.group != 0) {
      bool group_has_checked = false;
      bool first_in_group = true;
      for (std::size_t j = 0; j < records_.size(); ++j) {
        if (records_[j].group != record.group) continue;
        group_has_checked |= records_[j].checked;
        if (j < i && IsTabStop(records_[j])) first_in_group = false;
      }
      if (group_has_checked ? !record.checked : !first_in_group) continue;
    }
    stops.push_back(i);
  }
  if (stops.empty()) return;
  std::size_t current = stops.size();
  for (std::size_t k = 0; k < stops.size(); ++k) {
    const ControlRecord& record = records_[stops[k]];
    if (record.id == state_.focused) current = k;
    if (current == stops.size() && record.group != 0) {
      if (const ControlRecord* focused = FindRecord(state_.focused); focused != nullptr && focused->group == record.group) {
        current = k;
      }
    }
  }
  std::size_t next;
  if (current == stops.size()) {
    next = forward ? 0 : stops.size() - 1;
  } else {
    next = forward ? (current + 1) % stops.size() : (current + stops.size() - 1) % stops.size();
  }
  SetFocusTo(records_[stops[next]].id, true);
}

void App::OnKeyDown(WPARAM key) {
  const bool control = GetKeyState(VK_CONTROL) < 0;
  const bool shift = GetKeyState(VK_SHIFT) < 0;
  if (overlay_.open && key == VK_ESCAPE) {
    CloseOverlay();
    return;
  }
  if (options_.mode == AppMode::kPrompt && key == VK_ESCAPE) {
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    return;
  }
  if (menu_focus_ >= 0) {
    state_.focus_visible = true;
    const std::wstring control = menu_control_;
    const int count = menu_count_;
    const int last = count - 1;
    switch (key) {
      case VK_DOWN: SetMenu(control, std::min(menu_focus_ + 1, last), count); return;
      case VK_UP: SetMenu(control, std::max(menu_focus_ - 1, 0), count); return;
      case VK_HOME: SetMenu(control, 0, count); return;
      case VK_END: SetMenu(control, last, count); return;
      case VK_RETURN:
      case VK_SPACE: Activate(control + L"#" + std::to_wstring(menu_focus_)); return;
      case VK_ESCAPE:
      case VK_F4: CloseMenu(); return;
      case VK_TAB: CloseMenu(); break;
      default: break;
    }
  } else if (FocusedIsComboBox() && (key == VK_DOWN || key == VK_UP || key == VK_F4)) {
    state_.focus_visible = true;
    Activate(state_.focused);
    return;
  }
  if (control && key >= '0' && key <= '9' && !overlay_.open && options_.mode == AppMode::kLauncher) {
    const int index = key == '0' ? 9 : static_cast<int>(key - '1');
    if (index >= kPageCount) return;
    const Page target = static_cast<Page>(index);
    Navigate(target);
    if (page_ == target) SetFocusTo(L"nav." + Utf8ToWide(PageKey(target)), true);
    return;
  }
  const ControlRecord* focused = FindRecord(state_.focused);
  const auto send_step = [&](int step) {
    Dispatch(Interaction{Interaction::Type::kStep, focused->id, 0.0, step, {}});
    state_.focus_visible = true;
  };
  if (focused != nullptr && focused->kind == ControlKind::kTextView) {
    const std::wstring id = focused->id;
    switch (key) {
      case VK_UP: send_step(-1); return;
      case VK_DOWN: send_step(1); return;
      case VK_PRIOR: send_step(-14); return;
      case VK_NEXT: send_step(14); return;
      case VK_HOME:
      case VK_END:
        Dispatch(Interaction{Interaction::Type::kSetValue, id, key == VK_HOME ? 0.0 : 1e9, 0, {}});
        return;
      default: break;
    }
  }
  switch (key) {
    case VK_TAB:
      MoveFocus(!shift);
      return;
    case VK_RETURN:
    case VK_SPACE:
      if (focused != nullptr) {
        state_.focus_visible = true;
        const ControlRecord copy = *focused;
        ActivateRecord(copy);
      }
      return;
    case VK_LEFT:
    case VK_RIGHT:
      if (focused != nullptr && (focused->kind == ControlKind::kSegment || focused->kind == ControlKind::kSlider)) {
        send_step(key == VK_RIGHT ? 1 : -1);
      }
      return;
    case VK_UP:
    case VK_DOWN: {
      const int step = key == VK_DOWN ? 1 : -1;
      if (focused != nullptr && focused->kind == ControlKind::kNav) {
        int index = static_cast<int>(focused - records_.data()) + step;
        for (; index >= 0 && index < static_cast<int>(records_.size()); index += step) {
          if (records_[index].kind != ControlKind::kNav) continue;
          const ControlRecord next = records_[index];
          ActivateRecord(next);
          if (!overlay_.open) SetFocusTo(next.id, true);
          break;
        }
      } else if (focused != nullptr && (focused->kind == ControlKind::kListItem || focused->kind == ControlKind::kRadio) &&
                 focused->group != 0) {
        std::vector<const ControlRecord*> members;
        for (const ControlRecord& record : records_) {
          if (record.group == focused->group && record.enabled) members.push_back(&record);
        }
        for (std::size_t i = 0; i < members.size(); ++i) {
          if (members[i]->id != focused->id) continue;
          const int target = static_cast<int>(i) + step;
          if (target >= 0 && target < static_cast<int>(members.size())) {
            const ControlRecord copy = *members[static_cast<std::size_t>(target)];
            SetFocusTo(copy.id, true);
            ActivateRecord(copy);
          }
          break;
        }
      } else if (focused != nullptr &&
                 (focused->kind == ControlKind::kSegment || focused->kind == ControlKind::kSlider)) {
        send_step(key == VK_DOWN ? 1 : -1);
      } else {
        ScrollBy(step * 64.0f);
      }
      return;
    }
    case VK_PRIOR:
    case VK_NEXT:
      ScrollBy((key == VK_NEXT ? 1.0f : -1.0f) * (viewport_.bottom - viewport_.top) * 0.85f);
      return;
    case VK_HOME:
    case VK_END:
      if (focused != nullptr && focused->kind == ControlKind::kSlider) {
        Dispatch(Interaction{Interaction::Type::kSetValue, focused->id,
                             key == VK_HOME ? focused->slider_min : focused->slider_max, 0, {}});
      } else {
        ScrollBy(key == VK_HOME ? -1e7f : 1e7f);
      }
      return;
    case VK_F5:
      if (page_ == Page::kCrashLogs) RefreshCrashLogs();
      if (page_ == Page::kSettings) {
        RefreshAudioDevices();
        MeasureCaches();
      }
      if (page_ == Page::kFastFlags) LoadFastFlags(false);
      if (page_ == Page::kGlobalSettings || page_ == Page::kFastFlags) RefreshGlobalSettings();
      if (page_ == Page::kMods) RefreshMods();
      if (page_ == Page::kShortcuts) RefreshShortcuts();
      Invalidate();
      return;
    case VK_ESCAPE:
      state_.focus_visible = false;
      Invalidate();
      return;
    default:
      return;
  }
}

bool App::SmoothScrolling() const { return motion_enabled_ && !options_.test_mode; }

void App::ScrollBy(float delta) {
  const int index = static_cast<int>(page_);
  ScrollTo((SmoothScrolling() ? scroll_target_[index] : scroll_[index]) + delta);
}

void App::ScrollTo(float scroll) {
  if (!state_.editing.empty()) EndEdit(true);
  const int index = static_cast<int>(page_);
  const float max_scroll = std::max(0.0f, content_height_[index] - (viewport_.bottom - viewport_.top));
  const float next = std::clamp(scroll, 0.0f, max_scroll);
  if (!SmoothScrolling()) {
    scroll_target_[index] = next;
    if (next != scroll_[index]) {
      scroll_[index] = next;
      Invalidate();
    }
    return;
  }
  if (next == scroll_target_[index]) return;
  if (scroll_target_[index] == scroll_[index]) scroll_tick_ = NowMs();
  scroll_target_[index] = next;
  Invalidate();
}

void App::AdvanceScroll() {
  const int index = static_cast<int>(page_);
  float& scroll = scroll_[index];
  const float target = scroll_target_[index];
  const double now = NowMs();
  if (scroll == target || !SmoothScrolling()) {
    scroll = target;
    scroll_tick_ = now;
    return;
  }
  const double elapsed = std::clamp(now - scroll_tick_, 0.0, 100.0);
  scroll_tick_ = now;
  const float remaining = target - scroll;
  const float step = remaining * static_cast<float>(1.0 - std::exp(-elapsed / 55.0));
  scroll = std::fabs(remaining - step) < 0.5f ? target : scroll + step;
}

void App::ScrollIntoView(const D2D1_RECT_F& rect) {
  const float margin = 16.0f;
  const float shown = scroll_[static_cast<int>(page_)];
  if (rect.top < viewport_.top + margin) {
    ScrollTo(shown + rect.top - (viewport_.top + margin));
  } else if (rect.bottom > viewport_.bottom - margin) {
    ScrollTo(shown + std::min(rect.bottom - (viewport_.bottom - margin), rect.top - (viewport_.top + margin)));
  }
}

void App::Navigate(Page page) {
  if (menu_focus_ >= 0) CloseMenu();
  if (!state_.editing.empty()) EndEdit(true);
  if (page_ != page) {
    if (GuardFastFlags(page, false)) return;
    if (page_ == Page::kFastFlags) {
      fast_flags_.editor_open = false;
      fast_flags_.adding = false;
      fast_flags_.selected.clear();
    }
    page_ = page;
    state_.hovered.clear();
    if (page == Page::kCrashLogs) RefreshCrashLogs();
    if (page == Page::kSettings) MeasureCaches();
    if (page == Page::kFastFlags) LoadFastFlags(false);
    if (page == Page::kGlobalSettings || page == Page::kFastFlags) RefreshGlobalSettings();
    if (page == Page::kMods) RefreshMods();
    if (page == Page::kShortcuts) RefreshShortcuts();
    if (page == Page::kUpdates && releases_.stage == ReleaseHistory::Stage::kIdle) CheckForUpdates();
    if (hwnd_ != nullptr) NotifyWinEvent(EVENT_OBJECT_REORDER, hwnd_, OBJID_CLIENT, CHILDID_SELF);
  }
  Invalidate();
}

bool App::busy() const { return pending_work_->load() > 0; }

int App::AccessibleFocusIndex() const {
  for (std::size_t i = 0; i < records_.size(); ++i) {
    if (records_[i].id == state_.focused && IsTabStop(records_[i])) return static_cast<int>(i);
  }
  return -1;
}

void App::AccessibleActivate(int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= records_.size()) return;
  const std::wstring id = records_[static_cast<std::size_t>(index)].id;
  PostTask(hwnd_, this, [this, id] { Activate(id); });
}

void App::AccessibleFocus(int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= records_.size()) return;
  SetFocusTo(records_[static_cast<std::size_t>(index)].id, true);
}

void App::BeginEdit(const ControlRecord& record) {
  animator_.Finish(L"page." + std::to_wstring(page_epoch_));
  if (!state_.editing.empty()) EndEdit(true);
  if (editor_ == nullptr || overlay_.open) return;
  state_.editing = record.id;
  state_.focused = record.id;
  editing_original_ = record.value;
  SetWindowTextW(editor_, record.value.c_str());
  PositionNativeChildren();
  ShowWindow(editor_, SW_SHOW);
  SetFocus(editor_);
  SendMessageW(editor_, EM_SETSEL, 0, -1);
  Invalidate();
}

void App::EndEdit(bool commit) {
  if (state_.editing.empty() || ending_edit_) return;
  ending_edit_ = true;
  const std::wstring id = state_.editing;
  const int length = GetWindowTextLengthW(editor_);
  std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
  GetWindowTextW(editor_, text.data(), length + 1);
  text.resize(static_cast<std::size_t>(length));
  state_.editing.clear();
  ShowWindow(editor_, SW_HIDE);
  if (GetFocus() == editor_) SetFocus(hwnd_);
  ending_edit_ = false;
  if (commit) Dispatch(Interaction{Interaction::Type::kCommitText, id, 0.0, 0, TrimWide(text)});
  Invalidate();
}

void App::PositionNativeChildren() {
  const float s = scale();
  if (!state_.editing.empty()) {
    const ControlRecord* record = FindRecord(state_.editing);
    const bool fully_visible = record != nullptr && !record->offscreen &&
                               record->rect.top == record->full_rect.top &&
                               record->rect.bottom == record->full_rect.bottom;
    if (!fully_visible || overlay_.open) {
      EndEdit(true);
    } else {
      HDC dc = GetDC(editor_);
      HGDIOBJ old = SelectObject(dc, editor_font_);
      TEXTMETRICW metrics{};
      GetTextMetricsW(dc, &metrics);
      SelectObject(dc, old);
      ReleaseDC(editor_, dc);
      const int line = metrics.tmHeight + 2;
      const int x = static_cast<int>(std::lround((record->rect.left + 11.0f) * s));
      const int width = static_cast<int>(std::lround((record->rect.right - record->rect.left - 22.0f) * s));
      const float center = (record->rect.top + record->rect.bottom) / 2.0f * s;
      const int y = static_cast<int>(std::lround(center - line / 2.0f));
      SetWindowPos(editor_, HWND_TOP, x, y, width, line, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
  }
}

void App::Post(std::function<void()> task) { PostTask(hwnd_, this, std::move(task)); }

void App::PostToWindow(HWND hwnd, App* target, std::function<void()> task) {
  PostTask(hwnd, target, std::move(task));
}

void App::RunInBackground(std::function<std::function<void()>()> work) {
  HWND hwnd = hwnd_;
  App* self = this;
  auto counter = pending_work_;
  ++*counter;
  std::thread([hwnd, self, counter, work = std::move(work)]() {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::function<void()> done = work();
    if (SUCCEEDED(com)) CoUninitialize();
    auto* task = new Task{self, [counter, done = std::move(done)] {
                            --*counter;
                            if (done) done();
                          }};
    if (hwnd == nullptr || !PostMessageW(hwnd, kTaskMessage, 0, reinterpret_cast<LPARAM>(task))) {
      --*counter;
      delete task;
    }
  }).detach();
}

void App::SaveConfig() {
  if (options_.test_mode || hwnd_ == nullptr) {
    SaveConfigNow();
    return;
  }
  config_dirty_ = true;
  SetTimer(hwnd_, kConfigSaveTimer, 300, nullptr);
}

void App::SaveConfigNow() {
  if (hwnd_ != nullptr) KillTimer(hwnd_, kConfigSaveTimer);
  config_dirty_ = false;
  std::wstring error;
  const std::wstring previous = save_error_;
  if (store_.Save(config_, &error)) {
    save_error_.clear();
  } else {
    save_error_ = SettingsSaveError(error);
  }
  if (save_error_ != previous) Invalidate();
}

void App::RefreshAudioDevices() {
  RunInBackground([this]() -> std::function<void()> {
    std::wstring output_error;
    std::wstring input_error;
    std::vector<AudioDevice> outputs = EnumerateAudioOutputs(&output_error);
    std::vector<AudioDevice> inputs = EnumerateAudioInputs(&input_error);
    return [this, outputs = std::move(outputs), inputs = std::move(inputs), output_error = std::move(output_error),
            input_error = std::move(input_error)]() mutable {
      audio_devices_ = std::move(outputs);
      audio_error_ = std::move(output_error);
      audio_input_devices_ = std::move(inputs);
      audio_input_error_ = std::move(input_error);
      audio_loaded_ = true;
      Invalidate();
    };
  });
}

void App::RefreshCrashLogs() {
  const std::wstring previous =
      crash_.selected >= 0 && crash_.selected < static_cast<int>(crash_.entries.size())
          ? crash_.entries[static_cast<std::size_t>(crash_.selected)].path
          : std::wstring();
  crash_.entries = ListCrashLogs(paths_.crashes_dir);
  int selected = crash_.entries.empty() ? -1 : 0;
  for (std::size_t i = 0; i < crash_.entries.size(); ++i) {
    if (crash_.entries[i].path == previous) selected = static_cast<int>(i);
  }
  if (selected >= 0 && crash_.entries[static_cast<std::size_t>(selected)].path == crash_.shown_path) {
    const CrashLogEntry& entry = crash_.entries[static_cast<std::size_t>(selected)];
    if (entry.size != crash_.shown_size || CompareFileTime(&entry.modified, &crash_.shown_modified) != 0) {
      crash_.shown_path.clear();
    }
  }
  SelectCrashLog(selected);
}

void App::SelectCrashLog(int index) {
  crash_.selected = index;
  crash_.error.clear();
  if (index < 0 || index >= static_cast<int>(crash_.entries.size())) {
    crash_.text.clear();
    crash_.shown_path.clear();
    Invalidate();
    return;
  }
  const CrashLogEntry& entry = crash_.entries[static_cast<std::size_t>(index)];
  if (entry.path == crash_.shown_path) {
    Invalidate();
    return;
  }
  const bool same_file = entry.path == crash_.shown_path;
  crash_.shown_path = entry.path;
  crash_.shown_size = entry.size;
  crash_.shown_modified = entry.modified;
  crash_.text.clear();
  if (!same_file) crash_.scroll = 0.0f;
  if (!entry.is_dump) {
    std::wstring error;
    if (!ReadCrashLogText(entry.path, &crash_.text, &error, kCrashViewerMaxBytes)) {
      crash_.error = L"Couldn’t open this crash log.";
    }
  }
  Invalidate();
}

void App::DeleteSelectedCrashLog() {
  if (crash_.selected < 0 || crash_.selected >= static_cast<int>(crash_.entries.size())) return;
  Overlay overlay;
  overlay.id = L"crash.delete";
  overlay.status = 1;
  overlay.title = L"Delete this crash log?";
  overlay.body = L"It will be moved to the Recycle Bin.";
  overlay.actions = {{L"crash.delete.confirm", L"Delete", ButtonStyle::kDanger},
                     {L"overlay.close", L"Cancel", ButtonStyle::kStandard}};
  ShowOverlay(std::move(overlay));
}

std::wstring App::FflagsPath() const {
  return config_.advanced.fflags_path.empty() ? paths_.fflags_file : Utf8ToWide(config_.advanced.fflags_path);
}

void App::ChooseApk() {
  if (options_.test_mode) return;
  ComPtr<IFileOpenDialog> dialog;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
  const COMDLG_FILTERSPEC filters[] = {{L"Roblox packages (*.apk, *.apkm, *.xapk)", L"*.apk;*.apkm;*.xapk"},
                                       {L"All files (*.*)", L"*.*"}};
  dialog->SetFileTypes(2, filters);
  dialog->SetTitle(L"Choose a Roblox file");
  FILEOPENDIALOGOPTIONS flags = 0;
  dialog->GetOptions(&flags);
  dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
  if (FAILED(dialog->Show(hwnd_))) return;
  ComPtr<IShellItem> item;
  PWSTR path = nullptr;
  if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return;
  const std::wstring chosen = path;
  CoTaskMemFree(path);
  ValidatePackage(chosen);
}

void App::ValidatePackage(const std::wstring& path) {
  if (install_.stage == InstallFlow::Stage::kValidating || install_.stage == InstallFlow::Stage::kInstalling) return;
  if (cache_.clearing || switching_ || RobloxUpdateRunning()) {
    ShowToast(cache_.clearing ? L"Elixir is clearing the cache. Try again in a moment."
              : RobloxUpdateRunning() ? L"Elixir is updating Roblox. Try again when it finishes."
                                      : L"Wait for the current install to finish, then try again.");
    return;
  }
  install_.file = path;
  install_.size = 0;
  for (const std::wstring& file : CompanionApkFiles(install_.file)) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &data)) {
      install_.size += (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    }
  }
  Navigate(Page::kInstall);
  StartValidation();
}

void App::StartValidation() {
  if (install_.file.empty() || PackageWorkRunning() || cache_.clearing || RobloxUpdateRunning()) return;
  install_.stage = InstallFlow::Stage::kValidating;
  install_.validation = ApkResult{};
  install_.install = ApkResult{};
  install_.progress = ApkProgress{};
  install_.cancel = std::make_shared<std::atomic<bool>>(false);
  const ApkComponent component = apk_;
  const std::wstring file = install_.file;
  const std::wstring work = JoinPath(paths_.data_root, L"tmp");
  const auto cancel = install_.cancel;
  HWND hwnd = hwnd_;
  RunInBackground([this, component, file, work, cancel, hwnd]() -> std::function<void()> {
    const ApkProgressCallback progress = [this, hwnd](const ApkProgress& progress) {
      PostTask(hwnd, this, [this, percent = progress.percent] {
        install_.progress.percent = percent;
        Invalidate();
      });
    };
    const std::vector<std::wstring> files = CompanionApkFiles(file);
    ApkResult result = files.size() > 1 ? ValidateApkFiles(component, files, progress, cancel.get())
                                        : ValidateApk(component, file, work, progress, cancel.get());
    return [this, result = std::move(result), cancel] {
      if (install_.cancel != cancel || install_.stage != InstallFlow::Stage::kValidating) return;
      install_.validation = result;
      install_.stage = result.ok ? InstallFlow::Stage::kValidated : InstallFlow::Stage::kRejected;
      Invalidate();
    };
  });
  Invalidate();
}

void App::StartInstall() {
  if (install_.stage != InstallFlow::Stage::kValidated || PackageWorkRunning() || cache_.clearing ||
      RobloxUpdateRunning()) {
    return;
  }
  install_.stage = InstallFlow::Stage::kInstalling;
  install_.progress = ApkProgress{};
  install_.cancel = std::make_shared<std::atomic<bool>>(false);
  const ApkComponent component = apk_;
  const ApkMetadata metadata = install_.validation.metadata;
  const std::wstring apps = paths_.apps_dir;
  const auto cancel = install_.cancel;
  HWND hwnd = hwnd_;
  RunInBackground([this, component, metadata, apps, cancel, hwnd]() -> std::function<void()> {
    ApkResult result = InstallApk(
        component, metadata, apps,
        [this, hwnd](const ApkProgress& progress) {
          PostTask(hwnd, this, [this, percent = progress.percent] {
            install_.progress.percent = percent;
            Invalidate();
          });
        },
        cancel.get());
    return [this, result = std::move(result), cancel] {
      if (install_.cancel != cancel || install_.stage != InstallFlow::Stage::kInstalling) return;
      install_.install = result;
      if (result.ok) {
        const ApkMetadata& m = result.metadata;
        InstalledApp app;
        app.package = m.package;
        app.version_code = m.version_code;
        app.version_name = m.version_name;
        app.signer_sha256 = m.signer_sha256;
        app.signer_status = m.signer_status;
        app.abi = PrimaryAbi(m.abis);
        app.install_dir = m.install_dir;
        app.installed_at = UtcIsoTimestamp();
        app.source_file = WideToUtf8(install_.file);
        SwitchToInstalledVersion(app, [this](SwitchOutcome result, const std::wstring& message) {
          if (result != SwitchOutcome::kSwitched) ShowToast(message);
        });
        CleanupApkWorkDirectory(JoinPath(paths_.data_root, L"tmp"));
        install_.stage = InstallFlow::Stage::kInstalled;
      } else {
        install_.stage = InstallFlow::Stage::kInstallFailed;
      }
      Invalidate();
    };
  });
  Invalidate();
}

bool App::RobloxUpdatePossible() const {
  if (options_.roblox_update_runner) return true;
  return !options_.test_mode && RobloxDownloadAvailable() && apk_.configured && apk_.found;
}

void App::StartRobloxUpdate(bool announce, const RobloxRelease* target) {
  if (RobloxUpdateRunning() || PackageWorkRunning() || cache_.clearing || !RobloxUpdatePossible()) return;
  roblox_update_ = RobloxUpdateFlow{};
  roblox_update_.stage = RobloxUpdateFlow::Stage::kRunning;
  roblox_update_.chosen = target != nullptr;
  if (target != nullptr) roblox_update_.version_name = target->version_name;
  roblox_update_.cancel = std::make_shared<std::atomic<bool>>(false);
  RobloxUpdateRequest request;
  request.component = apk_;
  request.exe_dir = paths_.exe_dir;
  request.downloads_dir = paths_.downloads_dir;
  request.apps_dir = paths_.apps_dir;
  request.work_dir = JoinPath(paths_.data_root, L"tmp");
  request.installed_code = config_.installed.package == kRobloxPackage ? config_.installed.version_code : 0;
  if (target != nullptr) request.target = *target;
  const auto cancel = roblox_update_.cancel;
  const auto runner = options_.roblox_update_runner;
  HWND hwnd = hwnd_;
  if (announce) ShowToast(L"Updating Roblox…");
  RunInBackground([this, request, cancel, runner, hwnd]() -> std::function<void()> {
    struct Shown {
      RobloxUpdatePhase phase = RobloxUpdatePhase::kChecking;
      int percent = -2;
      std::string version;
    };
    const auto shown = std::make_shared<Shown>();
    const RobloxUpdateProgressCallback progress = [this, hwnd, cancel, shown](const RobloxUpdateProgress& now) {
      if (shown->phase == now.phase && shown->percent == now.percent && shown->version == now.version_name) return;
      shown->phase = now.phase;
      shown->percent = now.percent;
      shown->version = now.version_name;
      PostTask(hwnd, this, [this, now, cancel] {
        if (roblox_update_.cancel != cancel) return;
        roblox_update_.phase = now.phase;
        roblox_update_.version_name = now.version_name;
        roblox_update_.percent = now.percent;
        Invalidate();
      });
    };
    RobloxUpdateOutcome outcome = runner ? runner(request, progress, cancel.get())
                                         : UpdateRoblox(request, RealRobloxUpdateSteps(request), progress, cancel.get());
    return [this, outcome = std::move(outcome), cancel] {
      if (roblox_update_.cancel != cancel) return;
      FinishRobloxUpdate(outcome);
    };
  });
  Invalidate();
}

void App::FinishRobloxUpdate(const RobloxUpdateOutcome& outcome) {
  using Result = RobloxUpdateOutcome::Result;
  roblox_update_.cancel.reset();
  roblox_update_.stage = RobloxUpdateFlow::Stage::kIdle;
  roblox_update_.percent = -1;
  switch (outcome.result) {
    case Result::kUpToDate:
    case Result::kCancelled:
      play_after_roblox_install_ = false;
      break;
    case Result::kFailed:
      play_after_roblox_install_ = false;
      roblox_update_.stage = RobloxUpdateFlow::Stage::kFailed;
      roblox_update_.error = outcome.error.empty() ? std::wstring(L"Couldn’t update Roblox.") : outcome.error;
      if (!roblox_update_.chosen) ShowToast(L"Roblox wasn’t updated. Open Installation to see why.");
      break;
    case Result::kInstalled: {
      const ApkMetadata& m = outcome.metadata;
      InstalledApp app;
      app.package = m.package;
      app.version_code = m.version_code;
      app.version_name = m.version_name;
      app.signer_sha256 = m.signer_sha256;
      app.signer_status = m.signer_status;
      app.abi = PrimaryAbi(m.abis);
      app.install_dir = m.install_dir;
      app.installed_at = UtcIsoTimestamp();
      app.source_file = "github.com/Roblox-DeployHistory-Updates/android-runner release " + std::to_string(m.version_code);
      const std::wstring name = Utf8ToWide(m.version_name);
      const bool chosen = roblox_update_.chosen;
      const bool play = play_after_roblox_install_;
      play_after_roblox_install_ = false;
      SwitchToInstalledVersion(app, [this, name, chosen, play](SwitchOutcome result, const std::wstring& message) {
        if (result != SwitchOutcome::kSwitched) {
          ShowToast(message);
        } else if (play) {
          play_from_menu_ = true;
          Play();
        } else if (!chosen) {
          ShowToast(L"Roblox was updated to " + name + L".");
        }
      });
      CleanupApkWorkDirectory(JoinPath(paths_.data_root, L"tmp"));
      break;
    }
  }
  Invalidate();
}

void App::Play() {
  if (runtime_.running) return;
  if (!PathIsFile(paths_.runtime_exe)) {
    Overlay overlay;
    overlay.id = L"play.unavailable";
    overlay.status = 2;
    overlay.title = L"Elixir is incomplete";
    overlay.body = L"Part of Elixir is missing. Reinstall Elixir, then try again.";
    overlay.actions = {{L"overlay.close", L"Close", ButtonStyle::kStandard}};
    ShowOverlay(std::move(overlay));
    return;
  }
  if (!config_.installed.present()) {
    Overlay overlay;
    overlay.id = L"play.noapp";
    overlay.status = 1;
    overlay.title = L"Install Roblox first";
    overlay.body = L"Roblox isn’t installed yet.";
    overlay.actions = {{L"overlay.install", L"Install Roblox", ButtonStyle::kStandard},
                       {L"overlay.close", L"Close", ButtonStyle::kStandard}};
    ShowOverlay(std::move(overlay));
    return;
  }
  if (options_.test_mode) return;
  if (switching_) {
    ShowToast(L"Roblox is being updated. Press Play again in a moment.");
    return;
  }
  LaunchRuntimeNow();
}

void App::LaunchRuntimeNow() {
  if (runtime_.running || options_.test_mode || !PathIsFile(paths_.runtime_exe) || !config_.installed.present()) {
    return;
  }
  if (preparing_assets_) return;
  const std::wstring install_dir = Utf8ToWide(config_.installed.install_dir);
  if (!install_dir.empty() && !PathIsDirectory(JoinPath(install_dir, L"assets"))) {
    preparing_assets_ = true;
    ShowLaunchProgress(L"Configuring Roblox…", false);
    const ApkComponent component = apk_;
    RunInBackground([this, component, install_dir]() -> std::function<void()> {
      const ApkResult result = EnsureInstalledAssets(component, install_dir);
      return [this, result] {
        preparing_assets_ = false;
        if (result.ok) {
          LaunchRuntimeNow();
          return;
        }
        CloseLaunchProgress();
        Overlay overlay;
        overlay.id = L"play.assets";
        overlay.status = 2;
        overlay.title = L"Roblox couldn’t be prepared";
        overlay.body = L"Reinstall Roblox on the Installation page, then try again.";
        overlay.actions = {{L"overlay.close", L"Close", ButtonStyle::kStandard}};
        ShowOverlay(std::move(overlay));
      };
    });
    Invalidate();
    return;
  }
  if (config_dirty_) SaveConfigNow();
  const RuntimeLaunch launch = LaunchRuntime(config_, paths_);
  if (!launch.ok) {
    CloseLaunchProgress();
    Overlay overlay;
    overlay.id = L"play.failed";
    overlay.status = 2;
    overlay.title = L"Roblox couldn’t start";
    overlay.body = (launch.error.empty() ? std::wstring() : launch.error + L" ") + L"Try again, or reinstall Elixir.";
    overlay.actions = {{L"overlay.close", L"Close", ButtonStyle::kStandard}};
    ShowOverlay(std::move(overlay));
    return;
  }
  runtime_ = RuntimeProcess{};
  runtime_.process = launch.process;
  runtime_.ipc_pipe = launch.ipc_pipe;
  runtime_.pid = launch.pid;
  runtime_.running = true;
  if (play_from_menu_) ShowWindow(hwnd_, SW_HIDE);
  AllowSetForegroundWindow(launch.pid);
  ShowLaunchProgress(L"Starting Roblox…", true);
  if (launch.ipc_pipe != nullptr) {
    HANDLE events = nullptr;
    DuplicateHandle(GetCurrentProcess(), launch.process, GetCurrentProcess(), &events, SYNCHRONIZE, FALSE, 0);
    const HANDLE pipe = launch.ipc_pipe;
    const HWND hwnd = hwnd_;
    App* self = this;
    std::thread([self, hwnd, pipe, events] {
      ReadRuntimeEvents(pipe, events, [self, hwnd](const RuntimeEvent& event) {
        PostToWindow(hwnd, self, [self, event] { self->OnRuntimeEvent(event); });
      });
      if (events != nullptr) CloseHandle(events);
    }).detach();
  }
  HANDLE wait = nullptr;
  DuplicateHandle(GetCurrentProcess(), launch.process, GetCurrentProcess(), &wait,
                  SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0);
  RunInBackground([this, wait]() -> std::function<void()> {
    DWORD code = 0;
    if (wait != nullptr) {
      WaitForSingleObject(wait, INFINITE);
      GetExitCodeProcess(wait, &code);
      CloseHandle(wait);
    }
    return [this, code] { OnRuntimeExited(code); };
  });
  Invalidate();
}

void App::StopRuntime() {
  if (!runtime_.running || runtime_.stopping || runtime_.process == nullptr) return;
  HANDLE process = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), runtime_.process, GetCurrentProcess(), &process,
                       SYNCHRONIZE | PROCESS_TERMINATE, FALSE, 0)) {
    ShowToast(L"Couldn’t stop Roblox.");
    return;
  }
  runtime_.stopping = true;
  const DWORD pid = runtime_.pid;
  SendRuntimeStop(runtime_.ipc_pipe);
  RunInBackground([process, pid]() -> std::function<void()> {
    EnumWindows(
        [](HWND window, LPARAM target) -> BOOL {
          DWORD owner = 0;
          GetWindowThreadProcessId(window, &owner);
          if (owner == static_cast<DWORD>(target)) PostMessageW(window, WM_CLOSE, 0, 0);
          return TRUE;
        },
        static_cast<LPARAM>(pid));
    if (WaitForSingleObject(process, 5000) == WAIT_TIMEOUT) TerminateProcess(process, kStoppedByLauncherExitCode);
    CloseHandle(process);
    return nullptr;
  });
  Invalidate();
}

void App::OnRuntimeEvent(const RuntimeEvent& event) {
  if (event.event == "host") {
    runtime_.connected = true;
    SendPendingJoin();
  } else if (event.event == "join") {
    OnJoinEvent(event.detail);
  } else if (event.event == "first-frame") {
    CloseLaunchProgress();
  }
}

void App::ShowLaunchProgress(const std::wstring& message, bool can_cancel) {
  if (options_.test_mode || hwnd_ == nullptr) return;
  LaunchProgress::View view;
  view.message = message;
  if (config_.installed.present()) view.version = L"Version: " + Utf8ToWide(config_.installed.version_name);
  view.cancel_enabled = can_cancel;
  launch_progress_.Show(std::move(view), [this] {
    Post([this] {
      CloseLaunchProgress();
      StopRuntime();
    });
  });
  SetTimer(hwnd_, kLaunchProgressTimer, kLaunchProgressMaxMs, nullptr);
}

void App::CloseLaunchProgress() {
  if (hwnd_ != nullptr) KillTimer(hwnd_, kLaunchProgressTimer);
  launch_progress_.Close();
}

void App::OnRuntimeExited(DWORD exit_code) {
  CloseLaunchProgress();
  const bool stopped = runtime_.stopping || exit_code == kStoppedByLauncherExitCode;
  const std::size_t crash_reports_before = crash_.entries.size();
  runtime_.running = false;
  runtime_.stopping = false;
  runtime_.connected = false;
  runtime_.in_game = false;
  runtime_.exited = true;
  runtime_.exit_code = exit_code;
  DropPendingJoin();
  if (runtime_.ipc_pipe != nullptr) {
    CloseHandle(runtime_.ipc_pipe);
    runtime_.ipc_pipe = nullptr;
  }
  if (runtime_.process != nullptr) {
    CloseHandle(runtime_.process);
    runtime_.process = nullptr;
  }
  RefreshCrashLogs();
  if (page_ == Page::kGlobalSettings || page_ == Page::kFastFlags) RefreshGlobalSettings();
  if (page_ == Page::kSettings) MeasureCaches();
  if (exit_code != 0 && !stopped) {
    Overlay overlay;
    overlay.id = L"play.ended";
    overlay.status = 2;
    overlay.title = L"Roblox closed unexpectedly";
    const std::wstring meaning = RuntimeExitMeaning(exit_code);
    overlay.body = meaning.empty() ? std::wstring(L"Press Play to try again.") : meaning;
    overlay.actions = {{L"overlay.close", L"Close", ButtonStyle::kStandard}};
    if (crash_.entries.size() > crash_reports_before) {
      overlay.actions.insert(overlay.actions.begin(), OverlayAction{L"overlay.crashes", L"Crash logs"});
    }
    ShowOverlay(std::move(overlay));
  }
  if (staged_roblox_) {
    const InstalledApp next = *staged_roblox_;
    SwitchToInstalledVersion(next, [this, next](SwitchOutcome result, const std::wstring& message) {
      if (result == SwitchOutcome::kSwitched) {
        ShowToast(L"Updated to Roblox " + Utf8ToWide(next.version_name) + L".");
      } else if (result == SwitchOutcome::kFailed) {
        ShowToast(message);
      }
    });
  }
  if (play_from_menu_) {
    play_from_menu_ = false;
    if (overlay_.open) {
      BringToFront();
    } else {
      ShowWindow(launch_menu_.hwnd(), SW_SHOWNORMAL);
      SetForegroundWindow(launch_menu_.hwnd());
    }
  }
  Invalidate();
}

void App::CheckForUpdates(bool fresh) {
  if (update_.stage == UpdateFlow::Stage::kChecking || update_.stage == UpdateFlow::Stage::kDownloading ||
      update_.stage == UpdateFlow::Stage::kInstalling) {
    return;
  }
  const HttpGetFunction get = WebGet();
  if (!get) return;
  update_.stage = UpdateFlow::Stage::kChecking;
  update_.error.clear();
  update_.cancel = std::make_shared<std::atomic<bool>>(false);
  if (releases_.list.empty()) releases_.stage = ReleaseHistory::Stage::kLoading;
  const auto cancel = update_.cancel;
  const std::wstring cache_file = options_.test_mode ? std::wstring() : JoinPath(paths_.updates_dir, L"releases.json");
  RunInBackground([this, get, cancel, cache_file, fresh]() -> std::function<void()> {
    FILETIME file_time{};
    GetSystemTimeAsFileTime(&file_time);
    const std::int64_t now =
        static_cast<std::int64_t>((static_cast<std::uint64_t>(file_time.dwHighDateTime) << 32 | file_time.dwLowDateTime) /
                                  10000000ull);
    ReleaseList list = FetchReleasesCached(get, cache_file, now, fresh);
    return [this, list = std::move(list), cancel] {
      if (update_.cancel != cancel || update_.stage != UpdateFlow::Stage::kChecking) return;
      if (!list.ok) {
        update_.stage = UpdateFlow::Stage::kFailed;
        update_.error_title = L"Couldn’t check for updates";
        update_.error = list.error;
        if (releases_.list.empty()) {
          releases_.stage = ReleaseHistory::Stage::kFailed;
          releases_.error = list.error;
        } else {
          releases_.stage = ReleaseHistory::Stage::kLoaded;
        }
      } else {
        releases_.list = list.releases;
        releases_.stage = ReleaseHistory::Stage::kLoaded;
        releases_.error.clear();
        const ElixirRelease* newest = NewestRelease(releases_.list);
        if (newest != nullptr && CompareVersions(newest->version, ELIXIR_VERSION) > 0) {
          update_.manifest = ManifestForRelease(*newest);
          update_.stage = UpdateFlow::Stage::kAvailable;
        } else {
          update_.stage = UpdateFlow::Stage::kUpToDate;
        }
      }
      if (update_.play_when_checked) {
        update_.play_when_checked = false;
        KillTimer(hwnd_, kUpdateWaitTimer);
        CloseLaunchProgress();
        PlayFromMenu();
      }
      Invalidate();
    };
  });
  Invalidate();
}

bool App::UpdateBeforePlaying() {
  if (options_.test_mode || !options_.installed || !config_.updates.auto_update || runtime_.running) return false;
  if (update_.stage == UpdateFlow::Stage::kChecking && !update_.waited) {
    update_.waited = true;
    update_.play_when_checked = true;
    ShowLaunchProgress(L"Checking for updates…", false);
    SetTimer(hwnd_, kUpdateWaitTimer, kUpdateWaitMs, nullptr);
    return true;
  }
  if (update_.stage != UpdateFlow::Stage::kAvailable) return false;
  update_.play_after = true;
  ShowLaunchProgress(L"Getting the latest Elixir…", false);
  DownloadUpdate();
  return true;
}

void App::DownloadUpdate() {
  if (update_.stage != UpdateFlow::Stage::kAvailable) return;
  update_.stage = UpdateFlow::Stage::kDownloading;
  update_.done = 0;
  update_.total = 0;
  update_.cancel = std::make_shared<std::atomic<bool>>(false);
  const UpdateManifest manifest = update_.manifest;
  const std::wstring updates_dir = paths_.updates_dir;
  const auto cancel = update_.cancel;
  HWND hwnd = hwnd_;
  RunInBackground([this, manifest, updates_dir, cancel, hwnd]() -> std::function<void()> {
    ULONGLONG last = 0;
    StagedUpdate staged = DownloadAndStage(
        manifest, updates_dir,
        [&](std::uint64_t done, std::uint64_t total) {
          const ULONGLONG now = GetTickCount64();
          if (now - last < 100 && done != total) return;
          last = now;
          PostTask(hwnd, this, [this, done, total] {
            update_.done = done;
            update_.total = total;
            Invalidate();
          });
        },
        cancel.get());
    return [this, staged = std::move(staged), cancel] {
      if (update_.cancel != cancel || update_.stage != UpdateFlow::Stage::kDownloading) return;
      update_.staged = staged;
      if (staged.ok) {
        InstallDownloadedUpdate();
        return;
      }
      update_.stage = UpdateFlow::Stage::kFailed;
      update_.error_title = L"Elixir couldn’t update";
      update_.error = cancel->load()        ? std::wstring(L"The download was cancelled.")
                      : staged.error.empty() ? std::wstring(L"The update couldn’t be downloaded. Try again.")
                                             : staged.error;
      if (update_.play_after) {
        update_.play_after = false;
        CloseLaunchProgress();
        PlayFromMenu();
      }
      Invalidate();
    };
  });
  Invalidate();
}

void App::InstallDownloadedUpdate() {
  if (!update_.staged.ok) return;
  update_.stage = UpdateFlow::Stage::kInstalling;
  Invalidate();
  if (options_.test_mode) return;
  const std::vector<std::wstring> arguments = {kUpgradeFlag, kWaitFlag, std::to_wstring(GetCurrentProcessId()),
                                               update_.play_after ? kPlayerFlag : kSettingsFlag};
  std::wstring command = BuildCommandLine(update_.staged.file, arguments);
  const std::wstring directory = ParentDirectory(update_.staged.file);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(update_.staged.file.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      directory.c_str(), &startup, &process)) {
    update_.stage = UpdateFlow::Stage::kFailed;
    update_.error_title = L"Elixir couldn’t update";
    update_.error = WithErrorCause(L"The new version couldn’t be started.", GetLastError());
    if (update_.play_after) {
      update_.play_after = false;
      CloseLaunchProgress();
      PlayFromMenu();
    }
    return;
  }
  AllowSetForegroundWindow(process.dwProcessId);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  CloseLaunchProgress();
  PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

void App::OpenPath(const std::wstring& path) {
  if (options_.test_mode || path.empty()) return;
  if (StartsWithI(path, L"https://") || PathIsDirectory(path)) {
    ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return;
  }
  if (IsDangerousToOpen(path)) {
    ShowToast(L"That file is a program, so Elixir didn’t open it.");
    return;
  }
  const std::wstring extension = ExtensionOf(path);
  wchar_t handler[MAX_PATH] = {};
  DWORD size = MAX_PATH;
  const bool has_handler =
      !extension.empty() && SUCCEEDED(AssocQueryStringW(ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_EXECUTABLE,
                                                        extension.c_str(), L"open", handler, &size));
  if (!has_handler) {
    wchar_t system_dir[MAX_PATH] = {};
    GetSystemDirectoryW(system_dir, MAX_PATH);
    const std::wstring notepad = JoinPath(system_dir, L"notepad.exe");
    ShellExecuteW(hwnd_, L"open", notepad.c_str(), QuoteArgument(path).c_str(), nullptr, SW_SHOWNORMAL);
    return;
  }
  ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void App::OpenFolder(const std::wstring& path) {
  if (options_.test_mode) return;
  EnsureDirectory(path, nullptr);
  if (!PathIsDirectory(path)) {
    ShowToast(L"Couldn’t open that folder.");
    return;
  }
  ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::wstring App::FindDocument(const wchar_t* relative) const {
  const std::wstring beside = JoinPath(paths_.exe_dir, relative);
  if (PathIsFile(beside)) return beside;
#if defined(ELIXIR_SOURCE_DIR)
  std::wstring source = Utf8ToWide(ELIXIR_SOURCE_DIR);
  std::replace(source.begin(), source.end(), L'/', L'\\');
  const std::wstring in_source = JoinPath(source, relative);
  if (PathIsFile(in_source)) return in_source;
#endif
  return {};
}

void App::OpenDocument(const wchar_t* relative, const wchar_t* fallback_name) {
  const std::wstring path = FindDocument(relative);
  if (path.empty()) {
    ShowToast(L"Couldn’t find " + std::wstring(fallback_name) + L". Reinstall Elixir to restore it.");
    return;
  }
  OpenPath(path);
}

void App::ShowOverlay(Overlay overlay) {
  if (menu_focus_ >= 0) CloseMenu();
  if (!state_.editing.empty()) EndEdit(true);
  if (!overlay_.open) overlay_return_focus_ = state_.focused;
  overlay_ = std::move(overlay);
  overlay_.open = true;
  state_.hovered.clear();
  state_.focused = overlay_.actions.empty() ? std::wstring() : overlay_.actions.front().id;
  PositionNativeChildren();
  Invalidate();
}

void App::CloseOverlay() {
  if (overlay_.id == L"join.leave") DropPendingJoin();
  overlay_ = Overlay{};
  state_.focused = overlay_return_focus_;
  Invalidate();
}

void App::OnOverlayAction(const std::wstring& id) {
  if (OnInstallerOverlayAction(id)) return;
  if (OnRobloxOverlayAction(id) || OnLinkOverlayAction(id)) return;
  if (id == L"overlay.close") {
    CloseOverlay();
  } else if (id == L"overlay.crashes") {
    CloseOverlay();
    Navigate(Page::kCrashLogs);
  } else if (id == L"overlay.install") {
    CloseOverlay();
    Navigate(Page::kInstall);
  } else if (id == L"crash.delete.confirm") {
    CloseOverlay();
    if (crash_.selected >= 0 && crash_.selected < static_cast<int>(crash_.entries.size())) {
      std::wstring error;
      const auto group = CrashLogGroup(crash_.entries[static_cast<std::size_t>(crash_.selected)].path);
      if (!options_.test_mode && RecycleFiles(group, &error)) {
        ShowToast(L"Moved to the Recycle Bin.");
      } else if (!error.empty()) {
        ShowToast(L"Couldn’t delete this crash log.");
      }
      RefreshCrashLogs();
    }
  }
}

}
