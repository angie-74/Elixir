// Modified by Elixir.
#include "webview/webview_helper_policy.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

#include "runtime/url_split.h"
#include "runtime/webview_helper_launcher.h"

namespace mocktail {
namespace webview {
namespace {

constexpr std::size_t kMaximumHybridIdentifierBytes = 128;
constexpr std::size_t kMaximumCallbackIdentifierBytes = 1024;
constexpr char kFallbackRobloxVersion[] = "2.725.1142";
constexpr char kBridgeSource[] = R"JS(
(() => {
  "use strict";
  if (window.top !== window) {
    return;
  }
  const host = window.chrome && window.chrome.webview;
  if (!host) {
    return;
  }
  const bridge = {};
  Object.defineProperty(bridge, "executeRoblox", {
    value: (query) => host.postMessage(JSON.parse(query)),
    enumerable: true,
    writable: false,
    configurable: false
  });
  try {
    Object.defineProperty(window, "__globalRobloxAndroidBridge__", {
      value: bridge,
      enumerable: true,
      writable: false,
      configurable: false
    });
  } catch (_) {
  }
})();
)JS";

bool IsSafeVersion(std::string_view version) {
  return !version.empty() && version.size() <= 32 &&
         std::all_of(version.begin(), version.end(), [](unsigned char value) {
           return (value >= '0' && value <= '9') || value == '.' || value == '-';
         });
}

bool ReadBoundedString(const nlohmann::json& object, const char* key, std::size_t maximum_bytes, std::string* value,
                       bool optional = false) {
  const auto found = object.find(key);
  if (found == object.end()) {
    if (optional) {
      value->clear();
      return true;
    }
    return false;
  }
  if (!found->is_string()) {
    return false;
  }
  const auto& text = found->get_ref<const std::string&>();
  if (text.size() > maximum_bytes ||
      std::any_of(text.begin(), text.end(),
                  [](unsigned char character) { return character < 0x20 || character == 0x7f; })) {
    return false;
  }
  *value = text;
  return true;
}

std::string LowerAscii(std::string_view text) {
  std::string lowered(text);
  for (char& character : lowered) {
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
  }
  return lowered;
}

bool IsRobloxHost(std::string_view host) {
  constexpr std::string_view kRoot = "roblox.com";
  const std::string value = LowerAscii(host);
  if (value.size() < kRoot.size()) {
    return false;
  }
  if (value.compare(value.size() - kRoot.size(), kRoot.size(), kRoot) != 0) {
    return false;
  }
  return value.size() == kRoot.size() || value[value.size() - kRoot.size() - 1] == '.';
}

bool IsCookieOctet(unsigned char byte) {
  return byte == 0x21 || (byte >= 0x23 && byte <= 0x2b) || (byte >= 0x2d && byte <= 0x3a) ||
         (byte >= 0x3c && byte <= 0x5b) || (byte >= 0x5d && byte <= 0x7e);
}

}

const char* AndroidBridgeSource() { return kBridgeSource; }

std::string RobloxUserAgentSuffix() {
  const char* configured_version = std::getenv("MOCKTAIL_ROBLOX_VERSION");
  const std::string_view version = configured_version != nullptr && IsSafeVersion(configured_version)
                                       ? std::string_view(configured_version)
                                       : std::string_view(kFallbackRobloxVersion);
  return "ROBLOX Android App " + std::string(version) + " Tablet Hybrid()  GooglePlayStore RobloxApp/" +
         std::string(version) + " (GlobalDist; GooglePlayStore)";
}

bool IsBrowserLoginUrl(std::string_view url) {
  if (!EvaluateNavigationUri(url).allowed) return false;
  runtime::UrlComponents parsed;
  if (!runtime::SplitUrl(url, &parsed)) return false;
  const std::string host = LowerAscii(parsed.host);
  const std::string path = LowerAscii(parsed.path);
  return (host == "www.roblox.com" || host == "roblox.com") && (path == "/login" || path == "/login/");
}

std::string BoundedLogToken(std::string_view value, std::string_view fallback) {
  if (value.empty()) {
    return std::string(fallback);
  }
  constexpr std::size_t kMaximumLogTokenBytes = 128;
  std::string token;
  for (const unsigned char character : value) {
    if (token.size() == kMaximumLogTokenBytes) {
      break;
    }
    token.push_back(character >= 0x21 && character <= 0x7e ? static_cast<char>(character) : '?');
  }
  return token;
}

UriPolicyResult EvaluateNavigationUri(std::string_view uri) {
  UriPolicyResult result;
  if (uri.empty()) {
    return result;
  }
  if (uri == "about:blank") {
    result.allowed = true;
    result.scheme = "about";
    result.host = "none";
    return result;
  }
  runtime::UrlComponents parsed;
  if (!runtime::SplitUrl(uri, &parsed)) {
    return result;
  }
  const std::string scheme = LowerAscii(parsed.scheme);
  result.scheme = BoundedLogToken(scheme, "invalid");
  result.host = BoundedLogToken(parsed.host, "none");
  result.allowed = scheme == "https" && !parsed.host.empty() && !parsed.has_user_info &&
                   (!parsed.has_port || parsed.port == "443");
  result.privileged_bridge_allowed = result.allowed && IsRobloxHost(parsed.host);
  return result;
}

bool FrameNavigationAllowed(std::string_view uri) {
  return uri == "about:srcdoc" || EvaluateNavigationUri(uri).allowed;
}

const char* CaptchaEventName(CaptchaEventType type) {
  return type == CaptchaEventType::kSuccess ? "CaptchaSuccess" : "CaptchaShown";
}

bool ExtractExecuteRobloxCommand(std::string_view message_json, std::string* command) {
  if (command == nullptr || message_json.empty() || message_json.size() > kMaximumHybridCommandBytes) {
    return false;
  }
  const nlohmann::json message =
      nlohmann::json::parse(message_json.begin(), message_json.end(), nullptr, false, false);
  if (message.is_discarded() || !message.is_object()) {
    return false;
  }
  std::string serialized = message.dump();
  if (serialized.empty() || serialized.size() > kMaximumHybridCommandBytes) {
    return false;
  }
  *command = std::move(serialized);
  return true;
}

bool ParseCaptchaEvent(std::string_view command, CaptchaEvent* event) {
  if (event == nullptr || command.empty() || command.size() > kMaximumHybridCommandBytes) {
    return false;
  }
  const nlohmann::json message = nlohmann::json::parse(command.begin(), command.end(), nullptr, false, false);
  if (message.is_discarded() || !message.is_object()) {
    return false;
  }

  std::string module;
  std::string function;
  if (!ReadBoundedString(message, "moduleID", kMaximumHybridIdentifierBytes, &module) ||
      !ReadBoundedString(message, "functionName", kMaximumHybridIdentifierBytes, &function) ||
      module != "Navigation" || function != "navigateToFeature") {
    return false;
  }
  const auto parameters = message.find("params");
  if (parameters == message.end() || !parameters->is_object()) {
    return false;
  }
  const auto navigation = parameters->find("params");
  if (navigation == parameters->end() || !navigation->is_object()) {
    return false;
  }

  std::string feature;
  if (!ReadBoundedString(*navigation, "feature", kMaximumHybridIdentifierBytes, &feature) ||
      !ReadBoundedString(message, "callbackID", kMaximumCallbackIdentifierBytes, &event->callback_id, true)) {
    return false;
  }
  if (feature == "CaptchaShown") {
    event->type = CaptchaEventType::kShown;
    return true;
  }
  if (feature == "CaptchaSuccess") {
    event->type = CaptchaEventType::kSuccess;
    return true;
  }
  return false;
}

std::string BuildCallbackScript(std::string_view callback_id) {
  const std::string quoted_callback = nlohmann::json(callback_id).dump();
  return "(() => { try { if (window.Roblox.Hybrid && "
         "window.Roblox.Hybrid.Bridge.nativeCallback && typeof "
         "window.Roblox.Hybrid.Bridge.nativeCallback === \"function\") { "
         "window.Roblox.Hybrid.Bridge.nativeCallback(" +
         quoted_callback +
         ", true, {}); return true; } } catch (_) {} return false; })();";
}

bool IsRobloxSessionCookie(std::string_view name, std::string_view value, std::string_view domain,
                           std::string_view path, bool secure, bool http_only) {
  if (name != ".ROBLOSECURITY" || path != "/" || !secure || !http_only) return false;
  if (domain != ".roblox.com" && domain != "roblox.com" && domain != "www.roblox.com") return false;
  if (value.empty() || value.size() > runtime::kMaximumWebViewCookieBytes) return false;
  return std::all_of(value.begin(), value.end(),
                     [](char byte) { return IsCookieOctet(static_cast<unsigned char>(byte)); });
}

}
}
