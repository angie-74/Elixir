// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/json.h"

#include <cmath>
#include <limits>

namespace discord_rpc::json {
namespace {

bool IsBlank(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

std::string_view Strip(std::string_view text) {
  while (!text.empty() && IsBlank(text.front())) text.remove_prefix(1);
  while (!text.empty() && IsBlank(text.back())) text.remove_suffix(1);
  return text;
}

bool ParseInteger(std::string_view text, bool* negative, std::uint64_t* magnitude) {
  text = Strip(text);
  *negative = false;
  if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
    *negative = text.front() == '-';
    text.remove_prefix(1);
  }
  if (text.empty()) return false;
  std::uint64_t value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
    const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return false;
    value = value * 10 + digit;
  }
  *magnitude = value;
  return true;
}

const char* TokenName(const Token& value) {
  return value.is_object() ? "StartObject" : value.is_array() ? "StartArray" : value.is_boolean() ? "Boolean" : "Value";
}

}

std::string Escape(std::string_view raw) {
  const std::string text = helper::WellFormed(raw);
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(text.size() + 8);
  for (std::size_t index = 0; index < text.size(); ++index) {
    const unsigned char c = static_cast<unsigned char>(text[index]);
    switch (c) {
      case '"': out += "\\\""; continue;
      case '\\': out += "\\\\"; continue;
      case '\b': out += "\\b"; continue;
      case '\f': out += "\\f"; continue;
      case '\n': out += "\\n"; continue;
      case '\r': out += "\\r"; continue;
      case '\t': out += "\\t"; continue;
      default: break;
    }
    if (c < 0x20) {
      out += "\\u00";
      out += kHex[c >> 4];
      out += kHex[c & 0x0F];
    } else if (c == 0xC2 && index + 1 < text.size() && static_cast<unsigned char>(text[index + 1]) == 0x85) {
      out += "\\u0085";
      index += 1;
    } else if (c == 0xE2 && index + 2 < text.size() && static_cast<unsigned char>(text[index + 1]) == 0x80 &&
               (static_cast<unsigned char>(text[index + 2]) == 0xA8 ||
                static_cast<unsigned char>(text[index + 2]) == 0xA9)) {
      out += static_cast<unsigned char>(text[index + 2]) == 0xA8 ? "\\u2028" : "\\u2029";
      index += 2;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

void Writer::Separate() {
  if (after_name_) {
    after_name_ = false;
    return;
  }
  if (!empty_.empty()) {
    if (!empty_.back()) out_ += ',';
    empty_.back() = false;
  }
}

void Writer::StartObject() {
  Separate();
  out_ += '{';
  empty_.push_back(true);
}

void Writer::EndObject() {
  out_ += '}';
  if (!empty_.empty()) empty_.pop_back();
}

void Writer::StartArray() {
  Separate();
  out_ += '[';
  empty_.push_back(true);
}

void Writer::EndArray() {
  out_ += ']';
  if (!empty_.empty()) empty_.pop_back();
}

void Writer::Name(std::string_view name) {
  Separate();
  out_ += '"';
  out_ += Escape(name);
  out_ += "\":";
  after_name_ = true;
}

void Writer::Null() {
  Separate();
  out_ += "null";
}

void Writer::Text(std::string_view text) {
  Separate();
  out_ += '"';
  out_ += Escape(text);
  out_ += '"';
}

void Writer::TextOrNull(const String& text) {
  if (text.has_value()) {
    Text(*text);
  } else {
    Null();
  }
}

void Writer::Int(long long value) {
  Separate();
  out_ += std::to_string(value);
}

void Writer::UInt(unsigned long long value) {
  Separate();
  out_ += std::to_string(value);
}

void Writer::Raw(std::string_view json) {
  Separate();
  out_ += json;
}

std::optional<Token> Parse(std::string_view text) {
  bool content = false;
  for (const char c : text) content = content || !(c == ' ' || c == '\t' || c == '\r' || c == '\n');
  if (!content) return std::nullopt;
  if (text.substr(0, 3) == "\xEF\xBB\xBF") {
    throw JsonReaderException("Unexpected character encountered while parsing value.");
  }
  Token token = Token::parse(text.begin(), text.end(), nullptr, false, false);
  if (token.is_discarded()) throw JsonReaderException("Unexpected character encountered while parsing value.");
  return token;
}

String ReadAsString(const Token& value, Source source) {
  if (value.is_null()) return std::nullopt;
  if (value.is_string()) return value.get<std::string>();
  if (value.is_boolean()) {
    const bool flag = value.get<bool>();
    return source == Source::kText ? std::string(flag ? "true" : "false") : helper::ToString(flag);
  }
  if (value.is_number_unsigned()) return std::to_string(value.get<std::uint64_t>());
  if (value.is_number_integer()) return std::to_string(value.get<std::int64_t>());
  if (value.is_number_float()) return helper::ToString(value.get<double>());
  throw JsonReaderException(std::string("Error reading string. Unexpected token: ") + TokenName(value) + ".");
}

std::optional<std::int32_t> ReadAsInt32(const Token& value, Source source) {
  if (value.is_null()) return std::nullopt;
  if (value.is_number_unsigned() || value.is_number_integer()) {
    const bool fits = value.is_number_unsigned()
                          ? value.get<std::uint64_t>() <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())
                          : value.get<std::int64_t>() >= std::numeric_limits<std::int32_t>::min() &&
                                value.get<std::int64_t>() <= std::numeric_limits<std::int32_t>::max();
    if (!fits) throw JsonReaderException("JSON integer is too large or small for an Int32.");
    return static_cast<std::int32_t>(value.get<std::int64_t>());
  }
  if (value.is_number_float()) {
    if (source == Source::kText) throw JsonReaderException("Input string is not a valid integer.");
    try {
      return ToInt32(value.get<double>());
    } catch (const OverflowException&) {
      throw JsonReaderException("Could not convert to integer.");
    }
  }
  if (value.is_string()) {
    const std::string& text = value.get_ref<const std::string&>();
    if (text.empty()) return std::nullopt;
    std::int32_t parsed = 0;
    if (!TryParseInt32(text, &parsed)) throw JsonReaderException("Could not convert string to integer: " + text + ".");
    return parsed;
  }
  throw JsonReaderException(std::string("Error reading integer. Unexpected token: ") + TokenName(value) + ".");
}

std::optional<bool> ReadAsBoolean(const Token& value) {
  if (value.is_null()) return std::nullopt;
  if (value.is_boolean()) return value.get<bool>();
  if (value.is_number_unsigned()) return value.get<std::uint64_t>() != 0;
  if (value.is_number_integer()) return value.get<std::int64_t>() != 0;
  if (value.is_number_float()) return value.get<double>() != 0.0;
  if (value.is_string()) {
    const std::string& text = value.get_ref<const std::string&>();
    if (text.empty()) return std::nullopt;
    std::string_view word = Strip(text);
    while (!word.empty() && word.back() == '\0') word.remove_suffix(1);
    if (helper::EqualsIgnoreCase(word, "true")) return true;
    if (helper::EqualsIgnoreCase(word, "false")) return false;
    throw JsonReaderException("Could not convert string to boolean: " + text + ".");
  }
  throw JsonReaderException(std::string("Error reading boolean. Unexpected token: ") + TokenName(value) + ".");
}

std::optional<std::uint64_t> ReadAsUInt64(const Token& value) {
  if (value.is_null()) return std::nullopt;
  if (value.is_number_unsigned()) return value.get<std::uint64_t>();
  if (value.is_number_integer()) {
    if (value.get<std::int64_t>() < 0) throw JsonSerializationException("Error converting value to type 'System.UInt64'.");
    return static_cast<std::uint64_t>(value.get<std::int64_t>());
  }
  if (value.is_boolean()) return value.get<bool>() ? 1u : 0u;
  if (value.is_number_float()) {
    try {
      return ToUInt64(value.get<double>());
    } catch (const OverflowException&) {
      throw JsonSerializationException("Error converting value to type 'System.UInt64'.");
    }
  }
  if (value.is_string()) {
    const std::string& text = value.get_ref<const std::string&>();
    if (text.empty()) return std::nullopt;
    std::uint64_t parsed = 0;
    if (!TryParseUInt64(text, &parsed)) {
      throw JsonSerializationException("Error converting value \"" + text + "\" to type 'System.UInt64'.");
    }
    return parsed;
  }
  throw JsonSerializationException("Error converting value to type 'System.UInt64'.");
}

std::optional<std::int32_t> ReadAsEnum(const Token& value, const std::vector<EnumName>& names) {
  if (value.is_null()) return std::nullopt;
  if (value.is_number_unsigned()) return static_cast<std::int32_t>(static_cast<std::uint32_t>(value.get<std::uint64_t>()));
  if (value.is_number_integer()) return static_cast<std::int32_t>(static_cast<std::uint32_t>(value.get<std::int64_t>()));
  if (value.is_boolean()) return value.get<bool>() ? 1 : 0;
  if (value.is_string()) {
    const std::string& text = value.get_ref<const std::string&>();
    bool negative = false;
    std::uint64_t magnitude = 0;
    if (ParseInteger(text, &negative, &magnitude)) {
      const std::int64_t limit = negative ? 2147483648ll : 2147483647ll;
      if (magnitude > static_cast<std::uint64_t>(limit)) {
        throw JsonSerializationException("Error converting value \"" + text + "\" to an enum.");
      }
      return static_cast<std::int32_t>(negative ? -static_cast<std::int64_t>(magnitude)
                                                : static_cast<std::int64_t>(magnitude));
    }
    std::int32_t combined = 0;
    std::string_view rest = text;
    bool any = false;
    for (;;) {
      const std::size_t comma = rest.find(',');
      const std::string_view name = Strip(rest.substr(0, comma));
      const EnumName* found = nullptr;
      for (const EnumName& candidate : names) {
        if (name == candidate.name) {
          found = &candidate;
          break;
        }
      }
      for (const EnumName& candidate : names) {
        if (found == nullptr && helper::EqualsIgnoreCase(name, candidate.name)) found = &candidate;
      }
      if (found == nullptr) throw JsonSerializationException("Error converting value \"" + text + "\" to an enum.");
      combined |= found->value;
      any = true;
      if (comma == std::string_view::npos) break;
      rest.remove_prefix(comma + 1);
    }
    if (any) return combined;
  }
  throw JsonSerializationException("Error converting value to an enum.");
}

int MatchProperty(std::string_view name, const std::vector<std::string_view>& names) {
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (names[index] == name) return static_cast<int>(index);
  }
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (helper::EqualsIgnoreCase(names[index], name)) return static_cast<int>(index);
  }
  return -1;
}

bool TryParseUInt64(std::string_view text, std::uint64_t* value) {
  bool negative = false;
  std::uint64_t magnitude = 0;
  if (!ParseInteger(text, &negative, &magnitude) || (negative && magnitude != 0)) return false;
  *value = magnitude;
  return true;
}

bool TryParseInt32(std::string_view text, std::int32_t* value) {
  bool negative = false;
  std::uint64_t magnitude = 0;
  if (!ParseInteger(text, &negative, &magnitude)) return false;
  if (magnitude > (negative ? 2147483648ull : 2147483647ull)) return false;
  *value = static_cast<std::int32_t>(negative ? -static_cast<std::int64_t>(magnitude)
                                               : static_cast<std::int64_t>(magnitude));
  return true;
}

std::uint64_t ToUInt64(double value) {
  const double rounded = std::nearbyint(value);
  if (!(rounded >= 0.0) || rounded >= 18446744073709551616.0) throw OverflowException();
  return static_cast<std::uint64_t>(rounded);
}

std::int32_t ToInt32(double value) {
  const double rounded = std::nearbyint(value);
  if (!(rounded >= -2147483648.0) || rounded > 2147483647.0) throw OverflowException();
  return static_cast<std::int32_t>(rounded);
}

}
