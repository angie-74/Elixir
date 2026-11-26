// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_JSON_H_
#define DISCORD_RPC_JSON_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "discord_rpc/exceptions.h"
#include "discord_rpc/helper.h"

namespace discord_rpc::json {

using Token = nlohmann::ordered_json;

class Writer {
 public:
  void StartObject();
  void EndObject();
  void StartArray();
  void EndArray();
  void Name(std::string_view name);
  void Null();
  void Text(std::string_view text);
  void TextOrNull(const String& text);
  void Int(long long value);
  void UInt(unsigned long long value);
  void Raw(std::string_view json);

  std::string Take() { return std::move(out_); }

 private:
  void Separate();

  std::string out_;
  std::vector<bool> empty_;
  bool after_name_ = false;
};

std::string Escape(std::string_view text);

std::optional<Token> Parse(std::string_view text);

enum class Source { kText, kToken };

String ReadAsString(const Token& value, Source source);
std::optional<std::int32_t> ReadAsInt32(const Token& value, Source source);
std::optional<bool> ReadAsBoolean(const Token& value);
std::optional<std::uint64_t> ReadAsUInt64(const Token& value);

struct EnumName {
  const char* name;
  std::int32_t value;
};

std::optional<std::int32_t> ReadAsEnum(const Token& value, const std::vector<EnumName>& names);

template <typename T>
T Required(const std::optional<T>& value, const char* type) {
  if (!value.has_value()) {
    throw JsonSerializationException(std::string("Error converting value {null} to type '") + type + "'.");
  }
  return *value;
}

int MatchProperty(std::string_view name, const std::vector<std::string_view>& names);

bool TryParseUInt64(std::string_view text, std::uint64_t* value);
bool TryParseInt32(std::string_view text, std::int32_t* value);

std::uint64_t ToUInt64(double value);
std::int32_t ToInt32(double value);

}

#endif
