// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/discord_presence.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

#include <nlohmann/json.hpp>

#include "discord_rpc/discord_rpc_client.h"
#include "discord_rpc/exceptions.h"
#include "elixir_runtime/host_log.h"
#include "runtime/roblox_experience_launch_request.h"

namespace elixir::runtime {
namespace {

namespace mr = mocktail::runtime;
using nlohmann::json;

constexpr std::size_t kTextLimit = 128;
constexpr std::size_t kMaxQueuedLines = 16;
constexpr std::size_t kActivityBudget = 5;
constexpr ULONGLONG kActivityWindowMs = 20000;
constexpr std::size_t kImageLimit = 256;
constexpr std::size_t kButtonLabelBytes = 31;
constexpr std::size_t kButtonUrlLimit = 512;
constexpr std::size_t kMaxCachedAssets = 20;
constexpr std::int64_t kLastUnixSecond = 253402300799;

std::int64_t UnixNow() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::size_t Characters(std::string_view text) {
  std::size_t count = 0;
  for (const char c : text) count += (static_cast<unsigned char>(c) & 0xC0) != 0x80;
  return count;
}

std::size_t Utf16Units(std::string_view text) {
  std::size_t units = 0;
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if ((byte & 0xC0) == 0x80) continue;
    units += byte >= 0xF0 ? 2 : 1;
  }
  return units;
}

std::string CutToUtf16Units(std::string text, std::size_t limit) {
  std::size_t units = 0, index = 0;
  while (index < text.size()) {
    const auto byte = static_cast<unsigned char>(text[index]);
    const std::size_t length = byte >= 0xF0 ? 4 : byte >= 0xE0 ? 3 : byte >= 0xC0 ? 2 : 1;
    const std::size_t step = byte >= 0xF0 ? 2 : 1;
    if (units + step > limit) break;
    units += step;
    index += length;
  }
  text.resize(std::min(index, text.size()));
  return text;
}

std::string Trimmed(std::string text) {
  const auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!text.empty() && blank(text.back())) text.pop_back();
  std::size_t start = 0;
  while (start < text.size() && blank(text[start])) ++start;
  return text.substr(start);
}

std::string Text(std::string text) {
  text.erase(std::remove_if(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20; }),
             text.end());
  text = CutToUtf16Units(Trimmed(std::move(text)), kTextLimit);
  text = Trimmed(std::move(text));
  if (Characters(text) == 1) text += "\xE2\xA0\x80";
  return text;
}

bool IsRobloxImageUrl(std::string_view url) {
  constexpr std::string_view kScheme = "https://";
  constexpr std::string_view kCdn = ".rbxcdn.com";
  if (url.size() > 512 || url.substr(0, kScheme.size()) != kScheme) return false;
  for (const char c : url) {
    if (static_cast<unsigned char>(c) <= 0x20 || c == 0x7F) return false;
  }
  const std::string_view rest = url.substr(kScheme.size());
  const std::string_view host = rest.substr(0, rest.find('/'));
  for (const char c : host) {
    if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '.' && c != '-') return false;
  }
  return host.size() > kCdn.size() && host.substr(host.size() - kCdn.size()) == kCdn;
}

const json* Member(const json* object, const char* key) {
  if (object == nullptr || !object->is_object()) return nullptr;
  const auto found = object->find(key);
  return found == object->end() ? nullptr : &*found;
}

std::string StringAt(const json* object, const char* key) {
  const json* value = Member(object, key);
  return value != nullptr && value->is_string() ? value->get<std::string>() : std::string();
}

std::uint64_t UnsignedAt(const json* object, const char* key) {
  const json* value = Member(object, key);
  return value != nullptr && value->is_number_unsigned() ? value->get<std::uint64_t>() : 0;
}

const json* FirstData(const std::optional<json>& body) {
  const json* data = body ? Member(&*body, "data") : nullptr;
  return data != nullptr && data->is_array() && !data->empty() ? &(*data)[0] : nullptr;
}

bool ParseImage(const json& value, BloxstrapRpcImage* image) {
  if (value.is_null()) return true;
  if (!value.is_object()) return false;
  image->present = true;
  for (const auto& item : value.items()) {
    const std::string& key = item.key();
    const json& member = item.value();
    if (member.is_null()) continue;
    if (key == "assetId") {
      if (!member.is_number_unsigned()) return false;
      image->asset_id = member.get<std::uint64_t>();
    } else if (key == "hoverText") {
      if (!member.is_string()) return false;
      if (Characters(member.get_ref<const std::string&>()) <= kTextLimit) image->hover_text = member.get<std::string>();
    } else if (key == "clear" || key == "reset") {
      if (!member.is_boolean()) return false;
      (key == "clear" ? image->clear : image->reset) = member.get<bool>();
    }
  }
  return true;
}

bool ParseJoiningGame(std::string_view line, std::string* job_id, std::int64_t* place_id) {
  constexpr std::string_view kMarker = "! Joining game '";
  constexpr std::string_view kPlace = "' place ";
  const std::size_t at = line.find(kMarker);
  if (at == std::string_view::npos) return false;
  std::string_view rest = line.substr(at + kMarker.size());
  if (rest.size() < 36 + kPlace.size() || rest.substr(36, kPlace.size()) != kPlace) return false;
  const std::string_view job = rest.substr(0, 36);
  rest.remove_prefix(36 + kPlace.size());
  std::int64_t place = 0;
  std::size_t digits = 0;
  while (digits < rest.size() && digits < 18 && rest[digits] >= '0' && rest[digits] <= '9') {
    place = place * 10 + (rest[digits] - '0');
    ++digits;
  }
  if (digits == 0 || place <= 0) return false;
  *job_id = std::string(job);
  *place_id = place;
  return true;
}

std::string DiscordError(int code, const discord_rpc::String& text) {
  std::string message = text.value_or(std::string());
  message.erase(std::remove_if(message.begin(), message.end(),
                               [](char c) { return static_cast<unsigned char>(c) < 0x20; }),
                message.end());
  if (message.size() > 120) message.resize(120);
  return std::to_string(code) + (message.empty() ? std::string() : ": " + message);
}

}

bool DiscordActivity::operator==(const DiscordActivity& other) const {
  return details == other.details && state == other.state && start == other.start && end == other.end &&
         large_image == other.large_image && large_text == other.large_text && small_image == other.small_image &&
         small_text == other.small_text && buttons == other.buttons;
}

bool ParseBloxstrapRpc(std::string_view text, BloxstrapRpcUpdate* update) {
  if (update == nullptr) return false;
  *update = BloxstrapRpcUpdate{};
  const json message = json::parse(text.begin(), text.end(), nullptr, false);
  if (message.is_discarded() || !message.is_object()) return false;
  const json* command = Member(&message, "command");
  if (command == nullptr || !command->is_string() || command->get_ref<const std::string&>().empty()) return false;
  const std::string& name = command->get_ref<const std::string&>();
  const json* data = Member(&message, "data");
  if (name == "SetLaunchData") {
    if (data == nullptr || !data->is_string() || Characters(data->get_ref<const std::string&>()) > 200) return false;
    update->set_launch_data = true;
    update->launch_data = data->get<std::string>();
    return true;
  }
  if (name != "SetRichPresence" || data == nullptr || !data->is_object()) return false;
  for (const auto& item : data->items()) {
    const std::string& key = item.key();
    const json& member = item.value();
    if (key == "details" || key == "state") {
      if (member.is_null()) continue;
      if (!member.is_string()) return false;
      if (Characters(member.get_ref<const std::string&>()) > kTextLimit) continue;
      (key == "details" ? update->details : update->state) = member.get<std::string>();
    } else if (key == "timeStart" || key == "timeEnd") {
      if (member.is_null()) continue;
      if (!member.is_number_unsigned()) return false;
      (key == "timeStart" ? update->time_start : update->time_end) = member.get<std::uint64_t>();
    } else if (key == "smallImage" || key == "largeImage") {
      if (!ParseImage(member, key == "smallImage" ? &update->small_image : &update->large_image)) return false;
    }
  }
  return true;
}

std::string_view BloxstrapRpcPayload(std::string_view line) {
  constexpr std::string_view kMarker = "[BloxstrapRPC] ";
  if (line.find("[FLog::CreatorOutput]") == std::string_view::npos) return {};
  const std::size_t at = line.find(kMarker);
  if (at == std::string_view::npos) return {};
  std::string_view payload = line.substr(at + kMarker.size());
  while (!payload.empty() && (payload.back() == '\r' || payload.back() == '\n' || payload.back() == ' ')) {
    payload.remove_suffix(1);
  }
  return payload;
}

std::shared_ptr<discord_rpc::RichPresence> DiscordRichPresence(const DiscordActivity& activity) {
  using discord_rpc::String;
  const auto set = [](const auto& setter, const std::string& value) {
    try {
      setter(String(value));
    } catch (const discord_rpc::Exception&) {
      setter(String());
    }
  };
  auto presence = std::make_shared<discord_rpc::RichPresence>();
  if (!activity.details.empty()) set([&](const String& text) { presence->SetDetails(text); }, Text(activity.details));
  if (!activity.state.empty()) set([&](const String& text) { presence->SetState(text); }, Text(activity.state));

  const auto milliseconds = [](std::int64_t seconds) {
    return static_cast<std::uint64_t>(std::min(seconds, kLastUnixSecond)) * 1000u;
  };
  auto timestamps = std::make_shared<discord_rpc::Timestamps>();
  try {
    if (activity.start > 0) timestamps->SetStartUnixMilliseconds(milliseconds(activity.start));
    if (activity.end > 0) timestamps->SetEndUnixMilliseconds(milliseconds(activity.end));
  } catch (const discord_rpc::Exception&) {
  }
  if (timestamps->Start.has_value() || timestamps->End.has_value()) presence->Timestamps = timestamps;

  auto assets = std::make_shared<discord_rpc::Assets>();
  const auto image = [](const std::string& key) { return !key.empty() && Utf16Units(key) <= kImageLimit; };
  if (image(activity.large_image)) {
    set([&](const String& text) { assets->SetLargeImageKey(text); }, activity.large_image);
  }
  if (!activity.large_text.empty()) {
    set([&](const String& text) { assets->SetLargeImageText(text); }, Text(activity.large_text));
  }
  if (image(activity.small_image)) {
    set([&](const String& text) { assets->SetSmallImageKey(text); }, activity.small_image);
  }
  if (!activity.small_text.empty()) {
    set([&](const String& text) { assets->SetSmallImageText(text); }, Text(activity.small_text));
  }
  if (assets->LargeImageKey().has_value() || assets->LargeImageText().has_value() ||
      assets->SmallImageKey().has_value() || assets->SmallImageText().has_value()) {
    presence->Assets = assets;
  }

  auto buttons = std::make_shared<discord_rpc::RichPresence::ButtonArray>();
  for (const auto& [raw_label, raw_url] : activity.buttons) {
    const std::string label = Trimmed(raw_label), url = Trimmed(raw_url);
    if (buttons->size() == 2 || label.empty() || label.size() > kButtonLabelBytes || url.empty() ||
        Utf16Units(url) > kButtonUrlLimit) {
      continue;
    }
    auto button = std::make_shared<discord_rpc::Button>();
    try {
      button->SetLabel(label);
      button->SetUrl(url);
    } catch (const discord_rpc::Exception&) {
      continue;
    }
    if (!button->Label().has_value() || !button->Url().has_value()) continue;
    buttons->push_back(button);
  }
  if (!buttons->empty()) presence->Buttons = buttons;
  return presence;
}

std::string RobloxJoinUrl(std::int64_t place_id, std::string_view job_id) {
  if (place_id <= 0 || job_id.size() != 36) return {};
  for (const char c : job_id) {
    const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!hex && c != '-') return {};
  }
  return "https://www.roblox.com/games/start?placeId=" + std::to_string(place_id) +
         "&gameInstanceId=" + std::string(job_id);
}

class DiscordPresence::Impl {
 public:
  explicit Impl(DiscordPresenceOptions options) : options_(std::move(options)) {
    stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  }

  ~Impl() {
    Stop();
    client_.reset();
    if (stop_ != nullptr) CloseHandle(stop_);
    if (wake_ != nullptr) CloseHandle(wake_);
  }

  void Start() {
    if (worker_.joinable() || stop_ == nullptr || wake_ == nullptr) return;
    ResetEvent(stop_);
    worker_ = std::thread([this] { Run(); });
    SetGuestLineObserver(&Impl::GuestLine, this);
  }

  void Stop() {
    if (!worker_.joinable()) return;
    SetGuestLineObserver(nullptr, nullptr);
    SetEvent(stop_);
    worker_.join();
  }

  static void Notify(void* context, mr::RobloxExperiencePresencePhase phase,
                     const mr::RobloxExperienceLaunchRequest* request) {
    auto& self = *static_cast<Impl*>(context);
    try {
      std::lock_guard<std::mutex> lock(self.mutex_);
      Shared& shared = self.shared_;
      shared.phase = phase;
      if (request != nullptr) {
        shared.place_id = request->place_id;
        shared.request_job_id = request->game_instance_id;
        shared.server = !request->reserved_server_access_code.empty()                     ? Server::kReserved
                        : !request->access_code.empty() || !request->link_code.empty() ? Server::kPrivate
                                                                                        : Server::kPublic;
      }
      ++shared.revision;
    } catch (...) {
      return;
    }
    SetEvent(self.wake_);
  }

  static void GuestLine(void* context, const char* tag, std::string_view message) {
    static_cast<Impl*>(context)->OnGuestLine(tag != nullptr ? tag : "", message);
  }

  void OnGuestLine(std::string_view tag, std::string_view message) {
    if (tag != "Roblox") return;
    const bool rpc = options_.show_game && options_.game_presence &&
                     message.find("[BloxstrapRPC]") != std::string_view::npos;
    const bool joining = message.find("! Joining game '") != std::string_view::npos;
    if (!rpc && !joining) return;
    try {
      std::lock_guard<std::mutex> lock(mutex_);
      if (lines_.size() >= kMaxQueuedLines) return;
      lines_.emplace_back(message.substr(0, 4096));
    } catch (...) {
      return;
    }
    SetEvent(wake_);
  }

  bool connected() const { return connected_.load(); }
  unsigned activities_sent() const { return activities_sent_.load(); }

 private:
  enum class Server { kPublic, kPrivate, kReserved };

  struct Shared {
    mr::RobloxExperiencePresencePhase phase = mr::RobloxExperiencePresencePhase::kBrowsing;
    std::int64_t place_id = 0;
    std::string request_job_id;
    Server server = Server::kPublic;
    unsigned revision = 0;
  };

  struct Game {
    bool in_game = false;
    std::int64_t place_id = 0;
    std::string job_id;
    Server server = Server::kPublic;
    std::int64_t joined_at = 0;
  };

  struct GameInfo {
    bool done = false;
    std::string name;
    std::string creator;
    std::string icon;
    unsigned failures = 0;
    ULONGLONG retry_at = 0;
  };

  struct ImageOverride {
    bool set = false;
    bool clear = false;
    std::uint64_t asset_id = 0;
    std::optional<std::string> text;
  };

  struct Overrides {
    std::optional<std::string> details;
    std::optional<std::string> state;
    std::optional<std::int64_t> start;
    std::optional<std::int64_t> end;
    ImageOverride small;
    ImageOverride large;
    ULONGLONG last_ms = 0;
  };

  void Run() {
    unsigned seen_revision = 0;
    DWORD wait = INFINITE;
    for (;;) {
      const HANDLE handles[2] = {stop_, wake_};
      if (WaitForMultipleObjects(2, handles, FALSE, wait) == WAIT_OBJECT_0) break;
      Shared shared;
      std::deque<std::string> lines;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        shared = shared_;
        lines.swap(lines_);
      }
      for (const std::string& message : lines) OnLog(message);
      if (shared.revision != seen_revision) {
        seen_revision = shared.revision;
        OnPhase(shared);
      }
      if (client_ == nullptr && game_.in_game && !start_failed_) StartClient();
      if (client_ != nullptr) {
        if (refused_.exchange(false)) overrides_ = Overrides{};
        if (connected_.load()) {
          FetchWhatIsMissing();
          if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) break;
          Publish(Desired());
        }
      }
      wait = client_ != nullptr && (game_.in_game || publish_pending_) ? 1000 : INFINITE;
    }
    if (client_ != nullptr) {
      try {
        client_->ClearPresence();
      } catch (const discord_rpc::Exception&) {
      }
      client_->Dispose();
      client_.reset();
      connected_ = false;
    }
  }

  void OnPhase(const Shared& shared) {
    switch (shared.phase) {
      case mr::RobloxExperiencePresencePhase::kBrowsing:
      case mr::RobloxExperiencePresencePhase::kJoining:
        if (game_.in_game) Log("discord: the experience ended");
        game_ = Game{};
        overrides_ = Overrides{};
        break;
      case mr::RobloxExperiencePresencePhase::kPlaying: {
        const bool same = game_.in_game && game_.place_id == shared.place_id;
        game_.in_game = true;
        game_.place_id = shared.place_id;
        game_.server = shared.server;
        game_.job_id = !shared.request_job_id.empty()           ? shared.request_job_id
                       : joined_line_place_ == shared.place_id ? joined_line_job_
                                                               : std::string();
        if (!same) {
          game_.joined_at = UnixNow();
          overrides_ = Overrides{};
          Log("discord: playing place " + std::to_string(shared.place_id));
        }
        break;
      }
    }
  }

  void OnLog(std::string_view message) {
    while (!message.empty()) {
      const std::size_t end = message.find('\n');
      const std::string_view line = message.substr(0, end);
      message = end == std::string_view::npos ? std::string_view() : message.substr(end + 1);
      std::string job;
      std::int64_t place = 0;
      if (ParseJoiningGame(line, &job, &place)) {
        joined_line_job_ = job;
        joined_line_place_ = place;
        if (game_.in_game && game_.place_id == place && game_.job_id.empty()) game_.job_id = job;
        continue;
      }
      const std::string_view payload = BloxstrapRpcPayload(line);
      if (payload.empty() || !game_.in_game || !options_.show_game || !options_.game_presence) continue;
      const ULONGLONG now = GetTickCount64();
      if (overrides_.last_ms != 0 && now - overrides_.last_ms < 1000) continue;
      BloxstrapRpcUpdate update;
      if (!ParseBloxstrapRpc(payload, &update)) continue;
      overrides_.last_ms = now;
      Apply(update);
    }
  }

  void Apply(const BloxstrapRpcUpdate& update) {
    const auto text = [](std::optional<std::string>* target, const std::optional<std::string>& value) {
      if (value) *target = *value == "<reset>" ? std::nullopt : value;
    };
    text(&overrides_.details, update.details);
    text(&overrides_.state, update.state);
    if (update.time_start) overrides_.start = static_cast<std::int64_t>(std::min<std::uint64_t>(*update.time_start, 1ull << 40));
    if (update.time_end) overrides_.end = static_cast<std::int64_t>(std::min<std::uint64_t>(*update.time_end, 1ull << 40));
    const auto image = [](ImageOverride* target, const BloxstrapRpcImage& value) {
      if (!value.present) return;
      if (value.reset) {
        *target = ImageOverride{};
        return;
      }
      if (value.clear) {
        target->set = true;
        target->clear = true;
      } else if (value.asset_id != 0) {
        target->set = true;
        target->clear = false;
        target->asset_id = value.asset_id;
      }
      if (value.hover_text) target->text = *value.hover_text;
    };
    image(&overrides_.small, update.small_image);
    image(&overrides_.large, update.large_image);
  }

  std::optional<DiscordActivity> Desired() const {
    if (!game_.in_game) return std::nullopt;
    DiscordActivity activity;
    activity.start = game_.joined_at;
    if (!options_.show_game) return activity;
    const auto found = games_.find(game_.place_id);
    const GameInfo* info = found != games_.end() && found->second.done ? &found->second : nullptr;
    const std::string name = info != nullptr ? info->name : std::string("a Roblox experience");
    activity.details = "Playing " + name;
    activity.state = game_.server == Server::kPrivate    ? "In a private server"
                     : game_.server == Server::kReserved ? "In a reserved server"
                     : info != nullptr && !info->creator.empty() ? "by " + info->creator
                                                                 : std::string();
    if (info != nullptr && !info->icon.empty()) {
      activity.large_image = info->icon;
      activity.large_text = name;
    }
    if (options_.join_button && game_.server == Server::kPublic) {
      const std::string join = RobloxJoinUrl(game_.place_id, game_.job_id);
      if (!join.empty()) activity.buttons.emplace_back("Join server", join);
    }
    activity.buttons.emplace_back("See game page", "https://www.roblox.com/games/" + std::to_string(game_.place_id));
    if (overrides_.details) activity.details = *overrides_.details;
    if (overrides_.state) activity.state = *overrides_.state;
    if (overrides_.start) activity.start = *overrides_.start;
    if (overrides_.end) activity.end = *overrides_.end;
    const auto image = [this](const ImageOverride& override, std::string* url, std::string* text) {
      if (override.set && override.clear) {
        url->clear();
        text->clear();
      } else if (override.set) {
        const auto asset = assets_.find(override.asset_id);
        if (asset != assets_.end() && !asset->second.empty()) *url = asset->second;
      }
      if (override.text && !url->empty()) *text = *override.text;
    };
    image(overrides_.large, &activity.large_image, &activity.large_text);
    image(overrides_.small, &activity.small_image, &activity.small_text);
    return activity;
  }

  std::optional<json> GetJson(const std::string& url) {
    if (!options_.http || WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return std::nullopt;
    mocktail::services::HttpRequest request;
    request.url = url;
    request.timeout_ms = 4000;
    request.maximum_body_bytes = 256 * 1024;
    request.follow_redirects = false;
    const mocktail::services::HttpResponse response = options_.http->Get(request);
    if (!response.transport_ok || response.status_code != 200) return std::nullopt;
    json body = json::parse(response.body, nullptr, false);
    if (body.is_discarded()) return std::nullopt;
    return body;
  }

  bool FetchGame(std::int64_t place_id, GameInfo* info) {
    const std::optional<json> universe =
        GetJson("https://apis.roblox.com/universes/v1/places/" + std::to_string(place_id) + "/universe");
    const std::uint64_t universe_id = universe ? UnsignedAt(&*universe, "universeId") : 0;
    if (universe_id == 0) return false;
    const std::optional<json> games =
        GetJson("https://games.roblox.com/v1/games?universeIds=" + std::to_string(universe_id));
    const json* game = FirstData(games);
    if (game == nullptr) return false;
    info->name = StringAt(game, "name");
    info->creator = StringAt(Member(game, "creator"), "name");
    const std::optional<json> icons =
        GetJson("https://thumbnails.roblox.com/v1/games/icons?universeIds=" + std::to_string(universe_id) +
                "&returnPolicy=PlaceHolder&size=512x512&format=Png&isCircular=false");
    const std::string icon_url = StringAt(FirstData(icons), "imageUrl");
    if (IsRobloxImageUrl(icon_url)) info->icon = icon_url;
    return !info->name.empty();
  }

  std::string FetchAssetThumbnail(std::uint64_t asset_id) {
    for (int attempt = 1; attempt <= 3; ++attempt) {
      const std::optional<json> answer =
          GetJson("https://thumbnails.roblox.com/v1/assets?assetIds=" + std::to_string(asset_id) +
                  "&returnPolicy=PlaceHolder&size=512x512&format=Png&isCircular=false");
      const json* thumbnail = FirstData(answer);
      if (thumbnail == nullptr) return {};
      if (StringAt(thumbnail, "state") != "Pending") {
        const std::string url = StringAt(thumbnail, "imageUrl");
        return IsRobloxImageUrl(url) ? url : std::string();
      }
      if (WaitForSingleObject(stop_, 500u * attempt) == WAIT_OBJECT_0) return {};
    }
    return {};
  }

  void FetchWhatIsMissing() {
    if (!options_.http || !game_.in_game || !options_.show_game) return;
    GameInfo& info = games_[game_.place_id];
    const ULONGLONG now = GetTickCount64();
    if (!info.done && now >= info.retry_at) {
      if (FetchGame(game_.place_id, &info)) {
        info.done = true;
      } else {
        info.failures = std::min(info.failures + 1, 6u);
        info.retry_at = now + std::min<ULONGLONG>(300000, 10000ull << (info.failures - 1));
      }
    }
    for (const ImageOverride* image : {&overrides_.small, &overrides_.large}) {
      if (!image->set || image->clear || image->asset_id == 0 || assets_.count(image->asset_id) != 0) continue;
      const std::string url = FetchAssetThumbnail(image->asset_id);
      if (assets_.size() >= kMaxCachedAssets) assets_.erase(assets_.begin());
      assets_[image->asset_id] = url;
    }
  }

  void StartClient() {
    namespace dr = discord_rpc;
    try {
      std::shared_ptr<dr::io::INamedPipeClient> pipe;
      if (!options_.pipe_prefix.empty()) pipe = std::make_shared<dr::io::ManagedNamedPipeClient>(options_.pipe_prefix);
      client_ = std::make_unique<dr::DiscordRpcClient>(options_.application_id, -1, nullptr, true, pipe);
      client_->OnReady += [this](dr::DiscordRpcClient*, const std::shared_ptr<dr::message::ReadyMessage>&) {
        waiting_logged_ = false;
        refusal_logged_ = false;
        ++ready_generation_;
        connected_ = true;
        Log("discord: connected to Discord");
        SetEvent(wake_);
      };
      client_->OnConnectionEstablished +=
          [this](dr::DiscordRpcClient*, const std::shared_ptr<dr::message::ConnectionEstablishedMessage>&) { Lost(); };
      client_->OnConnectionFailed +=
          [this](dr::DiscordRpcClient*, const std::shared_ptr<dr::message::ConnectionFailedMessage>&) {
            Lost();
            if (!waiting_logged_.exchange(true)) Log("discord: Discord isn't running; Elixir keeps looking for it");
          };
      client_->OnClose += [this](dr::DiscordRpcClient*, const std::shared_ptr<dr::message::CloseMessage>& message) {
        Lost();
        if (message != nullptr && !refusal_logged_.exchange(true)) {
          Log("discord: Discord refused the connection (" + DiscordError(message->Code, message->Reason) + ")");
        }
      };
      client_->OnError += [this](dr::DiscordRpcClient*, const std::shared_ptr<dr::message::ErrorMessage>& message) {
        if (message != nullptr) {
          Log("discord: Discord did not take the activity (" +
              DiscordError(static_cast<int>(message->Code), message->Message) + ")");
        }
        refused_ = true;
        SetEvent(wake_);
      };
      if (client_->Initialize()) return;
      Log("discord: Rich Presence could not start");
    } catch (const dr::Exception& e) {
      Log(std::string("discord: Rich Presence could not start (") + e.TypeName() + ")");
    }
    client_.reset();
    start_failed_ = true;
  }

  void Lost() {
    if (connected_.exchange(false)) Log("discord: disconnected from Discord");
  }

  void Publish(const std::optional<DiscordActivity>& desired) {
    publish_pending_ = false;
    const unsigned generation = ready_generation_.load();
    if (generation != published_generation_) {
      published_generation_ = generation;
      sent_any_ = false;
    }
    if (sent_any_ && sent_activity_ == desired) return;
    if (!sent_any_ && !desired && !handed_over_) {
      sent_any_ = true;
      sent_activity_.reset();
      return;
    }
    const ULONGLONG now = GetTickCount64();
    while (!sends_.empty() && now - sends_.front() >= kActivityWindowMs) sends_.pop_front();
    if (sends_.size() >= kActivityBudget) {
      publish_pending_ = true;
      return;
    }
    try {
      client_->SetSkipIdenticalPresence(false);
      client_->SetPresence(desired ? DiscordRichPresence(*desired) : nullptr);
      client_->SetSkipIdenticalPresence(true);
    } catch (const discord_rpc::Exception& e) {
      client_->SetSkipIdenticalPresence(true);
      Log(std::string("discord: the activity could not be set (") + e.TypeName() + ")");
    }
    sends_.push_back(now);
    sent_any_ = true;
    handed_over_ = true;
    sent_activity_ = desired;
    ++activities_sent_;
  }

  const DiscordPresenceOptions options_;
  HANDLE stop_ = nullptr;
  HANDLE wake_ = nullptr;
  std::thread worker_;

  std::mutex mutex_;
  Shared shared_;
  std::deque<std::string> lines_;

  Game game_;
  Overrides overrides_;
  std::string joined_line_job_;
  std::int64_t joined_line_place_ = 0;
  std::map<std::int64_t, GameInfo> games_;
  std::map<std::uint64_t, std::string> assets_;
  std::unique_ptr<discord_rpc::DiscordRpcClient> client_;
  bool start_failed_ = false;
  unsigned published_generation_ = 0;
  bool sent_any_ = false;
  bool handed_over_ = false;
  bool publish_pending_ = false;
  std::optional<DiscordActivity> sent_activity_;
  std::deque<ULONGLONG> sends_;
  std::atomic<unsigned> activities_sent_{0};
  std::atomic<bool> connected_{false};
  std::atomic<unsigned> ready_generation_{0};
  std::atomic<bool> refused_{false};
  std::atomic<bool> waiting_logged_{false};
  std::atomic<bool> refusal_logged_{false};
};

DiscordPresence::DiscordPresence(DiscordPresenceOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}

DiscordPresence::~DiscordPresence() = default;

void DiscordPresence::Start() { impl_->Start(); }

void DiscordPresence::Stop() { impl_->Stop(); }

mr::RobloxExperiencePresenceObserver DiscordPresence::observer() {
  return mr::RobloxExperiencePresenceObserver{impl_.get(), &Impl::Notify};
}

void DiscordPresence::OnGuestLine(std::string_view tag, std::string_view message) { impl_->OnGuestLine(tag, message); }

bool DiscordPresence::connected() const { return impl_->connected(); }

unsigned DiscordPresence::activities_sent() const { return impl_->activities_sent(); }

}
