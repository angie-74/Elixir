// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/message.h"

#include "discord_rpc/exceptions.h"
#include "discord_rpc/rpc.h"

namespace discord_rpc::message {
namespace {

template <typename Handler>
void ReadMessage(const json::Token& token, const char* type, std::vector<std::string_view> names, Handler&& handler) {
  if (!token.is_object()) {
    throw JsonSerializationException(std::string("Cannot deserialize the current JSON value into type '") + type +
                                     "'.");
  }
  const int own = static_cast<int>(names.size());
  names.push_back("Type");
  names.push_back("TimeCreated");
  for (auto member = token.begin(); member != token.end(); ++member) {
    const int index = json::MatchProperty(member.key(), names);
    if (index >= 0 && index < own) handler(index, member.value());
  }
}

EventType EventOf(rpc::ServerEvent evt) {
  switch (evt) {
    default:
    case rpc::ServerEvent::ActivityJoin: return EventType::Join;
    case rpc::ServerEvent::ActivityJoinRequest: return EventType::JoinRequest;
    case rpc::ServerEvent::ActivitySpectate: return EventType::Spectate;
  }
}

const std::vector<json::EnumName>& ErrorCodeNames() {
  static const std::vector<json::EnumName> names = {
      {"Success", 0},         {"PipeException", 1},     {"ReadCorrupt", 2},      {"NotImplemented", 10},
      {"UnknownError", 1000}, {"InvalidPayload", 4000}, {"InvalidCommand", 4002}, {"InvalidEvent", 4004}};
  return names;
}

template <typename Message>
void ReadSecret(const json::Token& token, const char* type, Message* message) {
  ReadMessage(token, type, {"secret"}, [&](int, const json::Token& value) {
    message->Secret = json::ReadAsString(value, json::Source::kToken);
  });
}

}

std::string ToString(MessageType type) {
  static constexpr const char* kNames[] = {"Ready",    "Close",       "Error",                 "PresenceUpdate",
                                           "Subscribe", "Unsubscribe", "Join",                  "Spectate",
                                           "JoinRequest", "ConnectionEstablished", "ConnectionFailed"};
  const int index = static_cast<int>(type);
  return index >= 0 && index < 11 ? kNames[index] : std::to_string(index);
}

std::string ToString(ErrorCode code) {
  for (const json::EnumName& candidate : ErrorCodeNames()) {
    if (candidate.value == static_cast<int>(code)) return candidate.name;
  }
  return std::to_string(static_cast<int>(code));
}

std::shared_ptr<ErrorMessage> ErrorMessage::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto message = std::make_shared<ErrorMessage>();
  ReadMessage(token, "DiscordRPC.Message.ErrorMessage", {"code", "message"}, [&](int index, const json::Token& value) {
    if (index == 0) {
      message->Code = static_cast<ErrorCode>(
          json::Required(json::ReadAsEnum(value, ErrorCodeNames()), "DiscordRPC.Message.ErrorCode"));
    } else {
      message->Message = json::ReadAsString(value, json::Source::kToken);
    }
  });
  return message;
}

std::shared_ptr<JoinMessage> JoinMessage::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto message = std::make_shared<JoinMessage>();
  ReadSecret(token, "DiscordRPC.Message.JoinMessage", message.get());
  return message;
}

std::shared_ptr<SpectateMessage> SpectateMessage::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto message = std::make_shared<SpectateMessage>();
  ReadSecret(token, "DiscordRPC.Message.SpectateMessage", message.get());
  return message;
}

std::shared_ptr<JoinRequestMessage> JoinRequestMessage::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto message = std::make_shared<JoinRequestMessage>();
  ReadMessage(token, "DiscordRPC.Message.JoinRequestMessage", {"user"}, [&](int, const json::Token& value) {
    message->User = discord_rpc::User::FromToken(value);
  });
  return message;
}

PresenceMessage::PresenceMessage(const std::shared_ptr<RichPresenceResponse>& rpr) {
  if (rpr == nullptr) {
    Presence = nullptr;
    Name = "No Rich Presence";
    ApplicationID = "";
  } else {
    Presence = rpr;
    Name = rpr->Name;
    ApplicationID = rpr->ClientID;
  }
}

std::shared_ptr<ReadyMessage> ReadyMessage::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto message = std::make_shared<ReadyMessage>();
  ReadMessage(token, "DiscordRPC.Message.ReadyMessage", {"config", "user", "v"},
              [&](int index, const json::Token& value) {
                if (index == 0) {
                  message->Configuration = discord_rpc::Configuration::FromToken(value);
                } else if (index == 1) {
                  message->User = discord_rpc::User::FromToken(value);
                } else {
                  message->Version = json::Required(json::ReadAsInt32(value, json::Source::kToken), "System.Int32");
                }
              });
  return message;
}

SubscribeMessage::SubscribeMessage(rpc::ServerEvent evt) : Event(EventOf(evt)) {}

UnsubscribeMessage::UnsubscribeMessage(rpc::ServerEvent evt) : Event(EventOf(evt)) {}

}
