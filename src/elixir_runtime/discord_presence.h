// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_DISCORD_PRESENCE_H_
#define ELIXIR_RUNTIME_DISCORD_PRESENCE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "discord_rpc/entities.h"
#include "runtime/roblox_experience_presence.h"
#include "services/http_client.h"

namespace elixir::runtime {

struct DiscordPresenceOptions {
  std::string application_id;
  bool show_game = true;
  bool join_button = false;
  bool game_presence = true;
  std::wstring pipe_prefix;
  std::shared_ptr<mocktail::services::HttpClient> http;
};

struct DiscordActivity {
  std::string details;
  std::string state;
  std::int64_t start = 0;
  std::int64_t end = 0;
  std::string large_image;
  std::string large_text;
  std::string small_image;
  std::string small_text;
  std::vector<std::pair<std::string, std::string>> buttons;

  bool operator==(const DiscordActivity& other) const;
};

struct BloxstrapRpcImage {
  bool present = false;
  bool clear = false;
  bool reset = false;
  std::uint64_t asset_id = 0;
  std::optional<std::string> hover_text;
};
struct BloxstrapRpcUpdate {
  bool set_launch_data = false;
  std::string launch_data;
  std::optional<std::string> details;
  std::optional<std::string> state;
  std::optional<std::uint64_t> time_start;
  std::optional<std::uint64_t> time_end;
  BloxstrapRpcImage small_image;
  BloxstrapRpcImage large_image;
};

bool ParseBloxstrapRpc(std::string_view json, BloxstrapRpcUpdate* update);

std::string_view BloxstrapRpcPayload(std::string_view line);

std::shared_ptr<discord_rpc::RichPresence> DiscordRichPresence(const DiscordActivity& activity);

std::string RobloxJoinUrl(std::int64_t place_id, std::string_view job_id);

class DiscordPresence final {
 public:
  explicit DiscordPresence(DiscordPresenceOptions options);
  ~DiscordPresence();
  DiscordPresence(const DiscordPresence&) = delete;
  DiscordPresence& operator=(const DiscordPresence&) = delete;

  void Start();
  void Stop();

  mocktail::runtime::RobloxExperiencePresenceObserver observer();
  void OnGuestLine(std::string_view tag, std::string_view message);

  bool connected() const;
  unsigned activities_sent() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}

#endif
