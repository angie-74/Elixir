// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_ROBLOX_LAUNCH_URI_H_
#define MOCKTAIL_RUNTIME_ROBLOX_LAUNCH_URI_H_

#include <cstddef>
#include <string_view>

#include "mocktail/status.h"
#include "runtime/roblox_experience_launch_request.h"

namespace mocktail {
namespace runtime {

inline constexpr std::size_t kMaximumRobloxLaunchUriBytes = 64 * 1024;

Status ParseRobloxLaunchUri(std::string_view uri,
                            RobloxExperienceLaunchRequest* request);

Status ParseRobloxWebViewLaunchGame(std::string_view command,
                                    RobloxExperienceLaunchRequest* request);

}
}

#endif
