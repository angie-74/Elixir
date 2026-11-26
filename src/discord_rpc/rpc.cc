// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/rpc.h"

#include <windows.h>

#include <exception>

#include "discord_rpc/exceptions.h"

namespace discord_rpc::rpc {
namespace {

using helper::Args;

constexpr bool CLEAR_ON_SHUTDOWN = true;
constexpr bool LOCK_STEP = false;

struct CommandName {
  Command command;
  const char* name;
  const char* wire;
};

constexpr CommandName kCommands[] = {
    {Command::Dispatch, "Dispatch", "DISPATCH"},
    {Command::SetActivity, "SetActivity", "SET_ACTIVITY"},
    {Command::Subscribe, "Subscribe", "SUBSCRIBE"},
    {Command::Unsubscribe, "Unsubscribe", "UNSUBSCRIBE"},
    {Command::SendActivityJoinInvite, "SendActivityJoinInvite", "SEND_ACTIVITY_JOIN_INVITE"},
    {Command::CloseActivityJoinRequest, "CloseActivityJoinRequest", "CLOSE_ACTIVITY_JOIN_REQUEST"},
    {Command::Authorize, "Authorize", nullptr},
    {Command::Authenticate, "Authenticate", nullptr},
    {Command::GetGuild, "GetGuild", nullptr},
    {Command::GetGuilds, "GetGuilds", nullptr},
    {Command::GetChannel, "GetChannel", nullptr},
    {Command::GetChannels, "GetChannels", nullptr},
    {Command::SetUserVoiceSettings, "SetUserVoiceSettings", nullptr},
    {Command::SelectVoiceChannel, "SelectVoiceChannel", nullptr},
    {Command::GetSelectedVoiceChannel, "GetSelectedVoiceChannel", nullptr},
    {Command::SelectTextChannel, "SelectTextChannel", nullptr},
    {Command::GetVoiceSettings, "GetVoiceSettings", nullptr},
    {Command::SetVoiceSettings, "SetVoiceSettings", nullptr},
    {Command::CaptureShortcut, "CaptureShortcut", nullptr},
};

struct EventName {
  ServerEvent event;
  const char* name;
  const char* wire;
};

constexpr EventName kEvents[] = {
    {ServerEvent::Ready, "Ready", "READY"},
    {ServerEvent::Error, "Error", "ERROR"},
    {ServerEvent::ActivityJoin, "ActivityJoin", "ACTIVITY_JOIN"},
    {ServerEvent::ActivitySpectate, "ActivitySpectate", "ACTIVITY_SPECTATE"},
    {ServerEvent::ActivityJoinRequest, "ActivityJoinRequest", "ACTIVITY_JOIN_REQUEST"},
};

Command ReadCommand(const json::Token& value, Command existing) {
  if (value.is_null()) throw JsonSerializationException("Error setting value to 'Command'.");
  if (value.is_object() || value.is_array()) throw JsonSerializationException("Error setting value to 'Command'.");
  if (!value.is_string()) throw InvalidCastException("Unable to cast object to type 'System.String'.");
  const std::string& text = value.get_ref<const std::string&>();
  for (const CommandName& candidate : kCommands) {
    if (candidate.wire != nullptr && text == candidate.wire) return candidate.command;
  }
  return existing;
}

std::optional<ServerEvent> ReadEvent(const json::Token& value, const std::optional<ServerEvent>& existing) {
  if (value.is_null()) return std::nullopt;
  if (value.is_object() || value.is_array()) throw JsonSerializationException("Error setting value to 'Event'.");
  if (!value.is_string()) throw InvalidCastException("Unable to cast object to type 'System.String'.");
  const std::string& text = value.get_ref<const std::string&>();
  for (const EventName& candidate : kEvents) {
    if (text == candidate.wire) return candidate.event;
  }
  return existing;
}

std::optional<json::Token> ReadData(const json::Token& value) {
  if (value.is_null()) return std::nullopt;
  if (!value.is_object()) {
    throw JsonSerializationException("Unable to cast the JSON value to type 'Newtonsoft.Json.Linq.JObject'.");
  }
  return value;
}

std::shared_ptr<EventPayload> ReadEventPayload(const json::Token& token, json::Source source) {
  if (token.is_null()) return nullptr;
  if (!token.is_object()) {
    throw JsonSerializationException(
        "Cannot deserialize the current JSON value into type 'DiscordRPC.RPC.Payload.EventPayload'.");
  }
  auto payload = std::make_shared<EventPayload>();
  static const std::vector<std::string_view> names = {"data", "evt", "cmd", "nonce"};
  for (auto member = token.begin(); member != token.end(); ++member) {
    switch (json::MatchProperty(member.key(), names)) {
      case 0: payload->Data = ReadData(member.value()); break;
      case 1: payload->Event = ReadEvent(member.value(), payload->Event); break;
      case 2: payload->Command = ReadCommand(member.value(), payload->Command); break;
      case 3: payload->Nonce = json::ReadAsString(member.value(), source); break;
      default: break;
    }
  }
  return payload;
}

void WriteCommandAndNonce(json::Writer* writer, const IPayload& payload) {
  writer->Name("cmd");
  writer->Text(WireName(payload.Command));
  writer->Name("nonce");
  writer->TextOrNull(payload.Nonce);
}

}

std::string ToString(Command command) {
  for (const CommandName& candidate : kCommands) {
    if (candidate.command == command) return candidate.name;
  }
  return std::to_string(static_cast<int>(command));
}

std::string ToString(ServerEvent event) {
  for (const EventName& candidate : kEvents) {
    if (candidate.event == event) return candidate.name;
  }
  return std::to_string(static_cast<int>(event));
}

std::string WireName(Command command) {
  for (const CommandName& candidate : kCommands) {
    if (candidate.command == command) return candidate.wire != nullptr ? candidate.wire : candidate.name;
  }
  return std::to_string(static_cast<int>(command));
}

std::string WireName(ServerEvent event) {
  for (const EventName& candidate : kEvents) {
    if (candidate.event == event) return candidate.wire;
  }
  return std::to_string(static_cast<int>(event));
}

std::string ToString(RpcState state) {
  switch (state) {
    case RpcState::Disconnected: return "Disconnected";
    case RpcState::Connecting: return "Connecting";
    case RpcState::Connected: return "Connected";
  }
  return std::to_string(static_cast<int>(state));
}

std::string IPayload::ToString() const {
  return "Payload || Command: " + rpc::ToString(Command) + ", Nonce: " + Nonce.value_or("");
}

std::string ArgumentPayload::ToJson() const {
  json::Writer writer;
  writer.StartObject();
  if (Arguments.has_value()) {
    writer.Name("args");
    writer.Raw(*Arguments);
  }
  WriteCommandAndNonce(&writer, *this);
  writer.EndObject();
  return writer.Take();
}

std::string ArgumentPayload::ToString() const { return "Argument " + IPayload::ToString(); }

std::string EventPayload::ToJson() const {
  json::Writer writer;
  writer.StartObject();
  if (Data.has_value()) {
    writer.Name("data");
    writer.Raw(Data->dump(-1, ' ', false, json::Token::error_handler_t::replace));
  }
  writer.Name("evt");
  if (Event.has_value()) {
    writer.Text(WireName(*Event));
  } else {
    writer.Null();
  }
  WriteCommandAndNonce(&writer, *this);
  writer.EndObject();
  return writer.Take();
}

std::string EventPayload::ToString() const {
  return "Event " + IPayload::ToString() + ", Event: " + (Event.has_value() ? rpc::ToString(*Event) : "N/A");
}

std::shared_ptr<EventPayload> EventPayload::FromJson(std::string_view text) {
  const std::optional<json::Token> token = json::Parse(text);
  if (!token.has_value()) return nullptr;
  return ReadEventPayload(*token, json::Source::kText);
}

std::shared_ptr<EventPayload> EventPayload::FromToken(const json::Token& token) {
  return ReadEventPayload(token, json::Source::kToken);
}

std::string ClosePayload::ToJson() const {
  json::Writer writer;
  writer.StartObject();
  writer.Name("code");
  writer.Int(Code);
  writer.Name("message");
  writer.TextOrNull(Reason);
  WriteCommandAndNonce(&writer, *this);
  writer.EndObject();
  return writer.Take();
}

std::shared_ptr<ClosePayload> ClosePayload::FromJson(std::string_view text) {
  const std::optional<json::Token> token = json::Parse(text);
  if (!token.has_value() || token->is_null()) return nullptr;
  if (!token->is_object()) {
    throw JsonSerializationException(
        "Cannot deserialize the current JSON value into type 'DiscordRPC.RPC.Payload.ClosePayload'.");
  }
  auto payload = std::make_shared<ClosePayload>();
  static const std::vector<std::string_view> names = {"code", "message", "cmd", "nonce"};
  for (auto member = token->begin(); member != token->end(); ++member) {
    switch (json::MatchProperty(member.key(), names)) {
      case 0: payload->Code = json::Required(json::ReadAsInt32(member.value(), json::Source::kText), "System.Int32"); break;
      case 1: payload->Reason = json::ReadAsString(member.value(), json::Source::kText); break;
      case 2: payload->Command = ReadCommand(member.value(), payload->Command); break;
      case 3: payload->Nonce = json::ReadAsString(member.value(), json::Source::kText); break;
      default: break;
    }
  }
  return payload;
}

std::string PresenceCommand::ToJson() const {
  json::Writer writer;
  writer.StartObject();
  writer.Name("pid");
  writer.Int(PID);
  writer.Name("activity");
  if (Presence != nullptr) {
    Presence->WriteJson(&writer);
  } else {
    writer.Null();
  }
  writer.EndObject();
  return writer.Take();
}

std::shared_ptr<IPayload> PresenceCommand::PreparePayload(long long nonce) {
  auto payload = std::make_shared<ArgumentPayload>(ToJson(), nonce);
  payload->Command = Command::SetActivity;
  return payload;
}

std::shared_ptr<IPayload> CloseCommand::PreparePayload(long long) {
  auto payload = std::make_shared<ArgumentPayload>();
  payload->Command = Command::Dispatch;
  payload->Nonce = std::nullopt;
  payload->Arguments = std::nullopt;
  return payload;
}

std::shared_ptr<IPayload> RespondCommand::PreparePayload(long long nonce) {
  json::Writer writer;
  writer.StartObject();
  writer.Name("user_id");
  writer.TextOrNull(UserID);
  writer.EndObject();
  auto payload = std::make_shared<ArgumentPayload>(writer.Take(), nonce);
  payload->Command = Accept ? Command::SendActivityJoinInvite : Command::CloseActivityJoinRequest;
  return payload;
}

std::shared_ptr<IPayload> SubscribeCommand::PreparePayload(long long nonce) {
  auto payload = std::make_shared<EventPayload>(nonce);
  payload->Command = IsUnsubscribe ? Command::Unsubscribe : Command::Subscribe;
  payload->Event = Event;
  return payload;
}

RpcConnection::RpcConnection(std::string application_id, int process_id, int target_pipe,
                             std::shared_ptr<io::INamedPipeClient> client, std::uint32_t max_rx_queue_size,
                             std::uint32_t max_rt_queue_size)
    : application_id_(std::move(application_id)),
      process_id_(process_id),
      named_pipe_(std::move(client)),
      target_pipe_(target_pipe),
      max_rt_queue_size_(max_rt_queue_size),
      max_rx_queue_size_(max_rx_queue_size),
      delay_(500, 60 * 1000) {
  queue_updated_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  released_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);

  SetLogger(std::make_shared<logging::ConsoleLogger>());
}

RpcConnection::~RpcConnection() {
  aborting_ = true;
  if (released_ != nullptr) SetEvent(static_cast<HANDLE>(released_));
  if (queue_updated_event_ != nullptr) SetEvent(static_cast<HANDLE>(queue_updated_event_));
  if (named_pipe_ != nullptr) named_pipe_->Release();
  if (thread_.joinable()) thread_.join();
  if (queue_updated_event_ != nullptr) CloseHandle(static_cast<HANDLE>(queue_updated_event_));
  if (released_ != nullptr) CloseHandle(static_cast<HANDLE>(released_));
}

std::shared_ptr<logging::ILogger> RpcConnection::Logger() const {
  std::lock_guard<std::mutex> lock(logger_lock_);
  return logger_;
}

void RpcConnection::SetLogger(std::shared_ptr<logging::ILogger> logger) {
  {
    std::lock_guard<std::mutex> lock(logger_lock_);
    logger_ = logger;
  }
  if (named_pipe_ != nullptr) named_pipe_->SetLogger(std::move(logger));
}

RpcState RpcConnection::State() const {
  std::lock_guard<std::recursive_mutex> lock(l_states_);
  return state_;
}

std::shared_ptr<discord_rpc::Configuration> RpcConnection::Configuration() const {
  std::lock_guard<std::recursive_mutex> lock(l_config_);
  return configuration_;
}

long long RpcConnection::GetNextNonce() {
  nonce_ += 1;
  return nonce_;
}

void RpcConnection::EnqueueCommand(const std::shared_ptr<ICommand>& command) {
  Logger()->Trace("Enqueue Command: {0}", Args(command->TypeName()));

  if (aborting_ || shutdown_) return;

  std::lock_guard<std::recursive_mutex> lock(l_rtqueue_);
  if (rtqueue_.size() == max_rt_queue_size_) {
    Logger()->Error("Too many enqueued commands, dropping oldest one. Maybe you are pushing new presences to fast?");
    rtqueue_.pop_front();
  }

  rtqueue_.push_back(command);
}

void RpcConnection::EnqueueMessage(const std::shared_ptr<message::IMessage>& message) {
  try {
    if (OnRpcMessage) OnRpcMessage.Invoke(this, message);
  } catch (const Exception& e) {
    Logger()->Error("Unhandled Exception while processing event: {0}", Args(e.TypeName()));
    Logger()->Error(e.Message());
  } catch (const std::exception& e) {
    Logger()->Error("Unhandled Exception while processing event: {0}", Args("std::exception"));
    Logger()->Error(e.what());
  } catch (...) {
    Logger()->Error("Unhandled Exception while processing event: {0}", Args("unknown"));
  }

  if (max_rx_queue_size_ <= 0) {
    Logger()->Trace("Enqueued Message, but queue size is 0.");
    return;
  }

  if (message == nullptr) throw NullReferenceException();
  Logger()->Trace("Enqueue Message: {0}", Args(message->Type()));
  std::lock_guard<std::recursive_mutex> lock(l_rxqueue_);
  if (rxqueue_.size() == max_rx_queue_size_) {
    Logger()->Warning("Too many enqueued messages, dropping oldest one.");
    rxqueue_.pop_front();
  }

  rxqueue_.push_back(message);
}

std::shared_ptr<message::IMessage> RpcConnection::DequeueMessage() {
  std::lock_guard<std::recursive_mutex> lock(l_rxqueue_);
  if (rxqueue_.empty()) return nullptr;

  std::shared_ptr<message::IMessage> message = rxqueue_.front();
  rxqueue_.pop_front();
  return message;
}

std::vector<std::shared_ptr<message::IMessage>> RpcConnection::DequeueMessages() {
  std::lock_guard<std::recursive_mutex> lock(l_rxqueue_);
  std::vector<std::shared_ptr<message::IMessage>> messages(rxqueue_.begin(), rxqueue_.end());

  rxqueue_.clear();

  return messages;
}

void RpcConnection::MainLoop() {
  Logger()->Info("RPC Connection Started");
  if (Logger()->Level() <= logging::LogLevel::Trace) {
    Logger()->Trace("============================");
    Logger()->Trace("Assembly:             DiscordRPC, a C++ translation of 1.6.1.70");
    Logger()->Trace("Pipe:                 DiscordRPC.IO.ManagedNamedPipeClient");
    Logger()->Trace("Platform:             Microsoft Windows NT");
    Logger()->Trace("applicationID:        " + application_id_);
    Logger()->Trace("targetPipe:           " + std::to_string(target_pipe_));
    Logger()->Trace("POLL_RATE:            " + std::to_string(POLL_RATE));
    Logger()->Trace("_maxRtQueueSize:      " + std::to_string(max_rt_queue_size_));
    Logger()->Trace("_maxRxQueueSize:      " + std::to_string(max_rx_queue_size_));
    Logger()->Trace("============================");
  }

  while (!aborting_ && !shutdown_) {
    try {
      Connection();
    } catch (const Exception& e) {
      Logger()->Error("Unhandled Exception: {0}", Args(e.TypeName()));
      Logger()->Error(e.Message());
    } catch (const std::exception& e) {
      Logger()->Error("Unhandled Exception: {0}", Args("std::exception"));
      Logger()->Error(e.what());
    }

    if (named_pipe_->IsConnected()) {
      Logger()->Trace("Closing the named pipe.");
      named_pipe_->Close();
    }

    SetConnectionState(RpcState::Disconnected);
  }

  Logger()->Trace("Left Main Loop");
  if (named_pipe_ != nullptr) named_pipe_->Dispose();

  Logger()->Info("Thread Terminated, no longer performing RPC connection.");
}

void RpcConnection::Connection() {
  Logger()->Trace("Connecting to the pipe through the {0}", Args("DiscordRPC.IO.ManagedNamedPipeClient"));
  if (named_pipe_->Connect(target_pipe_)) {
    Logger()->Trace("Connected to the pipe. Attempting to establish handshake...");
    {
      auto established = std::make_shared<message::ConnectionEstablishedMessage>();
      established->ConnectedPipe = named_pipe_->ConnectedPipe();
      EnqueueMessage(established);
    }

    EstablishHandshake();
    Logger()->Trace("Connection Established. Starting reading loop...");

    io::PipeFrame frame;
    bool mainloop = true;
    while (mainloop && !aborting_ && !shutdown_ && named_pipe_->IsConnected()) {
      if (named_pipe_->ReadFrame(&frame)) {
        Logger()->Trace("Read Payload: {0}", Args(frame.Opcode));

        switch (frame.Opcode) {
          case io::Opcode::Close: {
            const std::shared_ptr<ClosePayload> close = ClosePayload::FromJson(frame.Message());
            if (close == nullptr) throw NullReferenceException();
            Logger()->Warning("We have been told to terminate by discord: ({0}) {1}", Args(close->Code, close->Reason));
            auto closed = std::make_shared<message::CloseMessage>();
            closed->Code = close->Code;
            closed->Reason = close->Reason;
            EnqueueMessage(closed);
            mainloop = false;
            break;
          }

          case io::Opcode::Ping:
            Logger()->Trace("PING");
            frame.Opcode = io::Opcode::Pong;
            named_pipe_->WriteFrame(frame);
            break;

          case io::Opcode::Pong:
            Logger()->Trace("PONG");
            break;

          case io::Opcode::Frame: {
            if (shutdown_) {
              Logger()->Warning("Skipping frame because we are shutting down.");
              break;
            }

            if (frame.Data == nullptr) {
              Logger()->Error("We received no data from the frame so we cannot get the event payload!");
              break;
            }

            std::shared_ptr<EventPayload> response;
            try {
              response = EventPayload::FromJson(frame.Message());
            } catch (const Exception& e) {
              Logger()->Error("Failed to parse event! {0}", Args(e.Message()));
              Logger()->Error("Data: {0}", Args(frame.Message()));
            }

            try {
              if (response != nullptr) ProcessFrame(response);
            } catch (const Exception& e) {
              Logger()->Error("Failed to process event! {0}", Args(e.Message()));
              Logger()->Error("Data: {0}", Args(frame.Message()));
            }

            break;
          }

          default:
          case io::Opcode::Handshake:
            Logger()->Error("Invalid opcode: {0}", Args(frame.Opcode));
            mainloop = false;
            break;
        }
      }

      if (!aborting_ && named_pipe_->IsConnected()) {
        ProcessCommandQueue();

        WaitForSingleObject(static_cast<HANDLE>(queue_updated_event_), POLL_RATE);
      }
    }

    Logger()->Trace("Left main read loop for some reason. Aborting: {0}, Shutting Down: {1}",
                    Args(aborting_.load(), shutdown_.load()));
  } else {
    Logger()->Error("Failed to connect for some reason.");
    auto failed = std::make_shared<message::ConnectionFailedMessage>();
    failed->FailedPipe = target_pipe_;
    EnqueueMessage(failed);
  }

  if (!aborting_ && !shutdown_) {
    const long long sleep = delay_.NextDelay();

    Logger()->Trace("Waiting {0}ms before attempting to connect again", Args(sleep));
    WaitForSingleObject(static_cast<HANDLE>(released_), static_cast<DWORD>(delay_.NextDelay()));
  }
}

void RpcConnection::ProcessFrame(const std::shared_ptr<EventPayload>& response) {
  Logger()->Info("Handling Response. Cmd: {0}, Event: {1}", Args(response->Command, response->Event));

  if (response->Event.has_value() && *response->Event == ServerEvent::Error) {
    Logger()->Error("Error received from the RPC");

    const std::shared_ptr<message::ErrorMessage> err =
        response->Data.has_value() ? message::ErrorMessage::FromToken(*response->Data) : nullptr;
    if (err == nullptr) throw NullReferenceException();
    Logger()->Error("Server responded with an error message: ({0}) {1}", Args(message::ToString(err->Code), err->Message));

    EnqueueMessage(err);
    return;
  }

  if (State() == RpcState::Connecting) {
    if (response->Command == Command::Dispatch && response->Event.has_value() &&
        *response->Event == ServerEvent::Ready) {
      Logger()->Info("Connection established with the RPC");
      SetConnectionState(RpcState::Connected);
      delay_.Reset();

      const std::shared_ptr<message::ReadyMessage> ready =
          response->Data.has_value() ? message::ReadyMessage::FromToken(*response->Data) : nullptr;
      {
        std::lock_guard<std::recursive_mutex> lock(l_config_);
        if (ready == nullptr) throw NullReferenceException();
        configuration_ = ready->Configuration;
        if (ready->User == nullptr) throw NullReferenceException();
        ready->User->SetConfiguration(configuration_);
      }

      EnqueueMessage(ready);
      return;
    }
  }

  if (State() == RpcState::Connected) {
    switch (response->Command) {
      case Command::Dispatch:
        ProcessDispatch(response);
        break;

      case Command::SetActivity:
        if (!response->Data.has_value()) {
          EnqueueMessage(std::make_shared<message::PresenceMessage>());
        } else {
          const std::shared_ptr<RichPresenceResponse> rp = RichPresenceResponse::FromToken(*response->Data);
          EnqueueMessage(std::make_shared<message::PresenceMessage>(rp));
        }
        break;

      case Command::Unsubscribe:
      case Command::Subscribe: {
        const std::shared_ptr<EventPayload> inner =
            response->Data.has_value() ? EventPayload::FromToken(*response->Data) : nullptr;
        if (inner == nullptr) throw NullReferenceException();
        if (!inner->Event.has_value()) throw InvalidOperationException("Nullable object must have a value.");
        const ServerEvent evt = *inner->Event;

        if (response->Command == Command::Subscribe) {
          EnqueueMessage(std::make_shared<message::SubscribeMessage>(evt));
        } else {
          EnqueueMessage(std::make_shared<message::UnsubscribeMessage>(evt));
        }
        break;
      }

      case Command::SendActivityJoinInvite:
        Logger()->Trace("Got invite response ack.");
        break;

      case Command::CloseActivityJoinRequest:
        Logger()->Trace("Got invite response reject ack.");
        break;

      default:
        Logger()->Error("Unknown frame was received! {0}", Args(response->Command));
        return;
    }
    return;
  }

  Logger()->Trace("Received a frame while we are disconnected. Ignoring. Cmd: {0}, Event: {1}",
                  Args(response->Command, response->Event));
}

void RpcConnection::ProcessDispatch(const std::shared_ptr<EventPayload>& response) {
  if (response->Command != Command::Dispatch) return;
  if (!response->Event.has_value()) return;

  switch (*response->Event) {
    case ServerEvent::ActivitySpectate: {
      const std::shared_ptr<message::SpectateMessage> spectate =
          response->Data.has_value() ? message::SpectateMessage::FromToken(*response->Data) : nullptr;
      EnqueueMessage(spectate);
      break;
    }

    case ServerEvent::ActivityJoin: {
      const std::shared_ptr<message::JoinMessage> join =
          response->Data.has_value() ? message::JoinMessage::FromToken(*response->Data) : nullptr;
      EnqueueMessage(join);
      break;
    }

    case ServerEvent::ActivityJoinRequest: {
      const std::shared_ptr<message::JoinRequestMessage> request =
          response->Data.has_value() ? message::JoinRequestMessage::FromToken(*response->Data) : nullptr;
      EnqueueMessage(request);
      break;
    }

    default:
      Logger()->Warning("Ignoring {0}", Args(*response->Event));
      break;
  }
}

void RpcConnection::ProcessCommandQueue() {
  if (State() != RpcState::Connected) return;

  if (aborting_) Logger()->Warning("We have been told to write a queue but we have also been aborted.");

  bool needsWriting = true;
  std::shared_ptr<ICommand> item;

  const auto dequeue = [this] {
    std::lock_guard<std::recursive_mutex> lock(l_rtqueue_);
    if (!rtqueue_.empty()) rtqueue_.pop_front();
  };

  while (needsWriting && named_pipe_->IsConnected()) {
    {
      std::lock_guard<std::recursive_mutex> lock(l_rtqueue_);
      needsWriting = !rtqueue_.empty();
      if (!needsWriting) break;

      item = rtqueue_.front();
    }

    if (shutdown_ || (!aborting_ && LOCK_STEP)) needsWriting = false;

    const std::shared_ptr<IPayload> payload = item->PreparePayload(GetNextNonce());
    Logger()->Trace("Attempting to send payload: {0}", Args(payload->Command));

    io::PipeFrame frame;
    if (dynamic_cast<CloseCommand*>(item.get()) != nullptr) {
      SendHandwave();

      Logger()->Trace("Handwave sent, ending queue processing.");
      dequeue();

      return;
    } else {
      if (aborting_) {
        Logger()->Warning("- skipping frame because of abort.");
        dequeue();
      } else {
        frame.SetObject(io::Opcode::Frame, payload->ToJson());

        Logger()->Trace("Sending payload: {0}", Args(payload->Command));
        if (named_pipe_->WriteFrame(frame)) {
          Logger()->Trace("Sent Successfully.");
          dequeue();
        } else {
          Logger()->Warning("Something went wrong during writing!");
          return;
        }
      }
    }
  }
}

void RpcConnection::EstablishHandshake() {
  Logger()->Trace("Attempting to establish a handshake...");

  if (State() != RpcState::Disconnected) {
    Logger()->Error("State must be disconnected in order to start a handshake!");
    return;
  }

  Logger()->Trace("Sending Handshake...");
  io::Handshake handshake;
  handshake.Version = VERSION;
  handshake.ClientID = application_id_;
  if (!named_pipe_->WriteFrame(io::PipeFrame(io::Opcode::Handshake, handshake.ToJson()))) {
    Logger()->Error("Failed to write a handshake.");
    return;
  }

  SetConnectionState(RpcState::Connecting);
}

void RpcConnection::SendHandwave() {
  Logger()->Info("Attempting to wave goodbye...");

  if (State() == RpcState::Disconnected) {
    Logger()->Error("State must NOT be disconnected in order to send a handwave!");
    return;
  }

  io::Handshake handshake;
  handshake.Version = VERSION;
  handshake.ClientID = application_id_;
  if (!named_pipe_->WriteFrame(io::PipeFrame(io::Opcode::Close, handshake.ToJson()))) {
    Logger()->Error("failed to write a handwave.");
    return;
  }
}

bool RpcConnection::AttemptConnection() {
  Logger()->Info("Attempting a new connection");

  if (thread_started_) {
    Logger()->Error("Cannot attempt a new connection as the previous connection thread is not null!");
    return false;
  }

  if (State() != RpcState::Disconnected) {
    Logger()->Warning("Cannot attempt a new connection as the previous connection hasn't changed state yet.");
    return false;
  }

  if (aborting_) {
    Logger()->Error("Cannot attempt a new connection while aborting!");
    return false;
  }

  thread_started_ = true;
  thread_ = std::thread([this] {
    SetThreadDescription(GetCurrentThread(), L"Discord IPC Thread");
    try {
      MainLoop();
    } catch (...) {
    }
  });
  return true;
}

void RpcConnection::SetConnectionState(RpcState state) {
  Logger()->Trace("Setting the connection state to {0}",
                  Args(helper::ToUpperInvariant(
                      helper::StringTools::ToSnakeCase(rpc::ToString(state)).value_or(std::string()))));
  std::lock_guard<std::recursive_mutex> lock(l_states_);
  state_ = state;
}

void RpcConnection::Shutdown() {
  Logger()->Trace("Initiated shutdown procedure");
  shutdown_ = true;

  {
    std::lock_guard<std::recursive_mutex> lock(l_rtqueue_);
    rtqueue_.clear();
    if (CLEAR_ON_SHUTDOWN) {
      auto clear = std::make_shared<PresenceCommand>();
      clear->PID = process_id_;
      clear->Presence = nullptr;
      rtqueue_.push_back(clear);
    }
    rtqueue_.push_back(std::make_shared<CloseCommand>());
  }

  SetEvent(static_cast<HANDLE>(queue_updated_event_));
}

void RpcConnection::Close() {
  if (!thread_started_) {
    Logger()->Error("Cannot close as it is not available!");
    return;
  }

  if (aborting_) {
    Logger()->Error("Cannot abort as it has already been aborted");
    return;
  }

  if (ShutdownOnly()) {
    Shutdown();
    return;
  }

  Logger()->Trace("Updating Abort State...");
  aborting_ = true;
  SetEvent(static_cast<HANDLE>(queue_updated_event_));
}

void RpcConnection::Dispose() {
  SetShutdownOnly(false);
  Close();
}

}
