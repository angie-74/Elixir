// Modified by Elixir.
#ifndef MOCKTAIL_WEBVIEW_WEBVIEW_HELPER_POLICY_H_
#define MOCKTAIL_WEBVIEW_WEBVIEW_HELPER_POLICY_H_

#include <cstddef>
#include <string>
#include <string_view>

namespace mocktail {
namespace webview {

inline constexpr std::size_t kMaximumHybridCommandBytes = 64 * 1024;

enum class CaptchaEventType {
  kShown,
  kSuccess,
};

struct CaptchaEvent {
  CaptchaEventType type = CaptchaEventType::kShown;
  std::string callback_id;
};

struct UriPolicyResult {
  bool allowed = false;
  bool privileged_bridge_allowed = false;
  std::string scheme = "invalid";
  std::string host = "none";
};

const char* AndroidBridgeSource();
std::string RobloxUserAgentSuffix();
bool IsBrowserLoginUrl(std::string_view url);
std::string BoundedLogToken(std::string_view value, std::string_view fallback);
UriPolicyResult EvaluateNavigationUri(std::string_view uri);
bool FrameNavigationAllowed(std::string_view uri);
const char* CaptchaEventName(CaptchaEventType type);
bool ExtractExecuteRobloxCommand(std::string_view message_json, std::string* command);
bool ParseCaptchaEvent(std::string_view command, CaptchaEvent* event);
std::string BuildCallbackScript(std::string_view callback_id);
bool IsRobloxSessionCookie(std::string_view name, std::string_view value, std::string_view domain,
                           std::string_view path, bool secure, bool http_only);

}
}

#endif
