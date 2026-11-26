// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_ENTITIES_H_
#define DISCORD_RPC_ENTITIES_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "discord_rpc/helper.h"
#include "discord_rpc/json.h"

namespace discord_rpc {

struct DateTime {
  static constexpr std::int64_t kTicksPerMillisecond = 10000;
  static constexpr std::int64_t kUnixEpochTicks = 621355968000000000;
  static constexpr std::int64_t kMaxTicks = 3155378975999999999;

  std::int64_t Ticks = 0;

  static DateTime UtcNow();
  static DateTime Now();
  DateTime AddMilliseconds(double value) const;
  DateTime AddTicks(std::int64_t value) const;

  friend bool operator==(const DateTime& a, const DateTime& b) { return a.Ticks == b.Ticks; }
  friend bool operator!=(const DateTime& a, const DateTime& b) { return a.Ticks != b.Ticks; }
};

class Random {
 public:
  Random() : engine_(std::random_device{}()) {}
  explicit Random(int seed) : engine_(static_cast<std::mt19937::result_type>(seed)) {}

  int Next(int max_value) {
    return max_value <= 0 ? 0 : static_cast<int>(engine_() % static_cast<std::mt19937::result_type>(max_value));
  }
  void NextBytes(std::vector<std::uint8_t>* bytes) {
    for (std::uint8_t& byte : *bytes) byte = static_cast<std::uint8_t>(engine_() & 0xFF);
  }

 private:
  std::mt19937 engine_;
};

class Configuration {
 public:
  String ApiEndpoint;
  String CdnHost;
  String Environment;

  static std::shared_ptr<Configuration> FromToken(const json::Token& token);
};

enum class ActivityType {
  Playing = 0,
  Listening = 2,
  Watching = 3,
  Competing = 5,
};

enum class StatusDisplayType {
  Name = 0,
  State = 1,
  Details = 2,
};

class RichPresence;

class Assets {
 public:
  const String& LargeImageKey() const { return largeimagekey_; }
  void SetLargeImageKey(const String& value);
  const String& LargeImageText() const { return largeimagetext_; }
  void SetLargeImageText(const String& value);
  const String& LargeImageUrl() const { return largeimageurl_; }
  void SetLargeImageUrl(const String& value);
  const String& LargeImageID() const { return large_image_id_; }
  bool IsLargeImageKeyExternal() const { return is_large_image_key_external_; }

  const String& SmallImageKey() const { return smallimagekey_; }
  void SetSmallImageKey(const String& value);
  const String& SmallImageText() const { return smallimagetext_; }
  void SetSmallImageText(const String& value);
  const String& SmallImageUrl() const { return smallimageurl_; }
  void SetSmallImageUrl(const String& value);
  const String& SmallImageID() const { return small_image_id_; }
  bool IsSmallImageKeyExternal() const { return is_small_image_key_external_; }

  void Merge(const Assets& other);

  void WriteJson(json::Writer* writer) const;
  static std::shared_ptr<Assets> FromToken(const json::Token& token);

 private:
  String largeimagekey_;
  String largeimagetext_;
  String largeimageurl_;
  String large_image_id_;
  bool is_large_image_key_external_ = false;
  String smallimagekey_;
  String smallimagetext_;
  String smallimageurl_;
  String small_image_id_;
  bool is_small_image_key_external_ = false;
};

class Button {
 public:
  const String& Label() const { return label_; }
  void SetLabel(const String& value);
  const String& Url() const { return url_; }
  void SetUrl(const String& value);

 private:
  String label_;
  String url_;
};

class Party {
 public:
  enum class PrivacySetting {
    Private = 0,
    Public = 1,
  };

  const String& ID() const { return partyid_; }
  void SetID(const String& value);

  int Size = 0;
  int Max = 0;
  PrivacySetting Privacy = PrivacySetting::Private;

  void WriteJson(json::Writer* writer) const;
  static std::shared_ptr<Party> FromToken(const json::Token& token);

 private:
  String partyid_;
};

class Secrets {
 public:
  String MatchSecret;

  const String& JoinSecret() const { return join_secret_; }
  void SetJoinSecret(const String& value);
  const String& SpectateSecret() const { return spectate_secret_; }
  void SetSpectateSecret(const String& value);

  static int SecretLength() { return 128; }
  static std::string CreateSecret(Random* random);
  static std::string CreateFriendlySecret(Random* random);

  void WriteJson(json::Writer* writer) const;
  static std::shared_ptr<Secrets> FromToken(const json::Token& token);

 private:
  String join_secret_;
  String spectate_secret_;
};

class Timestamps {
 public:
  static std::shared_ptr<Timestamps> Now();
  static std::shared_ptr<Timestamps> FromTimeSpan(double seconds);

  std::optional<DateTime> Start;
  std::optional<DateTime> End;

  Timestamps() = default;
  explicit Timestamps(DateTime start) : Start(start) {}
  Timestamps(DateTime start, DateTime end) : Start(start), End(end) {}

  std::optional<std::uint64_t> StartUnixMilliseconds() const;
  void SetStartUnixMilliseconds(const std::optional<std::uint64_t>& value);
  std::optional<std::uint64_t> EndUnixMilliseconds() const;
  void SetEndUnixMilliseconds(const std::optional<std::uint64_t>& value);

  static DateTime FromUnixMilliseconds(std::uint64_t unix_time);
  static std::uint64_t ToUnixMilliseconds(DateTime date);

  void WriteJson(json::Writer* writer) const;
  static std::shared_ptr<Timestamps> FromToken(const json::Token& token);
};

class BaseRichPresence {
 public:
  virtual ~BaseRichPresence() = default;

  const String& State() const { return state_; }
  void SetState(const String& value);
  const String& StateUrl() const { return state_url_; }
  void SetStateUrl(const String& value);
  const String& Details() const { return details_; }
  void SetDetails(const String& value);
  const String& DetailsUrl() const { return details_url_; }
  void SetDetailsUrl(const String& value);

  std::shared_ptr<discord_rpc::Timestamps> Timestamps;
  std::shared_ptr<discord_rpc::Assets> Assets;
  std::shared_ptr<discord_rpc::Party> Party;
  std::shared_ptr<discord_rpc::Secrets> Secrets;
  ActivityType Type = ActivityType::Playing;
  StatusDisplayType StatusDisplay = StatusDisplayType::Name;

  bool HasTimestamps() const;
  bool HasAssets() const;
  bool HasParty() const;
  bool HasSecrets() const;

  static bool ValidateString(const String& str, String* result, bool use_bytes, int length);
  static bool ValidateUrl(const String& url);

  virtual bool Matches(const RichPresence* other) const;

  std::shared_ptr<RichPresence> ToRichPresence() const;

  std::string ToJson() const;
  virtual void WriteJson(json::Writer* writer) const;

 protected:
  void WriteBaseMembers(json::Writer* writer) const;

  String state_;
  String state_url_;
  String details_;
  String details_url_;
};

class RichPresence final : public BaseRichPresence {
 public:
  using ButtonArray = std::vector<std::shared_ptr<Button>>;

  std::shared_ptr<ButtonArray> Buttons;

  bool HasButtons() const { return Buttons != nullptr && !Buttons->empty(); }

  RichPresence& WithState(const String& state);
  RichPresence& WithStateUrl(const String& state_url);
  RichPresence& WithDetails(const String& details);
  RichPresence& WithDetailsUrl(const String& details_url);
  RichPresence& WithType(ActivityType type);
  RichPresence& WithStatusDisplay(StatusDisplayType status_display);
  RichPresence& WithTimestamps(std::shared_ptr<discord_rpc::Timestamps> timestamps);
  RichPresence& WithAssets(std::shared_ptr<discord_rpc::Assets> assets);
  RichPresence& WithParty(std::shared_ptr<discord_rpc::Party> party);
  RichPresence& WithSecrets(std::shared_ptr<discord_rpc::Secrets> secrets);
  RichPresence& WithButtons(std::shared_ptr<Button> top_button, std::shared_ptr<Button> bottom_button = nullptr);

  std::shared_ptr<RichPresence> Clone() const;

  RichPresence& Merge(const BaseRichPresence& presence);

  bool Matches(const RichPresence* other) const override;

  void WriteJson(json::Writer* writer) const override;
};

class RichPresenceResponse final : public BaseRichPresence {
 public:
  String ClientID;
  String Name;

  void WriteJson(json::Writer* writer) const override;
  static std::shared_ptr<RichPresenceResponse> FromToken(const json::Token& token);
};

class User {
 public:
  enum class AvatarFormat { PNG, JPEG, WebP, GIF };

  enum class AvatarSize {
    x16 = 16,
    x32 = 32,
    x64 = 64,
    x128 = 128,
    x256 = 256,
    x512 = 512,
    x1024 = 1024,
    x2048 = 2048,
  };

  struct AvatarDecorationData {
    String Asset;
    String SKU;
  };

  enum class Flag {
    None = 0,
    Employee = 1 << 0,
    Partner = 1 << 1,
    HypeSquad = 1 << 2,
    BugHunter = 1 << 3,
    HouseBravery = 1 << 6,
    HouseBrilliance = 1 << 7,
    HouseBalance = 1 << 8,
    EarlySupporter = 1 << 9,
    TeamUser = 1 << 10,
    BugHunterLevel2 = 1 << 14,
    VerifiedBot = 1 << 16,
    VerifiedDeveloper = 1 << 17,
    CertifiedModerator = 1 << 18,
    BotHttpInteractions = 1 << 19,
    ActiveDeveloper = 1 << 22,
  };

  enum class PremiumType {
    None = 0,
    NitroClassic = 1,
    Nitro = 2,
    NitroBasic = 3,
  };

  User() : cdn_endpoint_("cdn.discordapp.com") {}

  std::uint64_t ID() const { return id_; }
  const String& Username() const { return username_; }
  int Discriminator() const { return discriminator_; }
  const String& DisplayName() const { return display_name_; }
  const String& Avatar() const { return avatar_; }
  bool IsAvatarAnimated() const;
  const std::optional<AvatarDecorationData>& AvatarDecoration() const { return avatar_decoration_; }
  bool Bot() const { return bot_; }
  Flag Flags() const { return flags_; }
  PremiumType Premium() const { return premium_; }
  const String& CdnEndpoint() const { return cdn_endpoint_; }

  void SetConfiguration(const std::shared_ptr<Configuration>& configuration);

  std::string GetAvatarURL(AvatarFormat format = AvatarFormat::PNG, AvatarSize size = AvatarSize::x128) const;
  String GetAvatarDecorationURL(AvatarFormat format = AvatarFormat::PNG) const;
  std::string GetAvatarExtension(AvatarFormat format) const;

  String ToString() const;

  static std::shared_ptr<User> FromToken(const json::Token& token);

 private:
  std::uint64_t id_ = 0;
  String username_;
  int discriminator_ = 0;
  String display_name_;
  String avatar_;
  std::optional<AvatarDecorationData> avatar_decoration_;
  bool bot_ = false;
  Flag flags_ = Flag::None;
  PremiumType premium_ = PremiumType::None;
  String cdn_endpoint_;
};

namespace helper {
std::string ToString(const std::shared_ptr<User>& user);
}

}

#endif
