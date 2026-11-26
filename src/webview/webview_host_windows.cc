// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The window's behaviour follows Mocktail's WebKitGTK helper (Apache-2.0), on Microsoft Edge WebView2.

#include "webview/webview_host_windows.h"

#include <windows.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <wrl.h>

#include <WebView2.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "runtime/webview_helper_launcher.h"
#include "webview/webview_helper_policy.h"

namespace elixir {
namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using mocktail::runtime::WebViewHelperControlCommand;
using mocktail::runtime::WebViewHelperControlOperation;
using mocktail::runtime::WebViewHelperEventType;
using mocktail::webview::BoundedLogToken;
using mocktail::webview::CaptchaEvent;
using mocktail::webview::CaptchaEventName;
using mocktail::webview::CaptchaEventType;
using mocktail::webview::EvaluateNavigationUri;
using mocktail::webview::UriPolicyResult;

constexpr wchar_t kWindowClass[] = L"ElixirWebView";
constexpr UINT kControlPacketMessage = WM_APP + 1;
constexpr UINT kChannelClosedMessage = WM_APP + 2;
constexpr UINT_PTR kCookieTimer = 1;
constexpr UINT kCookieTimerMs = 1000;
constexpr std::size_t kMaximumRequestBytes = mocktail::runtime::kMaximumWebViewUrlBytes + 64;
constexpr DWORD kWriteTimeoutMs = 2000;
constexpr int kPrimaryWidth = 1100;
constexpr int kPrimaryHeight = 760;
constexpr int kPopupWidth = 900;
constexpr int kPopupHeight = 720;

enum class PendingCookie { kNone, kRetain, kSet, kClear };

void Log(const char* text) {
  std::fprintf(stderr, "[webview] %s\n", text);
  std::fflush(stderr);
}

void Log(const std::string& text) { Log(text.c_str()); }

void Wipe(std::string* value) {
  if (!value->empty()) SecureZeroMemory(value->data(), value->size());
  value->clear();
}

void Wipe(std::wstring* value) {
  if (!value->empty()) SecureZeroMemory(value->data(), value->size() * sizeof(wchar_t));
  value->clear();
}

std::wstring Widen(std::string_view text) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                       nullptr, 0);
  if (size <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size);
  return wide;
}

std::string Narrow(std::wstring_view text) {
  if (text.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr,
                                       nullptr);
  if (size <= 0) return {};
  std::string narrow(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
  return narrow;
}

struct CoString {
  LPWSTR text = nullptr;
  bool sensitive = false;
  ~CoString() {
    if (text == nullptr) return;
    if (sensitive) SecureZeroMemory(text, wcslen(text) * sizeof(wchar_t));
    CoTaskMemFree(text);
  }
  std::string utf8() const { return text != nullptr ? Narrow(text) : std::string(); }
};

struct Host;

struct Surface {
  Host* host = nullptr;
  HWND window = nullptr;
  bool primary = false;
  ComPtr<ICoreWebView2Controller> controller;
  ComPtr<ICoreWebView2> view;
};

struct Host {
  HANDLE control = nullptr;
  HANDLE reader = nullptr;
  ComPtr<ICoreWebView2Environment> environment;
  ComPtr<ICoreWebView2CookieManager> cookies;
  std::vector<std::unique_ptr<Surface>> surfaces;
  Surface* primary = nullptr;
  std::wstring default_user_agent;
  std::wstring title = L"Roblox";
  std::string initial_url;
  std::string pending_url;
  std::string pending_cookie;
  std::string last_reported_cookie;
  PendingCookie pending_cookie_operation = PendingCookie::kNone;
  bool cookie_sync_received = false;
  bool browser_login_mode = false;
  bool back_navigation_disabled = false;
  bool show_domain_as_title = false;
  bool visible = true;
  bool terminating = false;
  bool failed = false;
  bool closed_by_user = false;
};

void BeginTermination(Host* host, const char* reason);
void CreateSurfaceView(Host* host, Surface* surface, ICoreWebView2NewWindowRequestedEventArgs* request,
                       ICoreWebView2Deferral* deferral);

bool WriteEvent(Host* host, WebViewHelperEventType type, std::string_view payload) {
  std::string packet;
  if (host->control == nullptr || !mocktail::runtime::EncodeWebViewHelperEventPacket(type, payload, &packet)) {
    return false;
  }
  OVERLAPPED overlapped{};
  overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (overlapped.hEvent == nullptr) {
    Wipe(&packet);
    return false;
  }
  DWORD written = 0;
  bool ok = WriteFile(host->control, packet.data(), static_cast<DWORD>(packet.size()), &written, &overlapped) != 0;
  if (!ok && GetLastError() == ERROR_IO_PENDING) {
    ok = GetOverlappedResultEx(host->control, &overlapped, &written, kWriteTimeoutMs, FALSE) != 0;
    if (!ok) CancelIoEx(host->control, &overlapped);
  }
  CloseHandle(overlapped.hEvent);
  ok = ok && written == packet.size();
  Wipe(&packet);
  return ok;
}

struct ReaderContext {
  HWND window = nullptr;
  HANDLE control = nullptr;
};

DWORD WINAPI ReadControlChannel(void* context) {
  const std::unique_ptr<ReaderContext> reader(static_cast<ReaderContext*>(context));
  const HWND window = reader->window;
  const HANDLE control = reader->control;
  OVERLAPPED overlapped{};
  overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  std::string buffer(mocktail::runtime::kMaximumWebViewControlPacketBytes, '\0');
  for (;;) {
    DWORD read = 0;
    ResetEvent(overlapped.hEvent);
    bool ok = ReadFile(control, buffer.data(), static_cast<DWORD>(buffer.size()), &read, &overlapped) != 0;
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
      ok = GetOverlappedResult(control, &overlapped, &read, TRUE) != 0;
    }
    if (!ok) {
      if (GetLastError() == ERROR_MORE_DATA) continue;
      break;
    }
    if (read == 0) continue;
    auto* packet = new std::string(buffer.data(), read);
    SecureZeroMemory(buffer.data(), read);
    if (!PostMessageW(window, kControlPacketMessage, 0, reinterpret_cast<LPARAM>(packet))) {
      Wipe(packet);
      delete packet;
      break;
    }
  }
  CloseHandle(overlapped.hEvent);
  PostMessageW(window, kChannelClosedMessage, 0, 0);
  return 0;
}

bool ReadRequest(std::string* request) {
  request->clear();
  const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
  if (input == nullptr || input == INVALID_HANDLE_VALUE) return false;
  char buffer[1024];
  for (;;) {
    DWORD count = 0;
    if (!ReadFile(input, buffer, sizeof(buffer), &count, nullptr)) {
      return GetLastError() == ERROR_BROKEN_PIPE;
    }
    if (count == 0) return true;
    if (request->size() + count > kMaximumRequestBytes) return false;
    request->append(buffer, count);
  }
}

std::wstring UserDataFolder() {
  wchar_t configured[1024] = {};
  const DWORD length = GetEnvironmentVariableW(L"ELIXIR_WEBVIEW_DATA_DIR", configured, 1024);
  if (length > 0 && length < 1024) return configured;
  PWSTR local = nullptr;
  std::wstring folder;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local)) && local != nullptr) {
    folder = std::wstring(local) + L"\\Elixir\\webview";
  }
  if (local != nullptr) CoTaskMemFree(local);
  return folder;
}

Surface* SurfaceOf(HWND window) { return reinterpret_cast<Surface*>(GetWindowLongPtrW(window, GWLP_USERDATA + 0)); }

void ResizeView(Surface* surface) {
  if (surface == nullptr || surface->controller == nullptr) return;
  RECT bounds{};
  GetClientRect(surface->window, &bounds);
  surface->controller->put_Bounds(bounds);
}

std::wstring TitleLabel(std::wstring_view text) {
  constexpr std::size_t kMaximumLabel = 60;
  std::wstring label;
  bool space_pending = false;
  for (const wchar_t character : text) {
    if (character <= 0x20 || character == 0x7f) {
      space_pending = !label.empty();
      continue;
    }
    if (space_pending) label.push_back(L' ');
    space_pending = false;
    label.push_back(character);
    if (label.size() >= kMaximumLabel) {
      label.push_back(L'\u2026');
      break;
    }
  }
  return label;
}

void UpdateTitle(Surface* surface) {
  Host* host = surface->host;
  std::wstring label;
  std::string page_host;
  bool roblox_page = false;
  if (surface->view != nullptr) {
    CoString source;
    if (SUCCEEDED(surface->view->get_Source(&source.text))) {
      const UriPolicyResult policy = EvaluateNavigationUri(source.utf8());
      if (policy.allowed && policy.scheme == "https") {
        page_host = policy.host;
        roblox_page = policy.privileged_bridge_allowed;
      }
    }
  }
  if (host->show_domain_as_title && !page_host.empty()) {
    label = Widen(page_host);
  } else if (surface->primary && !host->title.empty() && host->title != L"Roblox") {
    label = TitleLabel(host->title);
  } else if (roblox_page) {
    CoString title;
    if (SUCCEEDED(surface->view->get_DocumentTitle(&title.text)) && title.text != nullptr) {
      label = TitleLabel(title.text);
    }
  } else if (!page_host.empty()) {
    label = Widen(page_host);
  }
  if (label.empty()) label = L"Roblox";
  SetWindowTextW(surface->window, (L"Elixir (" + label + L")").c_str());
}

void ReportRobloxCookie(Host* host) {
  if (!host->browser_login_mode || host->cookies == nullptr || host->terminating) return;
  host->cookies->GetCookies(
      L"https://www.roblox.com/",
      Callback<ICoreWebView2GetCookiesCompletedHandler>([host](HRESULT result, ICoreWebView2CookieList* list) {
        if (FAILED(result) || list == nullptr || !host->browser_login_mode || host->terminating) return S_OK;
        UINT count = 0;
        list->get_Count(&count);
        std::string candidate;
        for (UINT index = 0; index < count && candidate.empty(); ++index) {
          ComPtr<ICoreWebView2Cookie> cookie;
          if (FAILED(list->GetValueAtIndex(index, &cookie)) || cookie == nullptr) continue;
          CoString name;
          CoString value;
          CoString domain;
          CoString path;
          value.sensitive = true;
          BOOL secure = FALSE;
          BOOL http_only = FALSE;
          cookie->get_Name(&name.text);
          cookie->get_Value(&value.text);
          cookie->get_Domain(&domain.text);
          cookie->get_Path(&path.text);
          cookie->get_IsSecure(&secure);
          cookie->get_IsHttpOnly(&http_only);
          std::string narrow_value = value.utf8();
          if (mocktail::webview::IsRobloxSessionCookie(name.utf8(), narrow_value, domain.utf8(), path.utf8(),
                                                       secure != FALSE, http_only != FALSE)) {
            candidate = narrow_value;
          }
          Wipe(&narrow_value);
        }
        if (!candidate.empty() && candidate != host->last_reported_cookie) {
          if (WriteEvent(host, WebViewHelperEventType::kRobloxCookie, candidate)) {
            Wipe(&host->last_reported_cookie);
            host->last_reported_cookie = candidate;
            Log("browser sign-in session captured");
          } else {
            Log("could not forward browser sign-in session");
          }
        }
        Wipe(&candidate);
        return S_OK;
      }).Get());
}

void ConfigureUserAgent(Host* host, std::string_view url) {
  host->browser_login_mode = mocktail::webview::IsBrowserLoginUrl(url);
  if (host->primary == nullptr || host->primary->view == nullptr) return;
  ComPtr<ICoreWebView2Settings> settings;
  ComPtr<ICoreWebView2Settings2> settings2;
  if (FAILED(host->primary->view->get_Settings(&settings)) || FAILED(settings.As(&settings2))) return;
  if (host->browser_login_mode) {
    settings2->put_UserAgent(host->default_user_agent.c_str());
    SetTimer(host->primary->window, kCookieTimer, kCookieTimerMs, nullptr);
    return;
  }
  KillTimer(host->primary->window, kCookieTimer);
  const std::wstring agent = host->default_user_agent + L" " + Widen(mocktail::webview::RobloxUserAgentSuffix());
  settings2->put_UserAgent(agent.c_str());
}

void StartLoad(Host* host) {
  if (!host->cookie_sync_received || host->pending_cookie_operation != PendingCookie::kNone) return;
  if (host->primary == nullptr || host->primary->view == nullptr) return;
  std::string* target = !host->pending_url.empty() ? &host->pending_url : &host->initial_url;
  if (target->empty()) return;
  ConfigureUserAgent(host, *target);
  const std::wstring wide = Widen(*target);
  Wipe(target);
  if (wide.empty() || FAILED(host->primary->view->Navigate(wide.c_str()))) {
    Log("the page could not be opened");
  }
}

void ApplyPendingCookie(Host* host) {
  if (!host->cookie_sync_received || host->cookies == nullptr) return;
  switch (host->pending_cookie_operation) {
    case PendingCookie::kNone:
      break;
    case PendingCookie::kRetain:
      host->pending_cookie_operation = PendingCookie::kNone;
      Log("in-memory Roblox cookies retained before navigation");
      break;
    case PendingCookie::kClear:
      host->cookies->DeleteCookiesWithDomainAndPath(L".ROBLOSECURITY", L".roblox.com", L"/");
      host->pending_cookie_operation = PendingCookie::kNone;
      Log("Roblox cookie deleted before navigation");
      break;
    case PendingCookie::kSet: {
      std::wstring value = Widen(host->pending_cookie);
      Wipe(&host->pending_cookie);
      ComPtr<ICoreWebView2Cookie> cookie;
      const bool created =
          !value.empty() &&
          SUCCEEDED(host->cookies->CreateCookie(L".ROBLOSECURITY", value.c_str(), L".roblox.com", L"/", &cookie)) &&
          cookie != nullptr;
      Wipe(&value);
      if (!created) {
        BeginTermination(host, "cookie construction failed");
        return;
      }
      cookie->put_IsSecure(TRUE);
      cookie->put_IsHttpOnly(TRUE);
      cookie->put_SameSite(COREWEBVIEW2_COOKIE_SAME_SITE_KIND_LAX);
      if (FAILED(host->cookies->AddOrUpdateCookie(cookie.Get()))) {
        BeginTermination(host, "cookie installation failed");
        return;
      }
      host->pending_cookie_operation = PendingCookie::kNone;
      Log("Roblox cookie installed before navigation");
      break;
    }
  }
  StartLoad(host);
}

void CloseSurface(Surface* surface) {
  if (surface->primary) {
    BeginTermination(surface->host, "primary page closed");
    return;
  }
  DestroyWindow(surface->window);
}

void HandleHybridCommand(Surface* surface, std::string_view command) {
  CaptchaEvent event;
  if (!mocktail::webview::ParseCaptchaEvent(command, &event)) return;
  Log(std::string("hybrid event feature=") + CaptchaEventName(event.type) +
      " callback=" + (event.callback_id.empty() ? "missing" : "yes"));
  if (event.callback_id.empty() || surface->view == nullptr) return;
  const std::wstring script = Widen(mocktail::webview::BuildCallbackScript(event.callback_id));
  const HWND window = surface->window;
  const CaptchaEventType type = event.type;
  surface->view->ExecuteScript(
      script.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>([window, type](HRESULT result, LPCWSTR json) {
                        const bool confirmed = SUCCEEDED(result) && json != nullptr && std::wcscmp(json, L"true") == 0;
                        Log(std::string("hybrid callback feature=") + CaptchaEventName(type) +
                            " confirmed=" + (confirmed ? "yes" : "no"));
                        Surface* target = IsWindow(window) ? SurfaceOf(window) : nullptr;
                        if (confirmed && type == CaptchaEventType::kSuccess && target != nullptr) CloseSurface(target);
                        return S_OK;
                      }).Get());
}

void OnWebMessage(Surface* surface, ICoreWebView2WebMessageReceivedEventArgs* args) {
  Host* host = surface->host;
  CoString sender;
  CoString page;
  if (FAILED(args->get_Source(&sender.text)) || FAILED(surface->view->get_Source(&page.text)) ||
      !EvaluateNavigationUri(sender.utf8()).privileged_bridge_allowed ||
      !EvaluateNavigationUri(page.utf8()).privileged_bridge_allowed) {
    Log("rejected executeRoblox from untrusted origin");
    return;
  }
  CoString json;
  json.sensitive = true;
  std::string command;
  if (FAILED(args->get_WebMessageAsJson(&json.text)) ||
      !mocktail::webview::ExtractExecuteRobloxCommand(json.utf8(), &command)) {
    Log("rejected executeRoblox object command");
    return;
  }
  if (!WriteEvent(host, WebViewHelperEventType::kExecuteRoblox, command)) {
    Log("could not forward executeRoblox command");
  }
  HandleHybridCommand(surface, command);
  Wipe(&command);
}

void ConfigureView(Host* host, Surface* surface) {
  ComPtr<ICoreWebView2Settings> settings;
  if (SUCCEEDED(surface->view->get_Settings(&settings)) && settings != nullptr) {
    settings->put_IsScriptEnabled(TRUE);
    settings->put_IsWebMessageEnabled(TRUE);
    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_IsStatusBarEnabled(FALSE);
    ComPtr<ICoreWebView2Settings2> settings2;
    if (SUCCEEDED(settings.As(&settings2)) && host->default_user_agent.empty()) {
      CoString agent;
      if (SUCCEEDED(settings2->get_UserAgent(&agent.text)) && agent.text != nullptr) {
        host->default_user_agent = agent.text;
      }
    }
    ComPtr<ICoreWebView2Settings4> settings4;
    if (SUCCEEDED(settings.As(&settings4))) {
      settings4->put_IsPasswordAutosaveEnabled(FALSE);
      settings4->put_IsGeneralAutofillEnabled(FALSE);
    }
    ComPtr<ICoreWebView2Settings6> settings6;
    if (SUCCEEDED(settings.As(&settings6))) {
      settings6->put_IsSwipeNavigationEnabled(host->back_navigation_disabled ? FALSE : TRUE);
    }
    if (!surface->primary && settings2 != nullptr && !host->browser_login_mode) {
      const std::wstring agent = host->default_user_agent + L" " + Widen(mocktail::webview::RobloxUserAgentSuffix());
      settings2->put_UserAgent(agent.c_str());
    }
  }
  ComPtr<ICoreWebView2Controller2> controller2;
  if (SUCCEEDED(surface->controller.As(&controller2))) {
    controller2->put_DefaultBackgroundColor(COREWEBVIEW2_COLOR{255, 32, 32, 32});
  }
  surface->view->AddScriptToExecuteOnDocumentCreated(Widen(mocktail::webview::AndroidBridgeSource()).c_str(), nullptr);

  EventRegistrationToken token{};
  surface->view->add_NavigationStarting(
      Callback<ICoreWebView2NavigationStartingEventHandler>(
          [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) {
            CoString uri;
            args->get_Uri(&uri.text);
            const UriPolicyResult policy = EvaluateNavigationUri(uri.utf8());
            if (!policy.allowed) {
              Log("navigation blocked scheme=" + policy.scheme + " host=" + policy.host);
              args->put_Cancel(TRUE);
            } else {
              Log("load stage=started scheme=" + policy.scheme + " host=" + policy.host);
            }
            return S_OK;
          })
          .Get(),
      &token);
  surface->view->add_FrameNavigationStarting(
      Callback<ICoreWebView2NavigationStartingEventHandler>(
          [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) {
            CoString uri;
            args->get_Uri(&uri.text);
            if (!mocktail::webview::FrameNavigationAllowed(uri.utf8())) args->put_Cancel(TRUE);
            return S_OK;
          })
          .Get(),
      &token);
  surface->view->add_NavigationCompleted(
      Callback<ICoreWebView2NavigationCompletedEventHandler>(
          [surface](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) {
            BOOL success = FALSE;
            COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
            args->get_IsSuccess(&success);
            args->get_WebErrorStatus(&status);
            Log(success ? std::string("load stage=finished")
                        : "load failed status=" + std::to_string(static_cast<int>(status)));
            UpdateTitle(surface);
            ReportRobloxCookie(surface->host);
            return S_OK;
          })
          .Get(),
      &token);
  surface->view->add_SourceChanged(
      Callback<ICoreWebView2SourceChangedEventHandler>(
          [surface](ICoreWebView2*, ICoreWebView2SourceChangedEventArgs*) {
            UpdateTitle(surface);
            ReportRobloxCookie(surface->host);
            return S_OK;
          })
          .Get(),
      &token);
  surface->view->add_DocumentTitleChanged(
      Callback<ICoreWebView2DocumentTitleChangedEventHandler>([surface](ICoreWebView2*, IUnknown*) {
        UpdateTitle(surface);
        return S_OK;
      }).Get(),
      &token);
  surface->view->add_WebMessageReceived(
      Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          [surface](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) {
            OnWebMessage(surface, args);
            return S_OK;
          })
          .Get(),
      &token);
  surface->view->add_NewWindowRequested(
      Callback<ICoreWebView2NewWindowRequestedEventHandler>(
          [surface](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) {
            CoString uri;
            args->get_Uri(&uri.text);
            const UriPolicyResult policy = EvaluateNavigationUri(uri.utf8());
            args->put_Handled(TRUE);
            if (!policy.allowed || surface->host->terminating) {
              Log("popup blocked scheme=" + policy.scheme + " host=" + policy.host);
              return S_OK;
            }
            ComPtr<ICoreWebView2Deferral> deferral;
            if (FAILED(args->GetDeferral(&deferral))) return S_OK;
            CreateSurfaceView(surface->host, nullptr, args, deferral.Get());
            return S_OK;
          })
          .Get(),
      &token);
  surface->view->add_WindowCloseRequested(
      Callback<ICoreWebView2WindowCloseRequestedEventHandler>([surface](ICoreWebView2*, IUnknown*) {
        CloseSurface(surface);
        return S_OK;
      }).Get(),
      &token);
  surface->view->add_ProcessFailed(
      Callback<ICoreWebView2ProcessFailedEventHandler>(
          [](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) {
            COREWEBVIEW2_PROCESS_FAILED_KIND kind = COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
            args->get_ProcessFailedKind(&kind);
            Log("web process failed kind=" + std::to_string(static_cast<int>(kind)));
            return S_OK;
          })
          .Get(),
      &token);
}

HWND CreateSurfaceWindow(Surface* surface, int width, int height) {
  const HWND anchor = GetForegroundWindow();
  HMONITOR monitor = MonitorFromWindow(anchor, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info{sizeof(info)};
  GetMonitorInfoW(monitor, &info);
  UINT dpi = 96;
  if (anchor != nullptr) dpi = GetDpiForWindow(anchor);
  if (dpi == 0) dpi = GetDpiForSystem();
  const int work_width = info.rcWork.right - info.rcWork.left;
  const int work_height = info.rcWork.bottom - info.rcWork.top;
  const int scaled_width = std::min(MulDiv(width, static_cast<int>(dpi), 96), work_width);
  const int scaled_height = std::min(MulDiv(height, static_cast<int>(dpi), 96), work_height);
  const int left = info.rcWork.left + (work_width - scaled_width) / 2;
  const int top = info.rcWork.top + (work_height - scaled_height) / 2;
  const HWND window =
      CreateWindowExW(0, kWindowClass, L"Elixir (Roblox)", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, left, top,
                      scaled_width, scaled_height, nullptr, nullptr, GetModuleHandleW(nullptr), surface);
  if (window == nullptr) return nullptr;
  const BOOL dark = TRUE;
  if (FAILED(DwmSetWindowAttribute(window, 20, &dark, sizeof(dark)))) {
    DwmSetWindowAttribute(window, 19, &dark, sizeof(dark));
  }
  const COLORREF caption = RGB(32, 32, 32);
  DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
  return window;
}

void CreateSurfaceView(Host* host, Surface* existing, ICoreWebView2NewWindowRequestedEventArgs* request,
                       ICoreWebView2Deferral* deferral) {
  Surface* surface = existing;
  if (surface == nullptr) {
    auto created = std::make_unique<Surface>();
    created->host = host;
    created->primary = false;
    created->window = CreateSurfaceWindow(created.get(), kPopupWidth, kPopupHeight);
    if (created->window == nullptr) {
      if (deferral != nullptr) deferral->Complete();
      return;
    }
    surface = created.get();
    host->surfaces.push_back(std::move(created));
    ShowWindow(surface->window, SW_SHOWNORMAL);
  }
  ComPtr<ICoreWebView2NewWindowRequestedEventArgs> pending_request(request);
  ComPtr<ICoreWebView2Deferral> pending_deferral(deferral);
  const HWND window = surface->window;
  const auto on_controller = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
      [host, window, pending_request, pending_deferral](HRESULT result, ICoreWebView2Controller* controller) {
        Surface* target = IsWindow(window) ? SurfaceOf(window) : nullptr;
        if (FAILED(result) || controller == nullptr || target == nullptr || host->terminating) {
          if (pending_deferral != nullptr) pending_deferral->Complete();
          if (target != nullptr && target->primary) {
            host->failed = true;
            BeginTermination(host, "the web view could not be created");
          }
          return S_OK;
        }
        target->controller = controller;
        controller->get_CoreWebView2(&target->view);
        if (target->view == nullptr) {
          if (pending_deferral != nullptr) pending_deferral->Complete();
          return S_OK;
        }
        ConfigureView(host, target);
        ResizeView(target);
        controller->put_IsVisible(TRUE);
        if (target->primary) {
          ComPtr<ICoreWebView2_2> view2;
          if (SUCCEEDED(target->view.As(&view2))) view2->get_CookieManager(&host->cookies);
          if (host->cookies == nullptr) {
            host->failed = true;
            BeginTermination(host, "the web view has no cookie store");
            return S_OK;
          }
          ApplyPendingCookie(host);
        } else if (pending_request != nullptr) {
          pending_request->put_NewWindow(target->view.Get());
        }
        if (pending_deferral != nullptr) pending_deferral->Complete();
        if (GetForegroundWindow() == window) controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return S_OK;
      });
  ComPtr<ICoreWebView2Environment10> environment10;
  ComPtr<ICoreWebView2ControllerOptions> options;
  HRESULT started = E_FAIL;
  if (SUCCEEDED(host->environment.As(&environment10)) &&
      SUCCEEDED(environment10->CreateCoreWebView2ControllerOptions(&options)) && options != nullptr &&
      SUCCEEDED(options->put_IsInPrivateModeEnabled(TRUE))) {
    started = environment10->CreateCoreWebView2ControllerWithOptions(window, options.Get(), on_controller.Get());
  }
  if (FAILED(started)) {
    if (pending_deferral != nullptr) pending_deferral->Complete();
    if (surface->primary) {
      host->failed = true;
      BeginTermination(host, "this version of Microsoft Edge WebView2 is too old");
    }
  }
}

void BeginTermination(Host* host, const char* reason) {
  if (host->terminating) return;
  host->terminating = true;
  Log(std::string(reason) + "; closing");
  std::vector<HWND> windows;
  for (const auto& surface : host->surfaces) windows.push_back(surface->window);
  for (const HWND window : windows) {
    if (IsWindow(window)) DestroyWindow(window);
  }
  PostQuitMessage(host->failed ? 1
                               : host->closed_by_user ? mocktail::runtime::kWebViewHelperClosedByUserExitCode : 0);
}

void ApplyControl(Host* host, WebViewHelperControlCommand* command) {
  Surface* primary = host->primary;
  switch (command->operation) {
    case WebViewHelperControlOperation::kLoadUrl:
      Wipe(&host->pending_url);
      host->pending_url = command->payload;
      StartLoad(host);
      break;
    case WebViewHelperControlOperation::kSetTitle:
      host->title = Widen(command->payload);
      if (primary != nullptr) UpdateTitle(primary);
      break;
    case WebViewHelperControlOperation::kSetVisible:
      host->visible = command->visible;
      if (primary != nullptr) {
        ShowWindow(primary->window, command->visible ? SW_SHOW : SW_HIDE);
        if (command->visible) SetForegroundWindow(primary->window);
        if (primary->controller != nullptr) primary->controller->put_IsVisible(command->visible ? TRUE : FALSE);
      }
      break;
    case WebViewHelperControlOperation::kEvaluateJavaScript:
      if (primary != nullptr && primary->view != nullptr) {
        std::wstring script = Widen(command->payload);
        primary->view->ExecuteScript(script.c_str(), nullptr);
        Wipe(&script);
      } else {
        Log("rejected JavaScript control without a page");
      }
      break;
    case WebViewHelperControlOperation::kSetBackNavigationDisabled:
      host->back_navigation_disabled = command->visible;
      if (primary != nullptr && primary->view != nullptr) {
        ComPtr<ICoreWebView2Settings> settings;
        ComPtr<ICoreWebView2Settings6> settings6;
        if (SUCCEEDED(primary->view->get_Settings(&settings)) && SUCCEEDED(settings.As(&settings6))) {
          settings6->put_IsSwipeNavigationEnabled(command->visible ? FALSE : TRUE);
        }
      }
      break;
    case WebViewHelperControlOperation::kSetShowDomainAsTitle:
      host->show_domain_as_title = command->visible;
      if (primary != nullptr) UpdateTitle(primary);
      break;
    case WebViewHelperControlOperation::kSetRobloxCookie:
    case WebViewHelperControlOperation::kClearRobloxCookie:
    case WebViewHelperControlOperation::kRetainPersistentRobloxCookie:
      Wipe(&host->pending_cookie);
      if (command->operation == WebViewHelperControlOperation::kSetRobloxCookie) {
        host->pending_cookie = command->payload;
        host->pending_cookie_operation = PendingCookie::kSet;
      } else if (command->operation == WebViewHelperControlOperation::kClearRobloxCookie) {
        host->pending_cookie_operation = PendingCookie::kClear;
      } else {
        host->pending_cookie_operation = PendingCookie::kRetain;
      }
      host->cookie_sync_received = true;
      ApplyPendingCookie(host);
      break;
    case WebViewHelperControlOperation::kClose:
      BeginTermination(host, "control close received");
      break;
  }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    return DefWindowProcW(window, message, wparam, lparam);
  }
  Surface* surface = SurfaceOf(window);
  if (surface == nullptr) return DefWindowProcW(window, message, wparam, lparam);
  Host* host = surface->host;
  switch (message) {
    case WM_SIZE:
      ResizeView(surface);
      return 0;
    case WM_MOVE:
    case WM_MOVING:
      if (surface->controller != nullptr) surface->controller->NotifyParentWindowPositionChanged();
      break;
    case WM_SETFOCUS:
      if (surface->controller != nullptr) surface->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
      return 0;
    case WM_ERASEBKGND: {
      RECT client{};
      GetClientRect(window, &client);
      const HBRUSH brush = CreateSolidBrush(RGB(32, 32, 32));
      FillRect(reinterpret_cast<HDC>(wparam), &client, brush);
      DeleteObject(brush);
      return 1;
    }
    case WM_TIMER:
      if (wparam == kCookieTimer) ReportRobloxCookie(host);
      return 0;
    case kControlPacketMessage: {
      std::unique_ptr<std::string> packet(reinterpret_cast<std::string*>(lparam));
      WebViewHelperControlCommand command;
      std::string error;
      if (!mocktail::runtime::DecodeWebViewHelperControlPacket(*packet, &command, &error)) {
        Log("rejected malformed control packet: " + error);
      } else if (!host->terminating) {
        ApplyControl(host, &command);
      }
      Wipe(&command.payload);
      Wipe(packet.get());
      return 0;
    }
    case kChannelClosedMessage:
      BeginTermination(host, "control channel closed");
      return 0;
    case WM_CLOSE:
      if (surface->primary && !host->terminating) host->closed_by_user = true;
      CloseSurface(surface);
      return 0;
    case WM_DESTROY: {
      if (surface->controller != nullptr) surface->controller->Close();
      surface->controller.Reset();
      surface->view.Reset();
      SetWindowLongPtrW(window, GWLP_USERDATA, 0);
      const bool primary = surface->primary;
      if (primary) host->primary = nullptr;
      host->surfaces.erase(std::remove_if(host->surfaces.begin(), host->surfaces.end(),
                                          [surface](const std::unique_ptr<Surface>& item) {
                                            return item.get() == surface;
                                          }),
                           host->surfaces.end());
      if (primary && !host->terminating) BeginTermination(host, "primary window closed");
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

HANDLE ControlHandleFromArguments(int argc, wchar_t** argv) {
  for (int index = 1; index + 1 < argc; ++index) {
    if (std::wstring_view(argv[index]) != kWebViewControlFlag) continue;
    wchar_t* end = nullptr;
    const unsigned long long value = std::wcstoull(argv[index + 1], &end, 10);
    if (end == argv[index + 1] || *end != L'\0' || value == 0) return nullptr;
    const HANDLE handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(value));
    DWORD flags = 0;
    if (GetFileType(handle) != FILE_TYPE_PIPE || !GetNamedPipeInfo(handle, &flags, nullptr, nullptr, nullptr) ||
        (flags & PIPE_TYPE_MESSAGE) == 0) {
      return nullptr;
    }
    return handle;
  }
  return nullptr;
}

}

int WebViewHostMain(int argc, wchar_t** argv) {
  Host host;
  std::string request;
  std::string error;
  if (!ReadRequest(&request) || !mocktail::runtime::DecodeWebViewRequest(request, &host.initial_url, &error)) {
    Wipe(&request);
    Log("invalid request: " + (error.empty() ? std::string("cannot read the bounded request") : error));
    return 1;
  }
  Wipe(&request);
  host.control = ControlHandleFromArguments(argc, argv);
  if (host.control == nullptr) {
    Wipe(&host.initial_url);
    Log("invalid control channel");
    return 1;
  }
  DWORD mode = PIPE_READMODE_MESSAGE;
  SetNamedPipeHandleState(host.control, &mode, nullptr, nullptr);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return 1;

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = &WindowProc;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
  window_class.hIconSm = window_class.hIcon;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.lpszClassName = kWindowClass;
  if (!RegisterClassExW(&window_class)) return 1;

  auto primary = std::make_unique<Surface>();
  primary->host = &host;
  primary->primary = true;
  primary->window = CreateSurfaceWindow(primary.get(), kPrimaryWidth, kPrimaryHeight);
  if (primary->window == nullptr) return 1;
  host.primary = primary.get();
  host.surfaces.push_back(std::move(primary));
  ShowWindow(host.primary->window, SW_SHOWNORMAL);
  SetForegroundWindow(host.primary->window);

  const HWND primary_window = host.primary->window;
  auto* reader_context = new ReaderContext{primary_window, host.control};
  host.reader = CreateThread(nullptr, 0, &ReadControlChannel, reader_context, 0, nullptr);
  if (host.reader == nullptr) delete reader_context;
  if (host.reader == nullptr || !WriteEvent(&host, WebViewHelperEventType::kReady, {})) {
    host.failed = true;
    BeginTermination(&host, "could not publish readiness");
  } else {
    const std::wstring folder = UserDataFolder();
    const HRESULT started = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, folder.empty() ? nullptr : folder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&host](HRESULT result, ICoreWebView2Environment* environment) {
              if (FAILED(result) || environment == nullptr || host.terminating || host.primary == nullptr) {
                if (!host.terminating) {
                  host.failed = true;
                  BeginTermination(&host, "Microsoft Edge WebView2 could not start");
                }
                return S_OK;
              }
              host.environment = environment;
              CreateSurfaceView(&host, host.primary, nullptr, nullptr);
              return S_OK;
            })
            .Get());
    if (FAILED(started)) {
      host.failed = true;
      MessageBoxW(primary_window,
                  L"Roblox pages need Microsoft Edge WebView2, which isn’t installed on this PC.", L"Elixir",
                  MB_ICONINFORMATION | MB_OK);
      BeginTermination(&host, "Microsoft Edge WebView2 is not installed");
    }
  }

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  host.cookies.Reset();
  host.environment.Reset();
  if (host.control != nullptr) {
    CancelIoEx(host.control, nullptr);
    CloseHandle(host.control);
    host.control = nullptr;
  }
  if (host.reader != nullptr) {
    WaitForSingleObject(host.reader, 2000);
    CloseHandle(host.reader);
  }
  Wipe(&host.initial_url);
  Wipe(&host.pending_url);
  Wipe(&host.pending_cookie);
  Wipe(&host.last_reported_cookie);
  CoUninitialize();
  return host.failed ? 1 : static_cast<int>(message.wParam);
}

}
