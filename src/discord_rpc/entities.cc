// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/entities.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

#include "discord_rpc/exceptions.h"

namespace discord_rpc {
namespace {

using helper::StringTools;

constexpr char kExternalKeyPrefix[] = "mp:external";

bool StartsWith(std::string_view text, std::string_view prefix) { return text.substr(0, prefix.size()) == prefix; }

std::int64_t FileTimeTicks(const FILETIME& time) {
  const std::int64_t since_1601 = (static_cast<std::int64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
  return since_1601 + 504911232000000000;
}

bool IsSlash(char c) { return c == '/' || c == '\\'; }
bool IsLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

bool IsHostName(std::string_view host) {
  if (host.empty() || host.size() > 255) return false;
  std::size_t label = 0;
  for (std::size_t index = 0; index < host.size(); ++index) {
    const char c = host[index];
    if (c == '.') {
      if (label == 0) return false;
      label = 0;
      continue;
    }
    if (!IsLetter(c) && !IsDigit(c) && c != '-' && c != '_' && static_cast<unsigned char>(c) < 0x80) return false;
    if (++label > 63) return false;
  }
  return true;
}

bool IsIpv6(std::string_view address) {
  const std::size_t zone = address.find('%');
  if (zone != std::string_view::npos) address = address.substr(0, zone);
  if (address.empty()) return false;
  int groups = 0;
  bool compressed = false;
  std::size_t index = 0;
  if (StartsWith(address, "::")) {
    compressed = true;
    index = 2;
  } else if (address.front() == ':') {
    return false;
  }
  while (index < address.size()) {
    std::size_t end = index;
    while (end < address.size() && address[end] != ':') ++end;
    const std::string_view group = address.substr(index, end - index);
    if (group.find('.') != std::string_view::npos) {
      int parts = 0;
      std::size_t at = 0;
      while (at <= group.size()) {
        std::size_t dot = group.find('.', at);
        if (dot == std::string_view::npos) dot = group.size();
        const std::string_view part = group.substr(at, dot - at);
        if (part.empty() || part.size() > 3 || !std::all_of(part.begin(), part.end(), IsDigit)) return false;
        ++parts;
        at = dot + 1;
      }
      if (parts != 4 || end != address.size()) return false;
      groups += 2;
      index = end;
      break;
    }
    if (group.empty() || group.size() > 4 ||
        !std::all_of(group.begin(), group.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; })) {
      return false;
    }
    ++groups;
    index = end;
    if (index < address.size()) {
      ++index;
      if (index < address.size() && address[index] == ':') {
        if (compressed) return false;
        compressed = true;
        ++index;
      } else if (index == address.size()) {
        return false;
      }
    }
  }
  return compressed ? groups < 8 : groups == 8;
}

bool IsAbsoluteUri(std::string_view text) {
  const auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!text.empty() && blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && blank(text.back())) text.remove_suffix(1);
  if (text.empty()) return false;
  if (text.size() >= 2 && IsSlash(text[0]) && IsSlash(text[1])) {
    const std::string_view rest = text.substr(2);
    return IsHostName(rest.substr(0, rest.find_first_of("/\\")));
  }
  if (text.size() >= 3 && IsLetter(text[0]) && text[1] == ':' && IsSlash(text[2])) return true;
  const std::size_t colon = text.find(':');
  if (colon == std::string_view::npos || colon < 2 || colon > 1024 || !IsLetter(text[0])) return false;
  std::string scheme;
  for (const char c : text.substr(0, colon)) {
    if (!IsLetter(c) && !IsDigit(c) && c != '+' && c != '-' && c != '.') return false;
    scheme += c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
  }
  static constexpr std::string_view kWithAuthority[] = {"http", "https", "ftp",    "ws",      "wss",
                                                        "gopher", "nntp", "telnet", "net.tcp", "net.pipe"};
  if (std::find(std::begin(kWithAuthority), std::end(kWithAuthority), scheme) == std::end(kWithAuthority)) return true;
  std::string_view rest = text.substr(colon + 1);
  if (!StartsWith(rest, "//")) return false;
  rest.remove_prefix(2);
  std::string_view authority = rest.substr(0, rest.find_first_of("/?#"));
  const std::size_t at = authority.find('@');
  if (at != std::string_view::npos) authority.remove_prefix(at + 1);
  std::string_view port;
  if (!authority.empty() && authority.front() == '[') {
    const std::size_t close = authority.find(']');
    if (close == std::string_view::npos || !IsIpv6(authority.substr(1, close - 1))) return false;
    port = authority.substr(close + 1);
    if (!port.empty()) {
      if (port.front() != ':') return false;
      port.remove_prefix(1);
    }
  } else {
    const std::size_t separator = authority.find(':');
    if (separator != std::string_view::npos) port = authority.substr(separator + 1);
    if (!IsHostName(authority.substr(0, separator))) return false;
  }
  if (port.size() > 5 || !std::all_of(port.begin(), port.end(), IsDigit)) return false;
  unsigned number = 0;
  for (const char c : port) number = number * 10 + static_cast<unsigned>(c - '0');
  return number <= 65535;
}

template <typename Handler>
void ReadObject(const json::Token& token, const char* type, const std::vector<std::string_view>& names,
                Handler&& handler) {
  if (!token.is_object()) {
    throw JsonSerializationException(std::string("Cannot deserialize the current JSON value into type '") + type +
                                     "'.");
  }
  for (auto member = token.begin(); member != token.end(); ++member) {
    const int index = json::MatchProperty(member.key(), names);
    if (index >= 0) handler(index, member.value());
  }
}

template <typename Setter>
void Set(const char* property, const char* type, Setter&& setter) {
  try {
    setter();
  } catch (const JsonException&) {
    throw;
  } catch (const Exception&) {
    throw JsonSerializationException(std::string("Error setting value to '") + property + "' on '" + type + "'.");
  }
}

const std::vector<json::EnumName>& ActivityTypeNames() {
  static const std::vector<json::EnumName> names = {{"Playing", 0}, {"Listening", 2}, {"Watching", 3}, {"Competing", 5}};
  return names;
}

const std::vector<json::EnumName>& StatusDisplayTypeNames() {
  static const std::vector<json::EnumName> names = {{"Name", 0}, {"State", 1}, {"Details", 2}};
  return names;
}

const std::vector<json::EnumName>& PrivacyNames() {
  static const std::vector<json::EnumName> names = {{"Private", 0}, {"Public", 1}};
  return names;
}

const std::vector<json::EnumName>& FlagNames() {
  static const std::vector<json::EnumName> names = {
      {"None", 0},
      {"Employee", 1 << 0},
      {"Partner", 1 << 1},
      {"HypeSquad", 1 << 2},
      {"BugHunter", 1 << 3},
      {"HouseBravery", 1 << 6},
      {"HouseBrilliance", 1 << 7},
      {"HouseBalance", 1 << 8},
      {"EarlySupporter", 1 << 9},
      {"TeamUser", 1 << 10},
      {"BugHunterLevel2", 1 << 14},
      {"VerifiedBot", 1 << 16},
      {"VerifiedDeveloper", 1 << 17},
      {"CertifiedModerator", 1 << 18},
      {"BotHttpInteractions", 1 << 19},
      {"ActiveDeveloper", 1 << 22},
  };
  return names;
}

const std::vector<json::EnumName>& PremiumNames() {
  static const std::vector<json::EnumName> names = {{"None", 0}, {"NitroClassic", 1}, {"Nitro", 2}, {"NitroBasic", 3}};
  return names;
}

}

DateTime DateTime::UtcNow() {
  FILETIME now{};
  GetSystemTimeAsFileTime(&now);
  return DateTime{FileTimeTicks(now)};
}

DateTime DateTime::Now() {
  SYSTEMTIME local{};
  GetLocalTime(&local);
  FILETIME time{};
  if (!SystemTimeToFileTime(&local, &time)) return UtcNow();
  return DateTime{FileTimeTicks(time)};
}

DateTime DateTime::AddMilliseconds(double value) const {
  const double shifted = value + (value >= 0 ? 0.5 : -0.5);
  if (!(shifted > -9.0e18 && shifted < 9.0e18)) {
    throw ArgumentOutOfRangeException("value", "Value to add was out of range.");
  }
  const std::int64_t millis = static_cast<std::int64_t>(shifted);
  if (millis <= -315537897600000 || millis >= 315537897600000) {
    throw ArgumentOutOfRangeException("value", "Value to add was out of range.");
  }
  return AddTicks(millis * kTicksPerMillisecond);
}

DateTime DateTime::AddTicks(std::int64_t value) const {
  if (value > kMaxTicks - Ticks || value < -Ticks) {
    throw ArgumentOutOfRangeException("value",
                                      "The added or subtracted value results in an un-representable DateTime.");
  }
  return DateTime{Ticks + value};
}

std::shared_ptr<Configuration> Configuration::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto configuration = std::make_shared<Configuration>();
  static const std::vector<std::string_view> names = {"api_endpoint", "cdn_host", "environment"};
  ReadObject(token, "DiscordRPC.Configuration", names, [&](int index, const json::Token& value) {
    String* const members[] = {&configuration->ApiEndpoint, &configuration->CdnHost, &configuration->Environment};
    *members[index] = json::ReadAsString(value, json::Source::kToken);
  });
  return configuration;
}

void Assets::SetLargeImageKey(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &largeimagekey_, false, 256)) throw StringOutOfRangeException(256);

  large_image_id_ = std::nullopt;
}

void Assets::SetLargeImageText(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &largeimagetext_, false, 128)) throw StringOutOfRangeException(128);
}

void Assets::SetLargeImageUrl(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &largeimageurl_, false, 256)) throw StringOutOfRangeException(256);

  if (!BaseRichPresence::ValidateUrl(largeimageurl_)) throw ArgumentException("Url must be a valid URI");
}

void Assets::SetSmallImageKey(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &smallimagekey_, false, 256)) throw StringOutOfRangeException(256);

  small_image_id_ = std::nullopt;
}

void Assets::SetSmallImageText(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &smallimagetext_, false, 128)) throw StringOutOfRangeException(128);
}

void Assets::SetSmallImageUrl(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &smallimageurl_, false, 256)) throw StringOutOfRangeException(256);

  if (!BaseRichPresence::ValidateUrl(smallimageurl_)) throw ArgumentException("Url must be a valid URI");
}

void Assets::Merge(const Assets& other) {
  smallimagetext_ = other.smallimagetext_;
  smallimageurl_ = other.smallimageurl_;
  largeimagetext_ = other.largeimagetext_;
  largeimageurl_ = other.largeimageurl_;

  std::uint64_t number = 0;
  if (!other.largeimagekey_.has_value()) throw NullReferenceException();
  if (StartsWith(*other.largeimagekey_, kExternalKeyPrefix)) {
    is_large_image_key_external_ = true;
    large_image_id_ = other.largeimagekey_;
  } else if (json::TryParseUInt64(*other.largeimagekey_, &number)) {
    is_large_image_key_external_ = false;
    large_image_id_ = other.largeimagekey_;
  } else {
    is_large_image_key_external_ = false;
    large_image_id_ = std::nullopt;
    largeimagekey_ = other.largeimagekey_;
  }

  if (!other.smallimagekey_.has_value()) throw NullReferenceException();
  if (StartsWith(*other.smallimagekey_, kExternalKeyPrefix)) {
    is_small_image_key_external_ = true;
    small_image_id_ = other.smallimagekey_;
  } else if (json::TryParseUInt64(*other.smallimagekey_, &number)) {
    is_small_image_key_external_ = false;
    small_image_id_ = other.smallimagekey_;
  } else {
    is_small_image_key_external_ = false;
    small_image_id_ = std::nullopt;
    smallimagekey_ = other.smallimagekey_;
  }
}

void Assets::WriteJson(json::Writer* writer) const {
  writer->StartObject();
  const std::pair<const char*, const String*> members[] = {
      {"large_image", &largeimagekey_}, {"large_text", &largeimagetext_}, {"large_url", &largeimageurl_},
      {"small_image", &smallimagekey_}, {"small_text", &smallimagetext_}, {"small_url", &smallimageurl_},
  };
  for (const auto& [name, value] : members) {
    if (!value->has_value()) continue;
    writer->Name(name);
    writer->Text(**value);
  }
  writer->EndObject();
}

std::shared_ptr<Assets> Assets::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto assets = std::make_shared<Assets>();
  static const std::vector<std::string_view> names = {
      "large_image", "large_text", "large_url", "LargeImageID", "IsLargeImageKeyExternal",
      "small_image", "small_text", "small_url", "SmallImageID", "IsSmallImageKeyExternal"};
  ReadObject(token, "DiscordRPC.Assets", names, [&](int index, const json::Token& value) {
    if (index == 3 || index == 4 || index == 8 || index == 9) return;
    const String text = json::ReadAsString(value, json::Source::kToken);
    if (!text.has_value()) return;
    Set(names[static_cast<std::size_t>(index)].data(), "DiscordRPC.Assets", [&] {
      switch (index) {
        case 0: assets->SetLargeImageKey(text); break;
        case 1: assets->SetLargeImageText(text); break;
        case 2: assets->SetLargeImageUrl(text); break;
        case 5: assets->SetSmallImageKey(text); break;
        case 6: assets->SetSmallImageText(text); break;
        default: assets->SetSmallImageUrl(text); break;
      }
    });
  });
  return assets;
}

void Button::SetLabel(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &label_, true, 31)) throw StringOutOfRangeException(31);
}

void Button::SetUrl(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &url_, false, 512)) throw StringOutOfRangeException(512);

  if (!BaseRichPresence::ValidateUrl(url_)) throw ArgumentException("Url must be a valid URI");
}

void Party::SetID(const String& value) { partyid_ = StringTools::GetNullOrString(value); }

void Party::WriteJson(json::Writer* writer) const {
  writer->StartObject();
  if (partyid_.has_value()) {
    writer->Name("id");
    writer->Text(*partyid_);
  }
  writer->Name("privacy");
  writer->Int(static_cast<int>(Privacy));
  const int size = std::max(1, Size);
  writer->Name("size");
  writer->StartArray();
  writer->Int(size);
  writer->Int(std::max(size, Max));
  writer->EndArray();
  writer->EndObject();
}

std::shared_ptr<Party> Party::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto party = std::make_shared<Party>();
  static const std::vector<std::string_view> names = {"id", "Size", "Max", "privacy", "size"};
  ReadObject(token, "DiscordRPC.Party", names, [&](int index, const json::Token& value) {
    if (index == 0) {
      const String id = json::ReadAsString(value, json::Source::kToken);
      if (id.has_value()) Set("ID", "DiscordRPC.Party", [&] { party->SetID(id); });
    } else if (index == 3) {
      party->Privacy = static_cast<PrivacySetting>(
          json::Required(json::ReadAsEnum(value, PrivacyNames()), "DiscordRPC.Party+PrivacySetting"));
    } else if (index == 4) {
      if (value.is_null()) return;
      if (!value.is_array()) {
        throw JsonSerializationException("Cannot deserialize the current JSON value into type 'System.Int32[]'.");
      }
      std::vector<int> sizes;
      for (const json::Token& item : value) {
        sizes.push_back(json::Required(json::ReadAsInt32(item, json::Source::kToken), "System.Int32"));
      }
      if (sizes.size() != 2) {
        party->Size = 0;
        party->Max = 0;
      } else {
        party->Size = sizes[0];
        party->Max = sizes[1];
      }
    }
  });
  return party;
}

void Secrets::SetJoinSecret(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &join_secret_, false, 128)) throw StringOutOfRangeException(128);
}

void Secrets::SetSpectateSecret(const String& value) {
  if (!BaseRichPresence::ValidateString(value, &spectate_secret_, false, 128)) throw StringOutOfRangeException(128);
}

std::string Secrets::CreateSecret(Random* random) {
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(SecretLength()));
  random->NextBytes(&bytes);

  return helper::WellFormed(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

std::string Secrets::CreateFriendlySecret(Random* random) {
  static constexpr std::string_view charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::string builder;
  for (int i = 0; i < SecretLength(); i++) {
    builder += charset[static_cast<std::size_t>(random->Next(static_cast<int>(charset.size())))];
  }
  return builder;
}

void Secrets::WriteJson(json::Writer* writer) const {
  writer->StartObject();
  writer->Name("MatchSecret");
  writer->TextOrNull(MatchSecret);
  if (join_secret_.has_value()) {
    writer->Name("join");
    writer->Text(*join_secret_);
  }
  if (spectate_secret_.has_value()) {
    writer->Name("spectate");
    writer->Text(*spectate_secret_);
  }
  writer->EndObject();
}

std::shared_ptr<Secrets> Secrets::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto secrets = std::make_shared<Secrets>();
  static const std::vector<std::string_view> names = {"MatchSecret", "join", "spectate"};
  ReadObject(token, "DiscordRPC.Secrets", names, [&](int index, const json::Token& value) {
    const String text = json::ReadAsString(value, json::Source::kToken);
    if (index == 0) {
      secrets->MatchSecret = text;
    } else if (text.has_value()) {
      Set(index == 1 ? "JoinSecret" : "SpectateSecret", "DiscordRPC.Secrets", [&] {
        if (index == 1) {
          secrets->SetJoinSecret(text);
        } else {
          secrets->SetSpectateSecret(text);
        }
      });
    }
  });
  return secrets;
}

std::shared_ptr<Timestamps> Timestamps::Now() { return std::make_shared<Timestamps>(DateTime::UtcNow()); }

std::shared_ptr<Timestamps> Timestamps::FromTimeSpan(double seconds) {
  if (std::isnan(seconds)) throw ArgumentException("TimeSpan does not accept floating point Not-a-Number values.");
  const double millis = seconds * 1000 + (seconds >= 0 ? 0.5 : -0.5);
  if (millis > 922337203685477.0 || millis < -922337203685477.0) throw OverflowException();
  const std::int64_t ticks = static_cast<std::int64_t>(millis) * DateTime::kTicksPerMillisecond;
  auto timestamps = std::make_shared<Timestamps>();
  timestamps->Start = DateTime::UtcNow();
  timestamps->End = DateTime::UtcNow().AddTicks(ticks);
  return timestamps;
}

std::optional<std::uint64_t> Timestamps::StartUnixMilliseconds() const {
  return Start.has_value() ? std::optional<std::uint64_t>(ToUnixMilliseconds(*Start)) : std::nullopt;
}

void Timestamps::SetStartUnixMilliseconds(const std::optional<std::uint64_t>& value) {
  Start = value.has_value() ? std::optional<DateTime>(FromUnixMilliseconds(*value)) : std::nullopt;
}

std::optional<std::uint64_t> Timestamps::EndUnixMilliseconds() const {
  return End.has_value() ? std::optional<std::uint64_t>(ToUnixMilliseconds(*End)) : std::nullopt;
}

void Timestamps::SetEndUnixMilliseconds(const std::optional<std::uint64_t>& value) {
  End = value.has_value() ? std::optional<DateTime>(FromUnixMilliseconds(*value)) : std::nullopt;
}

DateTime Timestamps::FromUnixMilliseconds(std::uint64_t unix_time) {
  const DateTime epoch{DateTime::kUnixEpochTicks};
  return epoch.AddMilliseconds(static_cast<double>(unix_time));
}

std::uint64_t Timestamps::ToUnixMilliseconds(DateTime date) {
  double milliseconds = static_cast<double>(date.Ticks - DateTime::kUnixEpochTicks) * (1.0 / DateTime::kTicksPerMillisecond);
  milliseconds = std::min(std::max(milliseconds, -922337203685477.0), 922337203685477.0);
  return json::ToUInt64(milliseconds);
}

void Timestamps::WriteJson(json::Writer* writer) const {
  writer->StartObject();
  for (const bool start : {true, false}) {
    std::optional<std::uint64_t> value;
    try {
      value = start ? StartUnixMilliseconds() : EndUnixMilliseconds();
    } catch (const Exception&) {
      throw JsonSerializationException(std::string("Error getting value from '") +
                                       (start ? "StartUnixMilliseconds" : "EndUnixMilliseconds") +
                                       "' on 'DiscordRPC.Timestamps'.");
    }
    if (!value.has_value()) continue;
    writer->Name(start ? "start" : "end");
    writer->UInt(*value);
  }
  writer->EndObject();
}

std::shared_ptr<Timestamps> Timestamps::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto timestamps = std::make_shared<Timestamps>();
  static const std::vector<std::string_view> names = {"Start", "End", "start", "end"};
  ReadObject(token, "DiscordRPC.Timestamps", names, [&](int index, const json::Token& value) {
    if (index < 2) return;
    const std::optional<std::uint64_t> milliseconds = json::ReadAsUInt64(value);
    if (!milliseconds.has_value()) return;
    Set(index == 2 ? "StartUnixMilliseconds" : "EndUnixMilliseconds", "DiscordRPC.Timestamps", [&] {
      if (index == 2) {
        timestamps->SetStartUnixMilliseconds(milliseconds);
      } else {
        timestamps->SetEndUnixMilliseconds(milliseconds);
      }
    });
  });
  return timestamps;
}

void BaseRichPresence::SetState(const String& value) {
  if (!ValidateString(value, &state_, false, 128)) throw StringOutOfRangeException("State", 0, 128);
}

void BaseRichPresence::SetStateUrl(const String& value) {
  if (!ValidateString(value, &state_url_, false, 256)) throw StringOutOfRangeException(256);

  if (!ValidateUrl(state_url_)) throw ArgumentException("Url must be a valid URI");
}

void BaseRichPresence::SetDetails(const String& value) {
  if (!ValidateString(value, &details_, false, 128)) throw StringOutOfRangeException(128);
}

void BaseRichPresence::SetDetailsUrl(const String& value) {
  if (!ValidateString(value, &details_url_, false, 256)) throw StringOutOfRangeException(256);

  if (!ValidateUrl(details_url_)) throw ArgumentException("Url must be a valid URI");
}

bool BaseRichPresence::HasTimestamps() const {
  return Timestamps != nullptr && (Timestamps->Start.has_value() || Timestamps->End.has_value());
}

bool BaseRichPresence::HasAssets() const { return Assets != nullptr; }

bool BaseRichPresence::HasParty() const { return Party != nullptr && Party->ID().has_value(); }

bool BaseRichPresence::HasSecrets() const {
  return Secrets != nullptr && (Secrets->JoinSecret().has_value() || Secrets->SpectateSecret().has_value());
}

bool BaseRichPresence::ValidateString(const String& str, String* result, bool use_bytes, int length) {
  const String given = str;
  *result = given;
  if (!given.has_value()) return true;

  const std::string s = helper::Trim(*given);

  if ((use_bytes && !StringTools::WithinLength(s, length)) || helper::Length(s) > length) return false;

  *result = StringTools::GetNullOrString(s);
  return true;
}

bool BaseRichPresence::ValidateUrl(const String& url) {
  if (helper::IsNullOrEmpty(url)) return true;

  return IsAbsoluteUri(*url);
}

bool BaseRichPresence::Matches(const RichPresence* other) const {
  if (other == nullptr) return false;

  if (State() != other->State() || StateUrl() != other->StateUrl() || Details() != other->Details() ||
      DetailsUrl() != other->DetailsUrl() || Type != other->Type) {
    return false;
  }

  if (Timestamps != nullptr) {
    if (other->Timestamps == nullptr ||
        other->Timestamps->StartUnixMilliseconds() != Timestamps->StartUnixMilliseconds() ||
        other->Timestamps->EndUnixMilliseconds() != Timestamps->EndUnixMilliseconds()) {
      return false;
    }
  } else if (other->Timestamps != nullptr) {
    return false;
  }

  if (Secrets != nullptr) {
    if (other->Secrets == nullptr || other->Secrets->JoinSecret() != Secrets->JoinSecret() ||
        other->Secrets->SpectateSecret() != Secrets->SpectateSecret()) {
      return false;
    }
  } else if (other->Secrets != nullptr) {
    return false;
  }

  if (Party != nullptr) {
    if (other->Party == nullptr || other->Party->ID() != Party->ID() || other->Party->Max != Party->Max ||
        other->Party->Size != Party->Size || other->Party->Privacy != Party->Privacy) {
      return false;
    }
  } else if (other->Party != nullptr) {
    return false;
  }

  if (Assets != nullptr) {
    if (other->Assets == nullptr || other->Assets->LargeImageKey() != Assets->LargeImageKey() ||
        other->Assets->LargeImageText() != Assets->LargeImageText() ||
        other->Assets->LargeImageUrl() != Assets->LargeImageUrl() ||
        other->Assets->SmallImageKey() != Assets->SmallImageKey() ||
        other->Assets->SmallImageText() != Assets->SmallImageText() ||
        other->Assets->SmallImageUrl() != Assets->SmallImageUrl()) {
      return false;
    }
  } else if (other->Assets != nullptr) {
    return false;
  }

  return true;
}

std::shared_ptr<RichPresence> BaseRichPresence::ToRichPresence() const {
  auto presence = std::make_shared<RichPresence>();
  presence->SetState(State());
  presence->SetStateUrl(StateUrl());
  presence->SetDetails(Details());
  presence->SetDetailsUrl(DetailsUrl());
  presence->Type = Type;
  presence->StatusDisplay = StatusDisplay;

  presence->Party = !HasParty() ? Party : nullptr;
  presence->Secrets = !HasSecrets() ? Secrets : nullptr;

  if (HasAssets()) {
    presence->Assets = std::make_shared<discord_rpc::Assets>();
    presence->Assets->SetSmallImageKey(Assets->SmallImageKey());
    presence->Assets->SetSmallImageText(Assets->SmallImageText());
    presence->Assets->SetSmallImageUrl(Assets->SmallImageUrl());
    presence->Assets->SetLargeImageKey(Assets->LargeImageKey());
    presence->Assets->SetLargeImageText(Assets->LargeImageText());
    presence->Assets->SetLargeImageUrl(Assets->LargeImageUrl());
  }

  if (HasTimestamps()) {
    presence->Timestamps = std::make_shared<discord_rpc::Timestamps>();
    if (Timestamps->Start.has_value()) presence->Timestamps->Start = Timestamps->Start;
    if (Timestamps->End.has_value()) presence->Timestamps->End = Timestamps->End;
  }

  return presence;
}

std::string BaseRichPresence::ToJson() const {
  json::Writer writer;
  WriteJson(&writer);
  return writer.Take();
}

void BaseRichPresence::WriteJson(json::Writer* writer) const {
  writer->StartObject();
  WriteBaseMembers(writer);
  writer->EndObject();
}

void BaseRichPresence::WriteBaseMembers(json::Writer* writer) const {
  const std::pair<const char*, const String*> texts[] = {
      {"state", &state_}, {"state_url", &state_url_}, {"details", &details_}, {"details_url", &details_url_}};
  for (const auto& [name, value] : texts) {
    if (!value->has_value()) continue;
    writer->Name(name);
    writer->Text(**value);
  }
  if (Timestamps != nullptr) {
    writer->Name("timestamps");
    Timestamps->WriteJson(writer);
  }
  if (Assets != nullptr) {
    writer->Name("assets");
    Assets->WriteJson(writer);
  }
  if (Party != nullptr) {
    writer->Name("party");
    Party->WriteJson(writer);
  }
  if (Secrets != nullptr) {
    writer->Name("secrets");
    Secrets->WriteJson(writer);
  }
  writer->Name("type");
  writer->Int(static_cast<int>(Type));
  writer->Name("status_display_type");
  writer->Int(static_cast<int>(StatusDisplay));
}

RichPresence& RichPresence::WithState(const String& state) {
  SetState(state);
  return *this;
}

RichPresence& RichPresence::WithStateUrl(const String& state_url) {
  SetStateUrl(state_url);
  return *this;
}

RichPresence& RichPresence::WithDetails(const String& details) {
  SetDetails(details);
  return *this;
}

RichPresence& RichPresence::WithDetailsUrl(const String& details_url) {
  SetDetailsUrl(details_url);
  return *this;
}

RichPresence& RichPresence::WithType(ActivityType type) {
  Type = type;
  return *this;
}

RichPresence& RichPresence::WithStatusDisplay(StatusDisplayType status_display) {
  StatusDisplay = status_display;
  return *this;
}

RichPresence& RichPresence::WithTimestamps(std::shared_ptr<discord_rpc::Timestamps> timestamps) {
  Timestamps = std::move(timestamps);
  return *this;
}

RichPresence& RichPresence::WithAssets(std::shared_ptr<discord_rpc::Assets> assets) {
  Assets = std::move(assets);
  return *this;
}

RichPresence& RichPresence::WithParty(std::shared_ptr<discord_rpc::Party> party) {
  Party = std::move(party);
  return *this;
}

RichPresence& RichPresence::WithSecrets(std::shared_ptr<discord_rpc::Secrets> secrets) {
  Secrets = std::move(secrets);
  return *this;
}

RichPresence& RichPresence::WithButtons(std::shared_ptr<Button> top_button, std::shared_ptr<Button> bottom_button) {
  if (top_button != nullptr && bottom_button != nullptr) {
    Buttons = std::make_shared<ButtonArray>(ButtonArray{top_button, bottom_button});
  } else if (top_button == nullptr && bottom_button == nullptr) {
    Buttons = nullptr;
  } else {
    Buttons = std::make_shared<ButtonArray>(ButtonArray{top_button != nullptr ? top_button : bottom_button});
  }

  return *this;
}

std::shared_ptr<RichPresence> RichPresence::Clone() const {
  auto clone = std::make_shared<RichPresence>();
  clone->SetState(state_);
  clone->SetStateUrl(state_url_);
  clone->SetDetails(details_);
  clone->SetDetailsUrl(details_url_);
  clone->Type = Type;
  clone->StatusDisplay = StatusDisplay;

  clone->Buttons = !HasButtons() ? nullptr : std::make_shared<ButtonArray>(*Buttons);
  if (HasSecrets()) {
    auto secrets = std::make_shared<discord_rpc::Secrets>();
    secrets->SetJoinSecret(Secrets->JoinSecret());
    secrets->SetSpectateSecret(Secrets->SpectateSecret());
    clone->Secrets = secrets;
  }
  if (HasTimestamps()) {
    auto timestamps = std::make_shared<discord_rpc::Timestamps>();
    timestamps->Start = Timestamps->Start;
    timestamps->End = Timestamps->End;
    clone->Timestamps = timestamps;
  }
  if (HasAssets()) {
    auto assets = std::make_shared<discord_rpc::Assets>();
    assets->SetLargeImageKey(Assets->LargeImageKey());
    assets->SetLargeImageText(Assets->LargeImageText());
    assets->SetLargeImageUrl(Assets->LargeImageUrl());
    assets->SetSmallImageKey(Assets->SmallImageKey());
    assets->SetSmallImageText(Assets->SmallImageText());
    assets->SetSmallImageUrl(Assets->SmallImageUrl());
    clone->Assets = assets;
  }
  if (HasParty()) {
    auto party = std::make_shared<discord_rpc::Party>();
    party->SetID(Party->ID());
    party->Size = Party->Size;
    party->Max = Party->Max;
    party->Privacy = Party->Privacy;
    clone->Party = party;
  }
  return clone;
}

RichPresence& RichPresence::Merge(const BaseRichPresence& presence) {
  state_ = presence.State();
  state_url_ = presence.StateUrl();
  details_ = presence.Details();
  details_url_ = presence.DetailsUrl();
  Type = presence.Type;
  StatusDisplay = presence.StatusDisplay;
  Party = presence.Party;
  Timestamps = presence.Timestamps;
  Secrets = presence.Secrets;

  if (presence.HasAssets()) {
    if (!HasAssets()) {
      Assets = presence.Assets;
    } else {
      Assets->Merge(*presence.Assets);
    }
  } else {
    Assets = nullptr;
  }

  return *this;
}

bool RichPresence::Matches(const RichPresence* other) const {
  if (!BaseRichPresence::Matches(other)) return false;

  if ((Buttons == nullptr) != (other->Buttons == nullptr)) return false;
  if (Buttons != nullptr) {
    if (Buttons->size() != other->Buttons->size()) return false;
    for (std::size_t i = 0; i < Buttons->size(); i++) {
      const std::shared_ptr<Button>& a = (*Buttons)[i];
      const std::shared_ptr<Button>& b = (*other->Buttons)[i];
      if (a == nullptr || b == nullptr) throw NullReferenceException();
      if (a->Label() != b->Label() || a->Url() != b->Url()) return false;
    }
  }

  return true;
}

void RichPresence::WriteJson(json::Writer* writer) const {
  writer->StartObject();
  if (Buttons != nullptr) {
    writer->Name("buttons");
    writer->StartArray();
    for (const std::shared_ptr<Button>& button : *Buttons) {
      if (button == nullptr) {
        writer->Null();
        continue;
      }
      writer->StartObject();
      writer->Name("label");
      writer->TextOrNull(button->Label());
      writer->Name("url");
      writer->TextOrNull(button->Url());
      writer->EndObject();
    }
    writer->EndArray();
  }
  WriteBaseMembers(writer);
  writer->EndObject();
}

void RichPresenceResponse::WriteJson(json::Writer* writer) const {
  writer->StartObject();
  writer->Name("application_id");
  writer->TextOrNull(ClientID);
  writer->Name("name");
  writer->TextOrNull(Name);
  WriteBaseMembers(writer);
  writer->EndObject();
}

std::shared_ptr<RichPresenceResponse> RichPresenceResponse::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto response = std::make_shared<RichPresenceResponse>();
  static const std::vector<std::string_view> names = {
      "application_id", "name",  "state",   "state_url", "details", "details_url",
      "timestamps",     "assets", "party",  "secrets",   "type",    "status_display_type"};
  constexpr char kType[] = "DiscordRPC.RichPresenceResponse";
  ReadObject(token, kType, names, [&](int index, const json::Token& value) {
    switch (index) {
      case 0: response->ClientID = json::ReadAsString(value, json::Source::kToken); break;
      case 1: response->Name = json::ReadAsString(value, json::Source::kToken); break;
      case 2:
      case 3:
      case 4:
      case 5: {
        const String text = json::ReadAsString(value, json::Source::kToken);
        if (!text.has_value()) break;
        Set(names[static_cast<std::size_t>(index)].data(), kType, [&] {
          if (index == 2) response->SetState(text);
          if (index == 3) response->SetStateUrl(text);
          if (index == 4) response->SetDetails(text);
          if (index == 5) response->SetDetailsUrl(text);
        });
        break;
      }
      case 6:
        if (!value.is_null()) response->Timestamps = discord_rpc::Timestamps::FromToken(value);
        break;
      case 7:
        if (!value.is_null()) response->Assets = discord_rpc::Assets::FromToken(value);
        break;
      case 8:
        if (!value.is_null()) response->Party = discord_rpc::Party::FromToken(value);
        break;
      case 9:
        if (!value.is_null()) response->Secrets = discord_rpc::Secrets::FromToken(value);
        break;
      case 10: {
        const std::optional<std::int32_t> type = json::ReadAsEnum(value, ActivityTypeNames());
        if (type.has_value()) response->Type = static_cast<ActivityType>(*type);
        break;
      }
      default: {
        const std::optional<std::int32_t> display = json::ReadAsEnum(value, StatusDisplayTypeNames());
        if (display.has_value()) response->StatusDisplay = static_cast<StatusDisplayType>(*display);
        break;
      }
    }
  });
  return response;
}

bool User::IsAvatarAnimated() const { return avatar_.has_value() && StartsWith(*avatar_, "a_"); }

void User::SetConfiguration(const std::shared_ptr<Configuration>& configuration) {
  if (configuration == nullptr) throw NullReferenceException();
  cdn_endpoint_ = configuration->CdnHost;
}

std::string User::GetAvatarURL(AvatarFormat format, AvatarSize size) const {
  std::string endpoint = "/avatars/" + std::to_string(id_) + "/" + avatar_.value_or("");

  if (helper::IsNullOrEmpty(avatar_)) {
    if (format != AvatarFormat::PNG) {
      static constexpr const char* kNames[] = {"PNG", "JPEG", "WebP", "GIF"};
      throw BadImageFormatException(std::string("The user has no avatar and the requested format ") +
                                    kNames[static_cast<int>(format)] + " is not supported. (Only supports PNG).");
    }

    int index = static_cast<int>((id_ >> 22) % 6);
    if (discriminator_ > 0) index = discriminator_ % 5;

    endpoint = "/embed/avatars/" + std::to_string(index);
  }

  return "https://" + cdn_endpoint_.value_or("") + endpoint + GetAvatarExtension(format) +
         "?size=" + std::to_string(static_cast<int>(size)) + "&animated=true";
}

String User::GetAvatarDecorationURL(AvatarFormat format) const {
  if (!avatar_decoration_.has_value()) return std::nullopt;

  const std::string endpoint = "/avatar-decoration-presets/" + avatar_decoration_->Asset.value_or("");
  return "https://" + cdn_endpoint_.value_or("") + endpoint + GetAvatarExtension(format);
}

std::string User::GetAvatarExtension(AvatarFormat format) const {
  static constexpr const char* kExtensions[] = {".png", ".jpeg", ".webp", ".gif"};
  return kExtensions[static_cast<int>(format)];
}

String User::ToString() const {
  if (!helper::IsNullOrEmpty(display_name_)) return display_name_;

  if (discriminator_ != 0) {
    const std::string digits = std::to_string(discriminator_ < 0 ? -static_cast<long long>(discriminator_) : discriminator_);
    return username_.value_or("") + "#" + (discriminator_ < 0 ? "-" : "") +
           std::string(digits.size() < 4 ? 4 - digits.size() : 0, '0') + digits;
  }

  return username_;
}

std::shared_ptr<User> User::FromToken(const json::Token& token) {
  if (token.is_null()) return nullptr;
  auto user = std::make_shared<User>();
  static const std::vector<std::string_view> names = {
      "id",   "username", "discriminator", "global_name", "avatar", "IsAvatarAnimated", "avatar_decoration_data",
      "bot",  "flags",    "premium_type",  "CdnEndpoint"};
  ReadObject(token, "DiscordRPC.User", names, [&](int index, const json::Token& value) {
    switch (index) {
      case 0: user->id_ = json::Required(json::ReadAsUInt64(value), "System.UInt64"); break;
      case 1: user->username_ = json::ReadAsString(value, json::Source::kToken); break;
      case 2:
        user->discriminator_ = json::Required(json::ReadAsInt32(value, json::Source::kToken), "System.Int32");
        break;
      case 3: user->display_name_ = json::ReadAsString(value, json::Source::kToken); break;
      case 4: user->avatar_ = json::ReadAsString(value, json::Source::kToken); break;
      case 6: {
        if (value.is_null()) {
          user->avatar_decoration_ = std::nullopt;
          break;
        }
        AvatarDecorationData data;
        static const std::vector<std::string_view> members = {"asset", "skuId"};
        ReadObject(value, "DiscordRPC.User+AvatarDecorationData", members, [&](int member, const json::Token& item) {
          (member == 0 ? data.Asset : data.SKU) = json::ReadAsString(item, json::Source::kToken);
        });
        user->avatar_decoration_ = data;
        break;
      }
      case 7: user->bot_ = json::Required(json::ReadAsBoolean(value), "System.Boolean"); break;
      case 8: {
        const std::optional<std::int32_t> flags = json::ReadAsEnum(value, FlagNames());
        if (flags.has_value()) user->flags_ = static_cast<Flag>(*flags);
        break;
      }
      case 9: {
        const std::optional<std::int32_t> premium = json::ReadAsEnum(value, PremiumNames());
        if (premium.has_value()) user->premium_ = static_cast<PremiumType>(*premium);
        break;
      }
      default: break;
    }
  });
  return user;
}

namespace helper {

std::string ToString(const std::shared_ptr<User>& user) {
  return user != nullptr ? user->ToString().value_or("") : std::string();
}

}

}
