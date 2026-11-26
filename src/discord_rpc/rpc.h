// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_RPC_H_
#define DISCORD_RPC_RPC_H_

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "discord_rpc/entities.h"
#include "discord_rpc/helper.h"
#include "discord_rpc/io.h"
#include "discord_rpc/json.h"
#include "discord_rpc/logging.h"
#include "discord_rpc/message.h"

namespace discord_rpc {

template <typename... T>
class Event {
 public:
  using Handler = std::function<void(T...)>;

  Event& operator+=(Handler handler) {
    std::lock_guard<std::mutex> lock(lock_);
    handlers_.push_back(std::move(handler));
    return *this;
  }

  explicit operator bool() const {
    std::lock_guard<std::mutex> lock(lock_);
    return !handlers_.empty();
  }

  void Invoke(T... args) const {
    std::vector<Handler> handlers;
    {
      std::lock_guard<std::mutex> lock(lock_);
      handlers = handlers_;
    }
    for (const Handler& handler : handlers) handler(args...);
  }

 private:
  mutable std::mutex lock_;
  std::vector<Handler> handlers_;
};

namespace rpc {

enum class Command : int {
  Dispatch,
  SetActivity,
  Subscribe,
  Unsubscribe,
  SendActivityJoinInvite,
  CloseActivityJoinRequest,
  Authorize,
  Authenticate,
  GetGuild,
  GetGuilds,
  GetChannel,
  GetChannels,
  SetUserVoiceSettings,
  SelectVoiceChannel,
  GetSelectedVoiceChannel,
  SelectTextChannel,
  GetVoiceSettings,
  SetVoiceSettings,
  CaptureShortcut,
};
std::string ToString(Command command);

enum class ServerEvent : int {
  Ready,
  Error,
  ActivityJoin,
  ActivitySpectate,
  ActivityJoinRequest,
};
std::string ToString(ServerEvent event);

std::string WireName(Command command);
std::string WireName(ServerEvent event);

class IPayload {
 public:
  virtual ~IPayload() = default;

  rpc::Command Command = rpc::Command::Dispatch;
  String Nonce;

  virtual std::string ToJson() const = 0;
  virtual std::string ToString() const;

 protected:
  IPayload() = default;
  explicit IPayload(long long nonce) : Nonce(std::to_string(nonce)) {}
};

class ArgumentPayload final : public IPayload {
 public:
  String Arguments;

  ArgumentPayload() = default;
  explicit ArgumentPayload(long long nonce) : IPayload(nonce) {}
  ArgumentPayload(const std::string& args, long long nonce) : IPayload(nonce) { SetObject(args); }

  void SetObject(const std::string& json) { Arguments = json; }

  std::string ToJson() const override;
  std::string ToString() const override;
};

class EventPayload final : public IPayload {
 public:
  std::optional<json::Token> Data;
  std::optional<ServerEvent> Event;

  EventPayload() = default;
  explicit EventPayload(long long nonce) : IPayload(nonce) {}

  std::string ToJson() const override;
  std::string ToString() const override;

  static std::shared_ptr<EventPayload> FromJson(std::string_view text);
  static std::shared_ptr<EventPayload> FromToken(const json::Token& token);
};

class ClosePayload final : public IPayload {
 public:
  int Code = -1;
  String Reason = std::string();

  std::string ToJson() const override;

  static std::shared_ptr<ClosePayload> FromJson(std::string_view text);
};

class ICommand {
 public:
  virtual ~ICommand() = default;

  virtual std::shared_ptr<IPayload> PreparePayload(long long nonce) = 0;
  virtual const char* TypeName() const = 0;
};

class PresenceCommand final : public ICommand {
 public:
  int PID = 0;
  std::shared_ptr<RichPresence> Presence;

  std::shared_ptr<IPayload> PreparePayload(long long nonce) override;
  const char* TypeName() const override { return "DiscordRPC.RPC.Commands.PresenceCommand"; }
  std::string ToJson() const;
};

class CloseCommand final : public ICommand {
 public:
  int PID = 0;
  std::string value = "Unity 5.5 doesn't handle thread aborts. Can you please close me discord?";

  std::shared_ptr<IPayload> PreparePayload(long long nonce) override;
  const char* TypeName() const override { return "DiscordRPC.RPC.Commands.CloseCommand"; }
};

class RespondCommand final : public ICommand {
 public:
  String UserID;
  bool Accept = false;

  std::shared_ptr<IPayload> PreparePayload(long long nonce) override;
  const char* TypeName() const override { return "DiscordRPC.RPC.Commands.RespondCommand"; }
};

class SubscribeCommand final : public ICommand {
 public:
  ServerEvent Event = ServerEvent::Ready;
  bool IsUnsubscribe = false;

  std::shared_ptr<IPayload> PreparePayload(long long nonce) override;
  const char* TypeName() const override { return "DiscordRPC.RPC.Commands.SubscribeCommand"; }
};

enum class RpcState {
  Disconnected,
  Connecting,
  Connected,
};
std::string ToString(RpcState state);

class RpcConnection {
 public:
  static constexpr int VERSION = 1;
  static constexpr int POLL_RATE = 1000;

  using OnRpcMessageEvent = Event<RpcConnection*, const std::shared_ptr<message::IMessage>&>;

  RpcConnection(std::string application_id, int process_id, int target_pipe,
                std::shared_ptr<io::INamedPipeClient> client, std::uint32_t max_rx_queue_size = 128,
                std::uint32_t max_rt_queue_size = 512);
  ~RpcConnection();
  RpcConnection(const RpcConnection&) = delete;
  RpcConnection& operator=(const RpcConnection&) = delete;

  std::shared_ptr<logging::ILogger> Logger() const;
  void SetLogger(std::shared_ptr<logging::ILogger> logger);

  OnRpcMessageEvent OnRpcMessage;

  RpcState State() const;
  std::shared_ptr<discord_rpc::Configuration> Configuration() const;
  bool IsRunning() const { return thread_started_.load(); }
  bool ShutdownOnly() const { return shutdown_only_.load(); }
  void SetShutdownOnly(bool value) { shutdown_only_ = value; }

  void EnqueueCommand(const std::shared_ptr<ICommand>& command);
  std::shared_ptr<message::IMessage> DequeueMessage();
  std::vector<std::shared_ptr<message::IMessage>> DequeueMessages();

  bool AttemptConnection();
  void Shutdown();
  void Close();
  void Dispose();

 private:
  long long GetNextNonce();
  void EnqueueMessage(const std::shared_ptr<message::IMessage>& message);
  void MainLoop();
  void Connection();
  void ProcessFrame(const std::shared_ptr<EventPayload>& response);
  void ProcessDispatch(const std::shared_ptr<EventPayload>& response);
  void ProcessCommandQueue();
  void EstablishHandshake();
  void SendHandwave();
  void SetConnectionState(RpcState state);

  mutable std::mutex logger_lock_;
  std::shared_ptr<logging::ILogger> logger_;

  RpcState state_ = RpcState::Disconnected;
  mutable std::recursive_mutex l_states_;

  std::shared_ptr<discord_rpc::Configuration> configuration_;
  mutable std::recursive_mutex l_config_;

  std::atomic<bool> aborting_{false};
  std::atomic<bool> shutdown_{false};
  std::atomic<bool> shutdown_only_{true};

  const std::string application_id_;
  const int process_id_;
  long long nonce_ = 0;

  std::thread thread_;
  std::atomic<bool> thread_started_{false};
  const std::shared_ptr<io::INamedPipeClient> named_pipe_;

  const int target_pipe_;

  std::recursive_mutex l_rtqueue_;
  const std::uint32_t max_rt_queue_size_;
  std::deque<std::shared_ptr<ICommand>> rtqueue_;

  std::recursive_mutex l_rxqueue_;
  const std::uint32_t max_rx_queue_size_;
  std::deque<std::shared_ptr<message::IMessage>> rxqueue_;

  void* queue_updated_event_ = nullptr;
  void* released_ = nullptr;
  helper::BackoffDelay delay_;
};

}
}

#endif
