// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_MESSAGE_H_
#define DISCORD_RPC_MESSAGE_H_

#include <memory>
#include <string>

#include "discord_rpc/entities.h"
#include "discord_rpc/helper.h"
#include "discord_rpc/json.h"

namespace discord_rpc {

enum class EventType : int {
  None = 0,
  Spectate = 0x1,
  Join = 0x2,
  JoinRequest = 0x4,
};
constexpr EventType operator|(EventType a, EventType b) {
  return static_cast<EventType>(static_cast<int>(a) | static_cast<int>(b));
}
constexpr EventType operator&(EventType a, EventType b) {
  return static_cast<EventType>(static_cast<int>(a) & static_cast<int>(b));
}
constexpr EventType operator~(EventType a) { return static_cast<EventType>(~static_cast<int>(a)); }

namespace rpc {
enum class ServerEvent : int;
}

namespace message {

enum class MessageType {
  Ready,
  Close,
  Error,
  PresenceUpdate,
  Subscribe,
  Unsubscribe,
  Join,
  Spectate,
  JoinRequest,
  ConnectionEstablished,
  ConnectionFailed,
};
std::string ToString(MessageType type);

class IMessage {
 public:
  virtual ~IMessage() = default;

  virtual MessageType Type() const = 0;
  DateTime TimeCreated() const { return timecreated_; }

 protected:
  IMessage() : timecreated_(DateTime::Now()) {}

 private:
  DateTime timecreated_;
};

class CloseMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::Close; }

  String Reason;
  int Code = 0;

  CloseMessage() = default;
  explicit CloseMessage(String reason) : Reason(std::move(reason)) {}
};

class ConnectionEstablishedMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::ConnectionEstablished; }

  int ConnectedPipe = 0;
};

class ConnectionFailedMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::ConnectionFailed; }

  int FailedPipe = 0;
};

enum class ErrorCode {
  Success = 0,
  PipeException = 1,
  ReadCorrupt = 2,

  NotImplemented = 10,

  UnknownError = 1000,
  InvalidPayload = 4000,
  InvalidCommand = 4002,
  InvalidEvent = 4004,
};
std::string ToString(ErrorCode code);

class ErrorMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::Error; }

  ErrorCode Code = ErrorCode::Success;
  String Message;

  static std::shared_ptr<ErrorMessage> FromToken(const json::Token& token);
};

class JoinMessage : public IMessage {
 public:
  MessageType Type() const override { return MessageType::Join; }

  String Secret;

  static std::shared_ptr<JoinMessage> FromToken(const json::Token& token);
};

class SpectateMessage final : public JoinMessage {
 public:
  MessageType Type() const override { return MessageType::Spectate; }

  static std::shared_ptr<SpectateMessage> FromToken(const json::Token& token);
};

class JoinRequestMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::JoinRequest; }

  std::shared_ptr<discord_rpc::User> User;

  static std::shared_ptr<JoinRequestMessage> FromToken(const json::Token& token);
};

class PresenceMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::PresenceUpdate; }

  PresenceMessage() : PresenceMessage(nullptr) {}
  explicit PresenceMessage(const std::shared_ptr<RichPresenceResponse>& rpr);

  std::shared_ptr<BaseRichPresence> Presence;
  String Name;
  String ApplicationID;
};

class ReadyMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::Ready; }

  std::shared_ptr<discord_rpc::Configuration> Configuration;
  std::shared_ptr<discord_rpc::User> User;
  int Version = 0;

  static std::shared_ptr<ReadyMessage> FromToken(const json::Token& token);
};

class SubscribeMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::Subscribe; }

  EventType Event = EventType::None;

  explicit SubscribeMessage(rpc::ServerEvent evt);
};

class UnsubscribeMessage final : public IMessage {
 public:
  MessageType Type() const override { return MessageType::Unsubscribe; }

  EventType Event = EventType::None;

  explicit UnsubscribeMessage(rpc::ServerEvent evt);
};

}
}

#endif
