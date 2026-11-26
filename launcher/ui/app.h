// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_APP_H_
#define ELIXIR_LAUNCHER_UI_APP_H_

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "core/apk_component.h"
#include "core/audio_devices.h"
#include "core/cache_cleaner.h"
#include "core/config.h"
#include "core/crash_logs.h"
#include "core/game_shortcuts.h"
#include "core/global_settings.h"
#include "core/installer.h"
#include "core/paths.h"
#include "core/process.h"
#include "core/releases.h"
#include "core/instance.h"
#include "core/link_request.h"
#include "core/roblox_download.h"
#include "core/roblox_links.h"
#include "core/roblox_update.h"
#include "core/update.h"
#include "core/util.h"
#include "core/mods.h"
#include "core/notices.h"
#include "core/shortcuts.h"
#include "fflags/editor.h"
#include "ui/accessibility.h"
#include "ui/launch_menu.h"
#include "ui/launch_progress.h"
#include "ui/renderer.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace elixir::launcher::ui {

enum class Page {
  kInstall,
  kFastFlags,
  kIntegrations,
  kSettings,
  kCrashLogs,
  kUpdates,
  kAbout,
  kGlobalSettings,
  kMods,
  kShortcuts
};
inline constexpr int kPageCount = 10;
const wchar_t* PageTitle(Page page);
const char* PageKey(Page page);
bool PageFromKey(std::string_view key, Page* page);

SIZE MinimumWindowSize(UINT dpi, const RECT& work_area);

enum class Backdrop { kAuto, kSolid };

enum class AppMode { kLauncher, kInstaller, kPrompt };

enum class InstallerAction { kNone, kSettings, kLaunch };

struct InstallerOutcome {
  bool finished = false;
  InstallerAction action = InstallerAction::kNone;
  std::wstring location;
};

struct PromptSpec {
  std::wstring heading;
  std::wstring text;
  std::wstring checkbox;
  std::wstring checkbox_note;
  bool checked = false;
  std::vector<std::wstring> buttons;
};

struct PromptResult {
  int button = -1;
  bool checked = false;
};

SIZE InstallerWindowSize(UINT dpi);
SIZE PromptWindowSize(UINT dpi);

inline constexpr std::uint64_t kCrashViewerMaxBytes = 1u << 20;

struct OffscreenFrame {
  std::vector<ControlRecord> controls;
  UiLayoutLog layout;
  float width = 0;
  float height = 0;
  D2D1_RECT_F viewport{};
};

struct AppOptions {
  LauncherPaths paths;
  bool test_mode = false;
  bool offscreen = false;
  Backdrop backdrop = Backdrop::kAuto;
  int show_command = SW_SHOWNORMAL;
  std::function<bool()> runtime_probe;
  std::function<std::uint32_t()> control_accent_probe;
  std::function<RobloxReleaseList()> roblox_version_probe;
  std::function<RobloxUpdateOutcome(const RobloxUpdateRequest&, const RobloxUpdateProgressCallback&,
                                    const std::atomic<bool>*)>
      roblox_update_runner;
  bool compact_start = false;
  bool launch_now = false;
  bool installed = false;
  HttpGetFunction web_get;
  AppMode mode = AppMode::kLauncher;
  InstallEnvironment install_environment;
  std::wstring install_source;
  PromptSpec prompt;
};

inline constexpr UINT_PTR kFastFlagsSaveTimer = 3;
inline constexpr UINT_PTR kConfigSaveTimer = 4;
inline constexpr UINT_PTR kCacheMeasureTimer = 5;
inline constexpr UINT_PTR kLaunchProgressTimer = 6;
inline constexpr UINT_PTR kUpdateWaitTimer = 7;
inline constexpr UINT kUpdateWaitMs = 3000;
inline constexpr UINT kLaunchProgressMaxMs = 90000;
inline constexpr UINT kCacheMeasureIntervalMs = 5000;

inline constexpr DWORD kStoppedByLauncherExitCode = 0xE1C50001;

class App final : public AccessibilityHost {
 public:
  explicit App(AppOptions options);
  ~App() override;
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  bool Create();
  bool TranslateLaunchMenu(MSG* message) const { return launch_menu_.Translate(message); }
  HWND hwnd() const { return hwnd_; }

  void PostInstanceMessage(InstanceMessage message);
  void OpenLink(LinkRequest request);

  void Navigate(Page page);
  Page page() const { return page_; }
  bool Activate(const std::wstring& id);
  bool EnterText(const std::wstring& id, const std::wstring& text, bool enter);
  const std::wstring& focused_control() const { return state_.focused; }
  const std::vector<ControlRecord>& controls() const { return records_; }
  bool busy() const;
  bool mica_active() const { return mica_; }
  const LauncherConfig& config() const { return config_; }
  void PaintNow();
  std::wstring overlay_title() const { return overlay_.open ? overlay_.title : std::wstring(); }
  void ValidatePackage(const std::wstring& path);
  bool RenderOffscreen(UINT dpi, float width, float height, const std::wstring& png_path, OffscreenFrame* frame,
                       std::wstring* error);
  void SetScroll(float scroll);
  bool animation_timer_active() const { return animation_timer_; }
  bool motion_active() const { return motion_active_; }
  float scroll() const { return scroll_[static_cast<int>(page_)]; }

  void ImportFastFlagsText(const std::string& json, bool replace);
  const fflags::FlagEditor* fast_flags() const { return fast_flags_.editor.get(); }
  bool fast_flags_dirty() const;
  const std::wstring& fast_flags_message() const { return fast_flags_.message; }
  void DeferForTest(std::function<void()> action) { Defer(std::move(action)); }
  const InstallerOutcome& installer_outcome() const { return installer_outcome_; }
  const PromptResult& prompt_result() const { return prompt_result_; }
  int installer_step() const { return installer_.step; }
  const std::wstring& installer_error() const { return installer_.error; }

  HWND AccessibleWindow() const override { return hwnd_; }
  const std::vector<ControlRecord>& AccessibleControls() const override { return records_; }
  float AccessibleScale() const override { return static_cast<float>(dpi_) / 96.0f; }
  int AccessibleFocusIndex() const override;
  void AccessibleActivate(int index) override;
  void AccessibleFocus(int index) override;

 private:
  struct InstallFlow {
    enum class Stage { kIdle, kValidating, kValidated, kRejected, kInstalling, kInstalled, kInstallFailed };
    Stage stage = Stage::kIdle;
    std::wstring file;
    std::uint64_t size = 0;
    ApkProgress progress;
    ApkResult validation;
    ApkResult install;
    std::shared_ptr<std::atomic<bool>> cancel;
  };
  struct UpdateFlow {
    enum class Stage { kIdle, kChecking, kUpToDate, kAvailable, kDownloading, kInstalling, kFailed };
    Stage stage = Stage::kIdle;
    UpdateManifest manifest;
    StagedUpdate staged;
    std::wstring error_title;
    std::wstring error;
    std::uint64_t done = 0;
    std::uint64_t total = 0;
    bool play_after = false;
    bool play_when_checked = false;
    bool waited = false;
    std::shared_ptr<std::atomic<bool>> cancel;
  };
  struct ReleaseHistory {
    enum class Stage { kIdle, kLoading, kLoaded, kFailed };
    Stage stage = Stage::kIdle;
    std::vector<ElixirRelease> list;
    std::wstring error;
  };
  struct CrashView {
    std::vector<CrashLogEntry> entries;
    int selected = -1;
    std::wstring shown_path;
    std::uint64_t shown_size = 0;
    FILETIME shown_modified{};
    std::wstring text;
    float scroll = 0.0f;
    std::wstring error;
  };
  struct RuntimeProcess {
    HANDLE process = nullptr;
    DWORD pid = 0;
    bool running = false;
    bool stopping = false;
    bool exited = false;
    DWORD exit_code = 0;
    HANDLE ipc_pipe = nullptr;
    bool connected = false;
    bool in_game = false;
  };
  struct PendingJoin {
    std::string request;
    ULONGLONG received = 0;
  };
  static constexpr ULONGLONG kPendingJoinMaxAgeMs = 5 * 60 * 1000;
  struct ListMotion {
    struct Row {
      std::wstring key;
      std::size_t index = 0;
      std::wstring text;
      std::wstring detail;
      std::uint32_t serial = 0;
    };
    std::vector<Row> entering;
    std::vector<Row> leaving;
    std::uint32_t serial = 0;
    void Enter(std::wstring key) { entering.push_back(Row{std::move(key), 0, {}, {}, ++serial}); }
    void Leave(std::wstring key, std::size_t index, std::wstring text = {}, std::wstring detail = {}) {
      leaving.push_back(Row{std::move(key), index, std::move(text), std::move(detail), ++serial});
    }
    const Row* Entering(std::wstring_view key) const {
      for (const Row& row : entering) {
        if (row.key == key) return &row;
      }
      return nullptr;
    }
    void Finish(const std::vector<std::uint32_t>& serials) {
      const auto ended = [&serials](const Row& row) {
        return std::find(serials.begin(), serials.end(), row.serial) != serials.end();
      };
      entering.erase(std::remove_if(entering.begin(), entering.end(), ended), entering.end());
      leaving.erase(std::remove_if(leaving.begin(), leaving.end(), ended), leaving.end());
    }
  };
  struct FastFlagsView {
    std::unique_ptr<fflags::FlagEditor> editor;
    std::wstring loaded_path;
    std::wstring load_error;
    std::wstring search;
    bool editor_open = false;
    bool adding = false;
    bool show_preset_flags = false;
    std::set<std::string> selected;
    std::wstring new_name;
    std::wstring new_value;
    std::wstring message;
    int message_status = 4;
    ListMotion motion;
    std::wstring import_error;
    enum class Runtime { kUnknown, kLoading, kLoaded, kUnavailable };
    Runtime runtime = Runtime::kUnknown;
    std::wstring runtime_note;
    std::optional<Page> pending_page;
    bool pending_close = false;
  };
  struct CacheView {
    std::vector<CacheLocation> locations;
    bool measuring = false;
    bool measured = false;
    bool clearing = false;
    std::wstring message;
    int message_status = 4;
  };
  struct RobloxVersionCheck {
    std::shared_ptr<std::atomic<bool>> pending;
    std::optional<RobloxRelease> latest;
    std::wstring error;
  };
  struct RobloxUpdateFlow {
    enum class Stage { kIdle, kRunning, kFailed };
    Stage stage = Stage::kIdle;
    RobloxUpdatePhase phase = RobloxUpdatePhase::kChecking;
    std::string version_name;
    int percent = -1;
    std::wstring error;
    bool chosen = false;
    std::shared_ptr<std::atomic<bool>> cancel;
  };
  struct RobloxVersionList {
    enum class Stage { kIdle, kLoading, kListed, kFailed };
    Stage stage = Stage::kIdle;
    RobloxReleaseList list;
    int selected = -1;
    bool show_all = false;
    bool asked = false;
    std::shared_ptr<std::atomic<bool>> cancel;
  };
  enum class SwitchOutcome { kSwitched, kStaged, kFailed };
  struct OverlayAction {
    std::wstring id;
    std::wstring label;
    ButtonStyle style = ButtonStyle::kStandard;
  };
  struct Overlay {
    bool open = false;
    std::wstring id;
    std::wstring title;
    std::wstring body;
    std::wstring detail;
    int status = 4;
    std::vector<OverlayAction> actions;
  };

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK EditorProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id,
                                     DWORD_PTR data);
  LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
  void ApplyBackdrop();
  void CreateNativeChildren();
  void UpdateNativeFonts();
  void Paint();
  void Invalidate();
  float scale() const { return static_cast<float>(dpi_) / 96.0f; }
  D2D1_POINT_2F ToDips(LPARAM lparam) const;

  void BuildFrame(Ui& ui);
  void BuildInstallerFrame(Ui& ui, float width, float height);
  float BuildInstallerWelcome(Ui& ui, float x, float y, float width);
  float BuildInstallerLocation(Ui& ui, float x, float y, float width);
  float BuildInstallerCompletion(Ui& ui, float x, float y, float width);
  void BuildPromptFrame(Ui& ui, float width, float height);
  void InstallerNext();
  void InstallerBrowse();
  void RunInstall();
  void FinishInstaller(InstallerAction action);
  bool OnInstallerOverlayAction(const std::wstring& id);
  bool ConfirmInstallerClose();
  SIZE WindowSizeForMode(UINT dpi, DWORD style) const;
  DWORD WindowStyleForMode() const;
  void BuildChrome(Ui& ui, float width, float height);
  float BuildPage(Ui& ui, float x, float y, float width);
  float BuildInstall(Ui& ui, float x, float y, float width);
  float BuildSettings(Ui& ui, float x, float y, float width);
  float BuildCrashLogs(Ui& ui, float x, float y, float width);
  float BuildUpdates(Ui& ui, float x, float y, float width);
  float BuildAbout(Ui& ui, float x, float y, float width);
  float BuildAboutCredits(Ui& ui, float x, float y, float width);
  float BuildAboutLicenses(Ui& ui, float x, float y, float width);
  float BuildMods(Ui& ui, float x, float y, float width);
  float BuildShortcuts(Ui& ui, float x, float y, float width);
  float BuildFastFlags(Ui& ui, float x, float y, float width);
  float BuildFramerateLimit(Ui& ui, float x, float y, float width);
  float BuildGlobalSettings(Ui& ui, float x, float y, float width);
  void RefreshGlobalSettings();
  void SaveGlobalValue(GlobalSetting setting, std::string value);
  bool MenuOpen(std::wstring_view control) const { return menu_focus_ >= 0 && menu_control_ == control; }
  void SetMenu(const std::wstring& control, int option, int count);
  void CloseMenu();
  bool FocusedIsComboBox() const;
  int MenuPanel(Ui& ui, const std::wstring& control, const D2D1_RECT_F& button,
                const std::vector<std::wstring>& options, int selected);
  float BuildIntegrations(Ui& ui, float x, float y, float width);
  float BuildCacheCard(Ui& ui, float x, float y, float width);
  float BuildFastFlagEditor(Ui& ui, float x, float y, float width);
  float FastFlagSetting(Ui& ui, float x, float y, float width, const fflags::Preset& preset);
  float FastFlagsWarning(Ui& ui, float x, float y, float width);
  void BuildOverlay(Ui& ui, float width, float height);
  float PageHeader(Ui& ui, float x, float y, float width, std::wstring_view title, std::wstring_view subtitle);
  float Banner(Ui& ui, float x, float y, float width, int status, std::wstring_view title, std::wstring_view body,
               float trailing = 0.0f);
  float SettingRow(Ui& ui, float x, float y, float width, std::wstring_view title, std::wstring_view description,
                   float control_width, float control_height, D2D1_RECT_F* control, bool first);
  void BeginOptions();
  float EndOptions(float y);
  void BuildBottomBar(Ui& ui, float width, float height);
  void UpdateSearch(const std::wstring& query);
  void ClearSearch();
  void GoToSearchHit(int position);
  void BuildSearchResults(Ui& ui);
  float KeyValue(Ui& ui, float x, float y, float width, std::wstring_view key, std::wstring_view value,
                 TextStyle style = TextStyle::kBody, D2D1_COLOR_F color = theme::kText, float key_width = 176.0f);
  float SectionTitle(Ui& ui, float x, float y, float width, std::wstring_view title);

  void Dispatch(const Interaction& interaction);
  const ControlRecord* HitTest(D2D1_POINT_2F point) const;
  const ControlRecord* FindRecord(const std::wstring& id) const;
  void OnMouseMove(D2D1_POINT_2F point);
  void OnMouseDown(D2D1_POINT_2F point);
  void OnMouseUp(D2D1_POINT_2F point);
  void OnKeyDown(WPARAM key);
  void MoveFocus(bool forward);
  void SetFocusTo(const std::wstring& id, bool visible, bool scroll_into_view = true);
  void ScrollBy(float delta);
  void ScrollTo(float scroll);
  void ScrollIntoView(const D2D1_RECT_F& rect);
  bool SmoothScrolling() const;
  void AdvanceScroll();
  void ActivateRecord(const ControlRecord& record);
  void SliderFromPoint(const ControlRecord& record, float x);

  void BeginEdit(const ControlRecord& record);
  void EndEdit(bool commit);
  void PositionNativeChildren();

  void Post(std::function<void()> task);
  static void PostToWindow(HWND hwnd, App* target, std::function<void()> task);
  void RunInBackground(std::function<std::function<void()>()> work);
  void Defer(std::function<void()> action) { deferred_.push_back(std::move(action)); }
  void RunDeferred();
  void UpdateAnimationTimer(bool progress, bool motion);
  void RequestFrame();
  void StopFrameClock();
  std::thread frame_clock_;
  HANDLE frame_event_ = nullptr;
  std::atomic<bool> frame_clock_run_{false};
  void RefreshMotionSetting();
  void RefreshControlAccent();
  void ShowToast(std::wstring text);

  void RefreshAudioDevices();
  void RefreshCrashLogs();
  void SelectCrashLog(int index);
  void DeleteSelectedCrashLog();
  void ChooseApk();
  void StartValidation();
  void StartInstall();
  void Play();
  void StartLauncherServices();
  void ChooseLaunchAction(LaunchMenuAction action);
  void StopRuntime();
  void OpenAnotherWindow();
  int FreeWindowNumber();
  void OnRuntimeExited(DWORD exit_code);
  void OnRuntimeEvent(const RuntimeEvent& event);
  void CheckForUpdates(bool fresh = false);
  void DownloadUpdate();
  void InstallDownloadedUpdate();
  bool UpdateBeforePlaying();
  void PlayFromMenu();
  float BuildVersionHistory(Ui& ui, float x, float y, float width);
  void SaveConfig();
  void SaveConfigNow();
  void OpenPath(const std::wstring& path);
  void OpenFolder(const std::wstring& path);
  void OpenDocument(const wchar_t* relative, const wchar_t* fallback_name);
  std::wstring FindDocument(const wchar_t* relative) const;
  std::wstring FflagsPath() const;
  void ShowOverlay(Overlay overlay);
  void CloseOverlay();
  void OnOverlayAction(const std::wstring& id);
  bool OnRobloxOverlayAction(const std::wstring& id);
  bool OnLinkOverlayAction(const std::wstring& id);
  bool RuntimeRunning();
  bool RuntimeRunningNow();
  bool PackageWorkRunning() const;
  void LoadFastFlags(bool force);
  bool SaveFastFlags();
  void SetFastFlagMessage(std::wstring message, int status);
  void AddFastFlag();
  void ImportFastFlagsFile(bool replace);
  void ExportFastFlags(bool applied_only);
  void OpenFastFlagEditor(bool open);
  void FastFlagsChanged();
  void SaveFastFlagsIfChanged();
  void RefreshRuntimeFlags();
  bool GuardFastFlags(std::optional<Page> next_page, bool closing);
  void MeasureCaches();
  void AskClearCaches();
  void ClearCaches();
  void CheckRobloxVersion(bool notify);
  void StartRobloxUpdate(bool announce, const RobloxRelease* target = nullptr);
  std::wstring RobloxUpdateStatus() const;
  void LoadRobloxVersions();
  void InstallSelectedRobloxVersion();
  void FinishRobloxUpdate(const RobloxUpdateOutcome& outcome);
  bool RobloxUpdateRunning() const { return roblox_update_.stage == RobloxUpdateFlow::Stage::kRunning; }
  bool RobloxUpdatePossible() const;
  using SwitchDone = std::function<void(SwitchOutcome, const std::wstring&)>;
  void SwitchToInstalledVersion(const InstalledApp& next, SwitchDone done);
  void AskRollBackRoblox();
  void RollBackRoblox();
  void LaunchRuntimeNow();
  std::wstring BackupsRoot() const { return JoinPath(paths_.data_root, L"backups"); }
  float BuildRobloxUpdates(Ui& ui, float x, float y, float width);
  float BuildRobloxVersions(Ui& ui, float x, float y, float width);

  void RefreshMods();
  void ChooseCustomFont();
  void ApplyCustomFontFile(const std::wstring& font);
  void RemoveCustomFontMod();
  std::wstring ShortcutFolder(bool desktop) const;
  void RefreshShortcuts();
  void SetElixirShortcut(bool desktop, bool on);
  GameShortcutPlaces ShortcutPlaces() const;
  HttpGetFunction WebGet() const;
  void CreateGameShortcut();
  void RenameGameShortcutTo(const std::wstring& path, const std::wstring& name);
  void RemoveGameShortcut(const std::wstring& path);
  void UpdateGameShortcutsNow(bool force);
  void LoadNotices();

  std::wstring LinkBackupPath() const;
  void RefreshLinkStatus();
  void SetOpenRobloxLinks(bool on);
  void BringToFront();
  void OnLinkRequest(std::string launch_request);
  void SendPendingJoin();
  void DropPendingJoin();
  void OnJoinEvent(const std::string& state);

  struct InstallerView {
    int step = 0;
    int shown_step = 0;
    std::wstring location;
    bool desktop = true;
    bool start_menu = true;
    std::wstring error;
    bool data_found = false;
    bool roblox_installed = false;
    bool own_folder_declined = false;
    bool cancel_confirmed = false;
    bool prompt_checked = false;
    float prompt_height = 0.0f;
  };
  InstallerView installer_;
  InstallerOutcome installer_outcome_;
  PromptResult prompt_result_;
  bool play_after_roblox_install_ = false;

  AppOptions options_;
  LaunchMenu launch_menu_;
  LaunchProgress launch_progress_;
  void ShowLaunchProgress(const std::wstring& message, bool can_cancel);
  void CloseLaunchProgress();
  bool services_started_ = false;
  bool play_from_menu_ = false;
  LauncherPaths paths_;
  ConfigStore store_;
  LauncherConfig config_;
  LoadResult load_result_;
  bool load_notice_ = false;
  std::wstring save_error_;
  ApkComponent apk_;
  InstallFlow install_;
  UpdateFlow update_;
  ReleaseHistory releases_;
  CrashView crash_;
  std::vector<AudioDevice> audio_devices_;
  std::wstring audio_error_;
  std::vector<AudioDevice> audio_input_devices_;
  std::wstring audio_input_error_;
  bool audio_loaded_ = false;
  RuntimeProcess runtime_;
  struct ExtraWindow {
    int number = 0;
    HANDLE process = nullptr;
  };
  std::vector<ExtraWindow> extra_windows_;
  bool opening_window_ = false;
  static constexpr ULONGLONG kRuntimeProbeMaxAgeMs = 2000;
  bool runtime_probe_valid_ = false;
  bool other_runtime_running_ = false;
  bool runtime_probe_pending_ = false;
  ULONGLONG runtime_probe_at_ = 0;
  bool config_dirty_ = false;
  FastFlagsView fast_flags_;
  std::optional<GlobalSettings> global_settings_;
  std::wstring global_settings_error_;
  std::wstring global_quality_override_;
  bool global_data_in_use_ = false;
  static constexpr wchar_t kTextSizeControl[] = L"global.textsize";
  static constexpr std::array<const wchar_t*, 4> kTextSizes{L"Medium", L"Large", L"Larger", L"Largest"};
  std::wstring menu_control_;
  int menu_focus_ = -1;
  int menu_count_ = 0;
  struct PendingMenu {
    std::wstring control;
    D2D1_RECT_F button{};
    std::vector<std::wstring> options;
    int selected = -1;
    std::string preset;
  };
  std::optional<PendingMenu> pending_menu_;
  CacheView cache_;
  struct ModsView {
    ModsSummary summary;
    bool scanning = false;
    bool scanned = false;
    bool working = false;
    std::wstring message;
    int message_status = 4;
  };
  ModsView mods_;
  struct ShortcutsView {
    bool loading = false;
    bool loaded = false;
    bool desktop = false;
    bool start_menu = false;
    std::vector<GameShortcutEntry> games;
    std::wstring game_link;
    bool working = false;
    bool updating = false;
    ULONGLONG updated_at = 0;
    std::wstring message;
    ListMotion motion;
  };
  ShortcutsView shortcuts_;
  int about_tab_ = 0;
  bool notices_loaded_ = false;
  std::vector<NoticeBlock> notices_;
  std::wstring license_text_;
  LinkStatus link_status_;
  std::wstring link_blocker_;
  std::wstring link_message_;
  bool link_status_loaded_ = false;
  std::optional<PendingJoin> pending_join_;
  RobloxVersionCheck roblox_version_;
  RobloxUpdateFlow roblox_update_;
  RobloxVersionList roblox_list_;
  std::optional<InstalledApp> staged_roblox_;
  Overlay overlay_;
  std::wstring overlay_return_focus_;
  std::wstring toast_;
  std::wstring search_text_;
  std::vector<int> search_hits_;
  bool search_enter_ = false;
  std::wstring search_anchor_;
  bool option_cards_ = false;
  bool switching_ = false;
  bool preparing_assets_ = false;
  std::shared_ptr<std::atomic<int>> pending_work_ = std::make_shared<std::atomic<int>>(0);

  HWND hwnd_ = nullptr;
  HWND editor_ = nullptr;
  HFONT editor_font_ = nullptr;
  HBRUSH field_brush_ = nullptr;
  std::wstring editing_original_;
  bool ending_edit_ = false;
  Renderer renderer_;
  bool renderer_ok_ = false;
  UiState state_;
  std::vector<ControlRecord> records_;
  std::vector<Ui::NativeRegionRecord> regions_;
  std::vector<std::function<void()>> deferred_;
  Page page_ = Page::kInstall;
  Page shown_page_ = Page::kInstall;
  std::wstring shown_toast_;
  std::wstring shown_dialog_;
  unsigned page_epoch_ = 0;
  unsigned toast_epoch_ = 0;
  unsigned dialog_epoch_ = 0;
  std::array<float, kPageCount> scroll_{};
  std::array<float, kPageCount> scroll_target_{};
  double scroll_tick_ = 0.0;
  std::array<float, kPageCount> content_height_{};
  D2D1_RECT_F viewport_{};
  D2D1_SIZE_F frame_size_override_{0.0f, 0.0f};
  UINT dpi_ = 96;
  bool mica_ = false;
  bool animation_timer_ = false;
  UINT animation_interval_ = 0;
  ULONGLONG animation_start_ = 0;
  Animator animator_;
  bool motion_enabled_ = true;
  bool motion_active_ = false;
  bool tracking_mouse_ = false;
  std::wstring drag_slider_;
  AccessibleRoot* accessible_ = nullptr;
};

}

#endif
