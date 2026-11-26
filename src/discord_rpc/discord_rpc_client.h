// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_DISCORD_RPC_CLIENT_H_
#define DISCORD_RPC_DISCORD_RPC_CLIENT_H_

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "discord_rpc/entities.h"
#include "discord_rpc/helper.h"
#include "discord_rpc/io.h"
#include "discord_rpc/logging.h"
#include "discord_rpc/message.h"
#include "discord_rpc/rpc.h"

namespace discord_rpc {

class DiscordRpcClient final {
 public:
  using OnReadyEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::ReadyMessage>&>;
  using OnCloseEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::CloseMessage>&>;
  using OnErrorEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::ErrorMessage>&>;
  using OnPresenceUpdateEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::PresenceMessage>&>;
  using OnSubscribeEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::SubscribeMessage>&>;
  using OnUnsubscribeEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::UnsubscribeMessage>&>;
  using OnJoinEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::JoinMessage>&>;
  using OnSpectateEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::SpectateMessage>&>;
  using OnJoinRequestedEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::JoinRequestMessage>&>;
  using OnConnectionEstablishedEvent =
      Event<DiscordRpcClient*, const std::shared_ptr<message::ConnectionEstablishedMessage>&>;
  using OnConnectionFailedEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::ConnectionFailedMessage>&>;
  using OnRpcMessageEvent = Event<DiscordRpcClient*, const std::shared_ptr<message::IMessage>&>;

  explicit DiscordRpcClient(const String& application_id);
  DiscordRpcClient(const String& application_id, int pipe, std::shared_ptr<logging::ILogger> logger = nullptr,
                   bool auto_events = true, std::shared_ptr<io::INamedPipeClient> client = nullptr);
  ~DiscordRpcClient();
  DiscordRpcClient(const DiscordRpcClient&) = delete;
  DiscordRpcClient& operator=(const DiscordRpcClient&) = delete;

  bool HasRegisteredUriScheme() const { return has_registered_uri_scheme_.load(); }
  const std::string& ApplicationID() const { return application_id_; }
  const String& SteamID() const { return steam_id_; }
  int ProcessID() const { return process_id_; }
  int MaxQueueSize() const { return 0; }
  bool IsDisposed() const { return is_disposed_.load(); }

  std::shared_ptr<logging::ILogger> Logger() const;
  void SetLogger(std::shared_ptr<logging::ILogger> logger);

  bool AutoEvents() const { return auto_events_; }
  bool SkipIdenticalPresence() const { return skip_identical_presence_.load(); }
  void SetSkipIdenticalPresence(bool value) { skip_identical_presence_ = value; }
  int TargetPipe() const { return target_pipe_; }

  std::shared_ptr<RichPresence> CurrentPresence() const;
  EventType Subscription() const;
  std::shared_ptr<User> CurrentUser() const;
  std::shared_ptr<discord_rpc::Configuration> Configuration() const;
  bool IsInitialized() const { return is_initialized_.load(); }
  bool ShutdownOnly() const { return shutdown_only_.load(); }
  void SetShutdownOnly(bool value);

  OnReadyEvent OnReady;
  OnCloseEvent OnClose;
  OnErrorEvent OnError;
  OnPresenceUpdateEvent OnPresenceUpdate;
  OnSubscribeEvent OnSubscribe;
  OnUnsubscribeEvent OnUnsubscribe;
  OnJoinEvent OnJoin;
  OnSpectateEvent OnSpectate;
  OnJoinRequestedEvent OnJoinRequested;
  OnConnectionEstablishedEvent OnConnectionEstablished;
  OnConnectionFailedEvent OnConnectionFailed;
  OnRpcMessageEvent OnRpcMessage;

  std::vector<std::shared_ptr<message::IMessage>> Invoke();

  void Respond(const std::shared_ptr<message::JoinRequestMessage>& request, bool accept_request);

  void SetPresence(const std::shared_ptr<RichPresence>& presence);

  std::shared_ptr<RichPresence> Update(const std::function<void(RichPresence&)>& func);
  std::shared_ptr<RichPresence> UpdateType(ActivityType type);
  std::shared_ptr<RichPresence> UpdateStatusDisplayType(StatusDisplayType type);
  std::shared_ptr<RichPresence> UpdateButtons(std::shared_ptr<RichPresence::ButtonArray> buttons = nullptr);
  std::shared_ptr<RichPresence> SetButton(std::shared_ptr<Button> button, int index = 0);
  std::shared_ptr<RichPresence> UpdateDetails(const String& details);
  std::shared_ptr<RichPresence> UpdateState(const String& state);
  std::shared_ptr<RichPresence> UpdateParty(std::shared_ptr<Party> party);
  std::shared_ptr<RichPresence> UpdatePartySize(int size);
  std::shared_ptr<RichPresence> UpdatePartySize(int size, int max);
  std::shared_ptr<RichPresence> UpdateLargeAsset(const String& key = std::nullopt, const String& tooltip = std::nullopt);
  std::shared_ptr<RichPresence> UpdateSmallAsset(const String& key = std::nullopt, const String& tooltip = std::nullopt);
  std::shared_ptr<RichPresence> UpdateSecrets(std::shared_ptr<Secrets> secrets);
  std::shared_ptr<RichPresence> UpdateStartTime();
  std::shared_ptr<RichPresence> UpdateStartTime(DateTime time);
  std::shared_ptr<RichPresence> UpdateEndTime();
  std::shared_ptr<RichPresence> UpdateEndTime(DateTime time);
  std::shared_ptr<RichPresence> UpdateClearTime();

  void ClearPresence();

  bool RegisterUriScheme(const String& steam_app_id = std::nullopt, const String& executable = std::nullopt);

  void Subscribe(EventType type);
  void Unsubscribe(EventType type);
  void SetSubscription(EventType type);

  void SynchronizeState();

  bool Initialize();
  void Deinitialize();
  void Dispose();

 private:
  friend struct DiscordRpcClientTestAccess;

  void ProcessMessage(const std::shared_ptr<message::IMessage>& message);
  void SubscribeToTypes(EventType type, bool is_unsubscribe);

  std::atomic<bool> has_registered_uri_scheme_{false};
  std::string application_id_;
  String steam_id_;
  int process_id_ = 0;
  std::atomic<bool> is_disposed_{false};

  mutable std::mutex logger_lock_;
  std::shared_ptr<logging::ILogger> logger_;

  bool auto_events_ = true;
  std::atomic<bool> skip_identical_presence_{true};
  int target_pipe_ = -1;

  std::shared_ptr<RichPresence> current_presence_;
  EventType subscription_ = EventType::None;
  std::shared_ptr<User> current_user_;
  std::shared_ptr<discord_rpc::Configuration> configuration_;
  std::atomic<bool> is_initialized_{false};
  std::atomic<bool> shutdown_only_{true};
  mutable std::recursive_mutex sync_;

  std::unique_ptr<rpc::RpcConnection> connection_;
};

}

#endif
