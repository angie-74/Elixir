// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_ROBLOX_EXPERIENCE_LAUNCH_REQUEST_H_
#define MOCKTAIL_RUNTIME_ROBLOX_EXPERIENCE_LAUNCH_REQUEST_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "mocktail/status.h"

namespace mocktail {
namespace runtime {

inline constexpr std::size_t kMaximumExperienceLaunchJsonBytes = 64 * 1024;

struct RobloxExperienceLaunchRequest {
  int64_t place_id = 0;
  int64_t user_id = 0;
  int64_t conversation_id = 0;
  int64_t referred_by_player_id = 0;
  std::string game_instance_id;
  std::string reserved_server_access_code;
  std::string call_id;
  std::string referral_page;
  std::string access_code;
  std::string link_code;
  std::string launch_data;
  std::string event_id;
  std::string game_join_context;
  std::string join_attempt_id;
  std::string join_attempt_origin;
  std::string iso_context;
  std::string canonical_json;
};

Status ParseRobloxExperienceLaunchJson(const std::string &json,
                                       RobloxExperienceLaunchRequest *request);

}
}

#endif
