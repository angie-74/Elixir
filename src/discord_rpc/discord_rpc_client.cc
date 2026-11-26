// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/discord_rpc_client.h"

#include <windows.h>

#include "discord_rpc/exceptions.h"
#include "discord_rpc/registry.h"

namespace discord_rpc {
namespace {

using helper::Args;

constexpr char kDisposedClient[] = "Discord IPC Client";
constexpr char kNoConnection[] = "Cannot initialize as the connection has been deinitialized";

}

DiscordRpcClient::DiscordRpcClient(const String& application_id) : DiscordRpcClient(application_id, -1) {}

DiscordRpcClient::DiscordRpcClient(const String& application_id, int pipe, std::shared_ptr<logging::ILogger> logger,
                                   bool auto_events, std::shared_ptr<io::INamedPipeClient> client) {
  if (helper::IsNullOrEmpty(application_id)) throw ArgumentNullException("applicationID");

  application_id_ = helper::Trim(*application_id);
  target_pipe_ = pipe;
  process_id_ = static_cast<int>(GetCurrentProcessId());
  has_registered_uri_scheme_ = false;
  auto_events_ = auto_events;
  skip_identical_presence_ = true;

  logger_ = logger != nullptr ? std::move(logger) : std::make_shared<logging::NullLogger>();

  connection_ = std::make_unique<rpc::RpcConnection>(
      application_id_, process_id_, target_pipe_,
      client != nullptr ? std::move(client) : std::make_shared<io::ManagedNamedPipeClient>(), auto_events ? 0u : 128u);
  connection_->SetShutdownOnly(shutdown_only_);
  connection_->SetLogger(logger_);

  connection_->OnRpcMessage += [this](rpc::RpcConnection*, const std::shared_ptr<message::IMessage>& msg) {
    if (OnRpcMessage) OnRpcMessage.Invoke(this, msg);

    if (AutoEvents()) ProcessMessage(msg);
  };
}

DiscordRpcClient::~DiscordRpcClient() {
  try {
    Dispose();
  } catch (const Exception&) {
  }
  connection_.reset();
}

std::shared_ptr<logging::ILogger> DiscordRpcClient::Logger() const {
  std::lock_guard<std::mutex> lock(logger_lock_);
  return logger_;
}

void DiscordRpcClient::SetLogger(std::shared_ptr<logging::ILogger> logger) {
  {
    std::lock_guard<std::mutex> lock(logger_lock_);
    logger_ = logger;
  }
  if (connection_ != nullptr) connection_->SetLogger(std::move(logger));
}

std::shared_ptr<RichPresence> DiscordRpcClient::CurrentPresence() const {
  std::lock_guard<std::recursive_mutex> lock(sync_);
  return current_presence_;
}

EventType DiscordRpcClient::Subscription() const {
  std::lock_guard<std::recursive_mutex> lock(sync_);
  return subscription_;
}

std::shared_ptr<User> DiscordRpcClient::CurrentUser() const {
  std::lock_guard<std::recursive_mutex> lock(sync_);
  return current_user_;
}

std::shared_ptr<discord_rpc::Configuration> DiscordRpcClient::Configuration() const {
  std::lock_guard<std::recursive_mutex> lock(sync_);
  return configuration_;
}

void DiscordRpcClient::SetShutdownOnly(bool value) {
  shutdown_only_ = value;
  if (connection_ != nullptr) connection_->SetShutdownOnly(value);
}

std::vector<std::shared_ptr<message::IMessage>> DiscordRpcClient::Invoke() {
  if (AutoEvents()) {
    Logger()->Error("Cannot Invoke client when AutomaticallyInvokeEvents has been set.");
    return {};
  }

  std::vector<std::shared_ptr<message::IMessage>> messages = connection_->DequeueMessages();
  for (const std::shared_ptr<message::IMessage>& message : messages) {
    ProcessMessage(message);
  }

  return messages;
}

void DiscordRpcClient::ProcessMessage(const std::shared_ptr<message::IMessage>& message) {
  if (message == nullptr) return;
  switch (message->Type()) {
    case message::MessageType::PresenceUpdate: {
      const auto pm = std::dynamic_pointer_cast<message::PresenceMessage>(message);
      {
        std::lock_guard<std::recursive_mutex> lock(sync_);
        if (pm != nullptr) {
          if (pm->Presence == nullptr) {
            current_presence_ = nullptr;
          } else if (current_presence_ == nullptr) {
            auto merged = std::make_shared<RichPresence>();
            merged->Merge(*pm->Presence);
            current_presence_ = merged;
          } else {
            current_presence_->Merge(*pm->Presence);
          }

          pm->Presence = current_presence_;
        }
      }

      if (OnPresenceUpdate) OnPresenceUpdate.Invoke(this, pm);
      break;
    }

    case message::MessageType::Ready: {
      const auto rm = std::dynamic_pointer_cast<message::ReadyMessage>(message);
      if (rm != nullptr) {
        {
          std::lock_guard<std::recursive_mutex> lock(sync_);
          configuration_ = rm->Configuration;
          current_user_ = rm->User;
        }

        SynchronizeState();
      }

      if (OnReady) OnReady.Invoke(this, rm);
      break;
    }

    case message::MessageType::Close:
      if (OnClose) OnClose.Invoke(this, std::dynamic_pointer_cast<message::CloseMessage>(message));
      break;

    case message::MessageType::Error:
      if (OnError) OnError.Invoke(this, std::dynamic_pointer_cast<message::ErrorMessage>(message));
      break;

    case message::MessageType::JoinRequest: {
      const auto jrm = std::dynamic_pointer_cast<message::JoinRequestMessage>(message);
      const std::shared_ptr<discord_rpc::Configuration> configuration = Configuration();
      if (configuration != nullptr) {
        if (jrm != nullptr) {
          if (jrm->User == nullptr) throw NullReferenceException();
          jrm->User->SetConfiguration(configuration);
        }
      }

      if (OnJoinRequested) OnJoinRequested.Invoke(this, jrm);
      break;
    }

    case message::MessageType::Subscribe: {
      const auto sub = std::dynamic_pointer_cast<message::SubscribeMessage>(message);
      {
        std::lock_guard<std::recursive_mutex> lock(sync_);
        if (sub == nullptr) throw NullReferenceException();
        subscription_ = subscription_ | sub->Event;
      }

      if (OnSubscribe) OnSubscribe.Invoke(this, sub);
      break;
    }

    case message::MessageType::Unsubscribe: {
      const auto unsub = std::dynamic_pointer_cast<message::UnsubscribeMessage>(message);
      {
        std::lock_guard<std::recursive_mutex> lock(sync_);
        if (unsub == nullptr) throw NullReferenceException();
        subscription_ = subscription_ & ~unsub->Event;
      }

      if (OnUnsubscribe) OnUnsubscribe.Invoke(this, unsub);
      break;
    }

    case message::MessageType::Join:
      if (OnJoin) OnJoin.Invoke(this, std::dynamic_pointer_cast<message::JoinMessage>(message));
      break;

    case message::MessageType::Spectate:
      if (OnSpectate) OnSpectate.Invoke(this, std::dynamic_pointer_cast<message::SpectateMessage>(message));
      break;

    case message::MessageType::ConnectionEstablished:
      if (OnConnectionEstablished) {
        OnConnectionEstablished.Invoke(this, std::dynamic_pointer_cast<message::ConnectionEstablishedMessage>(message));
      }
      break;

    case message::MessageType::ConnectionFailed:
      if (OnConnectionFailed) {
        OnConnectionFailed.Invoke(this, std::dynamic_pointer_cast<message::ConnectionFailedMessage>(message));
      }
      break;

    default:
      Logger()->Error("Message was queued with no appropriate handle! {0}", Args(message->Type()));
      break;
  }
}

void DiscordRpcClient::Respond(const std::shared_ptr<message::JoinRequestMessage>& request, bool accept_request) {
  if (IsDisposed()) throw ObjectDisposedException(kDisposedClient);

  if (connection_ == nullptr) throw ObjectDisposedException("Connection", kNoConnection);

  if (!IsInitialized()) throw UninitializedException();

  if (request == nullptr || request->User == nullptr) throw NullReferenceException();
  auto command = std::make_shared<rpc::RespondCommand>();
  command->Accept = accept_request;
  command->UserID = std::to_string(request->User->ID());
  connection_->EnqueueCommand(command);
}

void DiscordRpcClient::SetPresence(const std::shared_ptr<RichPresence>& presence) {
  if (IsDisposed()) throw ObjectDisposedException(kDisposedClient);

  if (connection_ == nullptr) throw ObjectDisposedException("Connection", kNoConnection);

  if (!IsInitialized()) {
    Logger()->Warning("The client is not yet initialized, storing the presence as a state instead.");
  }

  if (presence == nullptr) {
    if (!SkipIdenticalPresence() || CurrentPresence() != nullptr) {
      auto command = std::make_shared<rpc::PresenceCommand>();
      command->PID = ProcessID();
      command->Presence = nullptr;
      connection_->EnqueueCommand(command);
    }
  } else {
    if (presence->HasSecrets() && !HasRegisteredUriScheme()) {
      throw BadPresenceException(
          "Cannot send a presence with secrets as this object has not registered a URI scheme. Please enable the uri "
          "scheme registration in the DiscordRpcClient constructor.");
    }

    if (presence->HasParty() && presence->Party->Max < presence->Party->Size) {
      throw BadPresenceException("Presence maximum party size cannot be smaller than the current size.");
    }

    if (presence->HasSecrets() && !presence->HasParty()) {
      Logger()->Warning("The presence has set the secrets but no buttons will show as there is no party available.");
    }

    bool send = !SkipIdenticalPresence();
    if (!send) {
      std::lock_guard<std::recursive_mutex> lock(sync_);
      send = !presence->Matches(current_presence_.get());
    }
    if (send) {
      auto command = std::make_shared<rpc::PresenceCommand>();
      command->PID = ProcessID();
      command->Presence = presence->Clone();
      connection_->EnqueueCommand(command);
    }
  }

  std::shared_ptr<RichPresence> clone = presence != nullptr ? presence->Clone() : nullptr;
  std::lock_guard<std::recursive_mutex> lock(sync_);
  current_presence_ = std::move(clone);
}

std::shared_ptr<RichPresence> DiscordRpcClient::Update(const std::function<void(RichPresence&)>& func) {
  if (!IsInitialized()) throw UninitializedException();

  std::shared_ptr<RichPresence> presence;
  {
    std::lock_guard<std::recursive_mutex> lock(sync_);
    presence = current_presence_ == nullptr ? std::make_shared<RichPresence>() : current_presence_->Clone();
  }

  func(*presence);
  SetPresence(presence);
  return presence;
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateType(ActivityType type) {
  return Update([&](RichPresence& p) { p.Type = type; });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateStatusDisplayType(StatusDisplayType type) {
  return Update([&](RichPresence& p) { p.StatusDisplay = type; });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateButtons(std::shared_ptr<RichPresence::ButtonArray> buttons) {
  return Update([&](RichPresence& p) { p.Buttons = buttons; });
}

std::shared_ptr<RichPresence> DiscordRpcClient::SetButton(std::shared_ptr<Button> button, int index) {
  return Update([&](RichPresence& p) {
    if (p.Buttons == nullptr) throw NullReferenceException();
    if (index < 0 || static_cast<std::size_t>(index) >= p.Buttons->size()) throw IndexOutOfRangeException();
    (*p.Buttons)[static_cast<std::size_t>(index)] = button;
  });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateDetails(const String& details) {
  return Update([&](RichPresence& p) { p.SetDetails(details); });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateState(const String& state) {
  return Update([&](RichPresence& p) { p.SetState(state); });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateParty(std::shared_ptr<Party> party) {
  return Update([&](RichPresence& p) { p.Party = party; });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdatePartySize(int size) {
  return Update([&](RichPresence& p) {
    if (p.Party == nullptr) throw BadPresenceException("Cannot set the size of the party if the party does not exist");

    p.Party->Size = size;
  });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdatePartySize(int size, int max) {
  return Update([&](RichPresence& p) {
    if (p.Party == nullptr) throw BadPresenceException("Cannot set the size of the party if the party does not exist");

    p.Party->Size = size;
    p.Party->Max = max;
  });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateLargeAsset(const String& key, const String& tooltip) {
  return Update([&](RichPresence& p) {
    if (p.Assets == nullptr) p.Assets = std::make_shared<Assets>();

    p.Assets->SetLargeImageKey(key.has_value() ? key : p.Assets->LargeImageKey());
    p.Assets->SetLargeImageText(tooltip.has_value() ? tooltip : p.Assets->LargeImageText());
  });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateSmallAsset(const String& key, const String& tooltip) {
  return Update([&](RichPresence& p) {
    if (p.Assets == nullptr) p.Assets = std::make_shared<Assets>();

    p.Assets->SetSmallImageKey(key.has_value() ? key : p.Assets->SmallImageKey());
    p.Assets->SetSmallImageText(tooltip.has_value() ? tooltip : p.Assets->SmallImageText());
  });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateSecrets(std::shared_ptr<Secrets> secrets) {
  return Update([&](RichPresence& p) { p.Secrets = secrets; });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateStartTime() { return UpdateStartTime(DateTime::UtcNow()); }

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateStartTime(DateTime time) {
  return Update([&](RichPresence& p) {
    if (p.Timestamps == nullptr) p.Timestamps = std::make_shared<Timestamps>();

    p.Timestamps->Start = time;
  });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateEndTime() { return UpdateEndTime(DateTime::UtcNow()); }

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateEndTime(DateTime time) {
  return Update([&](RichPresence& p) {
    if (p.Timestamps == nullptr) p.Timestamps = std::make_shared<Timestamps>();

    p.Timestamps->End = time;
  });
}

std::shared_ptr<RichPresence> DiscordRpcClient::UpdateClearTime() {
  return Update([](RichPresence& p) { p.Timestamps = nullptr; });
}

void DiscordRpcClient::ClearPresence() {
  if (IsDisposed()) throw ObjectDisposedException(kDisposedClient);

  if (!IsInitialized()) throw UninitializedException();

  if (connection_ == nullptr) throw ObjectDisposedException("Connection", kNoConnection);

  SetPresence(nullptr);
}

bool DiscordRpcClient::RegisterUriScheme(const String& steam_app_id, const String& executable) {
  registry::SchemeInfo info;
  info.ApplicationID = ApplicationID();
  info.SteamAppID = steam_app_id;
  if (executable.has_value()) {
    info.ExecutablePath = executable;
  } else {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    info.ExecutablePath = helper::FromWide(path);
  }
  const bool registered = registry::UriScheme::Register(info, Logger());
  has_registered_uri_scheme_ = registered;
  return registered;
}

void DiscordRpcClient::Subscribe(EventType type) { SetSubscription(Subscription() | type); }

void DiscordRpcClient::Unsubscribe(EventType type) { SetSubscription(Subscription() & ~type); }

void DiscordRpcClient::SetSubscription(EventType type) {
  if (IsInitialized()) {
    SubscribeToTypes(Subscription() & ~type, true);
    SubscribeToTypes(~Subscription() & type, false);
  } else {
    Logger()->Warning(
        "Client has not yet initialized, but events are being subscribed too. Storing them as state instead.");
  }

  std::lock_guard<std::recursive_mutex> lock(sync_);
  subscription_ = type;
}

void DiscordRpcClient::SubscribeToTypes(EventType type, bool is_unsubscribe) {
  if (type == EventType::None) return;

  if (IsDisposed()) throw ObjectDisposedException(kDisposedClient);

  if (!IsInitialized()) throw UninitializedException();

  if (connection_ == nullptr) throw ObjectDisposedException("Connection", kNoConnection);

  if (!HasRegisteredUriScheme()) {
    throw InvalidConfigurationException(
        "Cannot subscribe/unsubscribe to an event as this application has not registered a URI Scheme. Call "
        "RegisterUriScheme().");
  }

  const auto enqueue = [&](rpc::ServerEvent event) {
    auto command = std::make_shared<rpc::SubscribeCommand>();
    command->Event = event;
    command->IsUnsubscribe = is_unsubscribe;
    connection_->EnqueueCommand(command);
  };
  if ((type & EventType::Spectate) == EventType::Spectate) enqueue(rpc::ServerEvent::ActivitySpectate);

  if ((type & EventType::Join) == EventType::Join) enqueue(rpc::ServerEvent::ActivityJoin);

  if ((type & EventType::JoinRequest) == EventType::JoinRequest) enqueue(rpc::ServerEvent::ActivityJoinRequest);
}

void DiscordRpcClient::SynchronizeState() {
  if (!IsInitialized()) throw UninitializedException();

  SetPresence(CurrentPresence());
  if (HasRegisteredUriScheme()) SubscribeToTypes(Subscription(), false);
}

bool DiscordRpcClient::Initialize() {
  if (IsDisposed()) throw ObjectDisposedException(kDisposedClient);

  if (IsInitialized()) throw UninitializedException("Cannot initialize a client that is already initialized");

  if (connection_ == nullptr) throw ObjectDisposedException("Connection", kNoConnection);

  const bool initialized = connection_->AttemptConnection();
  is_initialized_ = initialized;
  return initialized;
}

void DiscordRpcClient::Deinitialize() {
  if (!IsInitialized()) {
    throw UninitializedException("Cannot deinitialize a client that has not been initalized.");
  }

  connection_->Close();
  is_initialized_ = false;
}

void DiscordRpcClient::Dispose() {
  if (IsDisposed()) return;
  if (IsInitialized()) Deinitialize();
  is_disposed_ = true;
}

}
