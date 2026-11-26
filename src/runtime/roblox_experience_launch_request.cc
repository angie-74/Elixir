// Modified by Elixir.
#include "runtime/roblox_experience_launch_request.h"

#include <nlohmann/json.hpp>

#include <limits>
#include <string>
#include <utility>

namespace mocktail {
namespace runtime {
namespace {

constexpr std::size_t kMaximumLaunchStringBytes = 16 * 1024;

Status Invalid(std::string message) {
  return Status::Error(StatusCode::kInvalidArgument, std::move(message));
}

Status ReadOptionalInteger(const nlohmann::json &object, const char *name,
                           int64_t *value) {
  const auto found = object.find(name);
  if (found == object.end() || found->is_null()) {
    return Status::Ok();
  }
  if (!found->is_number_integer()) {
    return Invalid(std::string("experience launch field is not an integer: ") +
                   name);
  }
  if (found->is_number_unsigned() &&
      found->get<uint64_t>() >
          static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    return Invalid(std::string("experience launch integer is out of range: ") +
                   name);
  }
  *value = found->get<int64_t>();
  return Status::Ok();
}

Status ReadOptionalString(const nlohmann::json &object, const char *name,
                          std::string *value) {
  const auto found = object.find(name);
  if (found == object.end() || found->is_null()) {
    return Status::Ok();
  }
  if (!found->is_string()) {
    return Invalid(std::string("experience launch field is not a string: ") +
                   name);
  }
  if (found->get_ref<const std::string &>().size() >
      kMaximumLaunchStringBytes) {
    return Invalid(std::string("experience launch field is too large: ") +
                   name);
  }
  *value = found->get<std::string>();
  return Status::Ok();
}

}

Status ParseRobloxExperienceLaunchJson(const std::string &json,
                                       RobloxExperienceLaunchRequest *request) {
  if (request == nullptr) {
    return Invalid("experience launch output is null");
  }
  *request = {};
  if (json.empty() || json.size() > kMaximumExperienceLaunchJsonBytes) {
    return Invalid("experience launch JSON size is invalid");
  }

  const nlohmann::json document =
      nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
  if (document.is_discarded() || !document.is_object()) {
    return Invalid("experience launch payload is not a JSON object");
  }

  RobloxExperienceLaunchRequest parsed;
  Status status = ReadOptionalInteger(document, "placeId", &parsed.place_id);
  if (status.ok()) {
    status = ReadOptionalInteger(document, "userId", &parsed.user_id);
  }
  if (status.ok()) {
    status = ReadOptionalInteger(document, "conversationId",
                                 &parsed.conversation_id);
  }
  if (status.ok()) {
    status = ReadOptionalInteger(document, "referredByPlayerId",
                                 &parsed.referred_by_player_id);
  }
  struct StringField {
    const char *name;
    std::string RobloxExperienceLaunchRequest::*member;
  };
  constexpr StringField kStringFields[] = {
      {"gameInstanceId", &RobloxExperienceLaunchRequest::game_instance_id},
      {"reservedServerAccessCode",
       &RobloxExperienceLaunchRequest::reserved_server_access_code},
      {"callId", &RobloxExperienceLaunchRequest::call_id},
      {"referralPage", &RobloxExperienceLaunchRequest::referral_page},
      {"accessCode", &RobloxExperienceLaunchRequest::access_code},
      {"linkCode", &RobloxExperienceLaunchRequest::link_code},
      {"launchData", &RobloxExperienceLaunchRequest::launch_data},
      {"eventId", &RobloxExperienceLaunchRequest::event_id},
      {"gameJoinContext", &RobloxExperienceLaunchRequest::game_join_context},
      {"joinAttemptId", &RobloxExperienceLaunchRequest::join_attempt_id},
      {"joinAttemptOrigin",
       &RobloxExperienceLaunchRequest::join_attempt_origin},
      {"isoContext", &RobloxExperienceLaunchRequest::iso_context},
  };
  for (const StringField &field : kStringFields) {
    if (!status.ok()) {
      break;
    }
    status = ReadOptionalString(document, field.name, &(parsed.*field.member));
  }
  if (!status.ok()) {
    return status;
  }
  if (parsed.place_id <= 0) {
    return Invalid("experience launch requires a positive placeId");
  }
  if (parsed.user_id < 0 || parsed.conversation_id < 0 ||
      parsed.referred_by_player_id < 0) {
    return Invalid("experience launch identifiers cannot be negative");
  }
  parsed.canonical_json = document.dump();
  if (parsed.canonical_json.size() > kMaximumExperienceLaunchJsonBytes) {
    return Invalid("canonical experience launch JSON is too large");
  }
  *request = std::move(parsed);
  return Status::Ok();
}

}
}
