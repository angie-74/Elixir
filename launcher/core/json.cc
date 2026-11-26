// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/json.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>

namespace elixir::launcher {

namespace {

constexpr int kMaxDepth = 256;

long DecodeUtf8(std::string_view text, std::size_t* i) {
  const auto byte = [&](std::size_t k) { return static_cast<unsigned char>(text[k]); };
  const unsigned char lead = byte(*i);
  if (lead < 0x80) {
    ++*i;
    return lead;
  }
  int length = 0;
  long cp = 0;
  long min = 0;
  if ((lead & 0xE0) == 0xC0) {
    length = 2;
    cp = lead & 0x1F;
    min = 0x80;
  } else if ((lead & 0xF0) == 0xE0) {
    length = 3;
    cp = lead & 0x0F;
    min = 0x800;
  } else if ((lead & 0xF8) == 0xF0) {
    length = 4;
    cp = lead & 0x07;
    min = 0x10000;
  } else {
    ++*i;
    return -1;
  }
  if (*i + static_cast<std::size_t>(length) > text.size()) {
    ++*i;
    return -1;
  }
  for (int k = 1; k < length; ++k) {
    const unsigned char next = byte(*i + static_cast<std::size_t>(k));
    if ((next & 0xC0) != 0x80) {
      ++*i;
      return -1;
    }
    cp = (cp << 6) | (next & 0x3F);
  }
  if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
    ++*i;
    return -1;
  }
  *i += static_cast<std::size_t>(length);
  return cp;
}

void AppendUtf8(std::string* out, unsigned long cp) {
  if (cp < 0x80) {
    out->push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

}

bool IsValidUtf8(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size()) {
    if (DecodeUtf8(text, &i) < 0) return false;
  }
  return true;
}

void AppendJsonString(std::string* out, std::string_view value) {
  out->push_back('"');
  std::size_t i = 0;
  while (i < value.size()) {
    const std::size_t start = i;
    const long cp = DecodeUtf8(value, &i);
    if (cp < 0) {
      out->append("\xEF\xBF\xBD");
      continue;
    }
    switch (cp) {
      case '"': out->append("\\\""); break;
      case '\\': out->append("\\\\"); break;
      case '\b': out->append("\\b"); break;
      case '\f': out->append("\\f"); break;
      case '\n': out->append("\\n"); break;
      case '\r': out->append("\\r"); break;
      case '\t': out->append("\\t"); break;
      default:
        if (cp < 0x20 || cp == 0x7F || cp == 0x2028 || cp == 0x2029) {
          char escape[8];
          std::snprintf(escape, sizeof(escape), "\\u%04lx", static_cast<unsigned long>(cp));
          out->append(escape);
        } else {
          out->append(value.substr(start, i - start));
        }
    }
  }
  out->push_back('"');
}

std::string JsonQuote(std::string_view value) {
  std::string out;
  AppendJsonString(&out, value);
  return out;
}

Json Json::MakeArray() {
  Json value;
  value.type_ = Type::kArray;
  return value;
}

Json Json::MakeObject() {
  Json value;
  value.type_ = Type::kObject;
  return value;
}

std::int64_t Json::AsInt(std::int64_t fallback) const {
  if (!is_number()) return fallback;
  if (is_integer_) return integer_;
  if (!std::isfinite(number_) || std::floor(number_) != number_) return fallback;
  if (number_ < -9.2233720368547758e18 || number_ >= 9.2233720368547758e18) return fallback;
  return static_cast<std::int64_t>(number_);
}

void Json::Push(Json value) {
  if (type_ == Type::kNull) type_ = Type::kArray;
  array_.push_back(std::move(value));
}

const Json* Json::Find(std::string_view key) const {
  if (type_ != Type::kObject) return nullptr;
  for (const Member& member : object_) {
    if (member.first == key) return &member.second;
  }
  return nullptr;
}

Json& Json::Set(std::string_view key, Json value) {
  if (type_ == Type::kNull) type_ = Type::kObject;
  for (Member& member : object_) {
    if (member.first == key) {
      member.second = std::move(value);
      return member.second;
    }
  }
  object_.emplace_back(std::string(key), std::move(value));
  return object_.back().second;
}

std::string Json::GetString(std::string_view key, std::string_view fallback) const {
  const Json* value = Find(key);
  return value != nullptr && value->is_string() ? value->string_ : std::string(fallback);
}

bool Json::GetBool(std::string_view key, bool fallback) const {
  const Json* value = Find(key);
  return value != nullptr ? value->AsBool(fallback) : fallback;
}

double Json::GetDouble(std::string_view key, double fallback) const {
  const Json* value = Find(key);
  return value != nullptr ? value->AsDouble(fallback) : fallback;
}

std::int64_t Json::GetInt(std::string_view key, std::int64_t fallback) const {
  const Json* value = Find(key);
  return value != nullptr ? value->AsInt(fallback) : fallback;
}

std::string Json::Dump(int indent) const {
  std::string out;
  DumpTo(&out, indent, 0);
  if (indent >= 0) out.push_back('\n');
  return out;
}

void Json::DumpTo(std::string* out, int indent, int depth) const {
  const auto newline = [&](int level) {
    if (indent < 0) return;
    out->push_back('\n');
    out->append(static_cast<std::size_t>(indent * level), ' ');
  };
  switch (type_) {
    case Type::kNull: out->append("null"); break;
    case Type::kBool: out->append(bool_ ? "true" : "false"); break;
    case Type::kNumber: {
      char buffer[64];
      if (is_integer_) {
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), integer_);
        out->append(buffer, result.ptr);
      } else if (!std::isfinite(number_)) {
        out->append("null");
      } else {
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), number_);
        out->append(buffer, result.ptr);
      }
      break;
    }
    case Type::kString: AppendJsonString(out, string_); break;
    case Type::kArray:
      if (array_.empty()) {
        out->append("[]");
        break;
      }
      out->push_back('[');
      for (std::size_t i = 0; i < array_.size(); ++i) {
        if (i > 0) out->push_back(',');
        newline(depth + 1);
        array_[i].DumpTo(out, indent, depth + 1);
      }
      newline(depth);
      out->push_back(']');
      break;
    case Type::kObject:
      if (object_.empty()) {
        out->append("{}");
        break;
      }
      out->push_back('{');
      for (std::size_t i = 0; i < object_.size(); ++i) {
        if (i > 0) out->push_back(',');
        newline(depth + 1);
        AppendJsonString(out, object_[i].first);
        out->append(indent < 0 ? ":" : ": ");
        object_[i].second.DumpTo(out, indent, depth + 1);
      }
      newline(depth);
      out->push_back('}');
      break;
  }
}

class JsonParser {
 public:
  JsonParser(std::string_view text, std::string* error) : text_(text), error_(error) {}

  std::optional<Json> Run() {
    if (text_.substr(0, 3) == "\xEF\xBB\xBF") pos_ = 3;
    Json value;
    SkipSpace();
    if (!ParseValue(&value, 0)) return std::nullopt;
    SkipSpace();
    if (pos_ != text_.size()) {
      Fail("unexpected trailing characters");
      return std::nullopt;
    }
    return value;
  }

 private:
  bool Fail(const char* message) {
    if (error_ != nullptr) {
      std::size_t line = 1;
      std::size_t column = 1;
      for (std::size_t i = 0; i < pos_ && i < text_.size(); ++i) {
        if (text_[i] == '\n') {
          ++line;
          column = 1;
        } else {
          ++column;
        }
      }
      char buffer[160];
      std::snprintf(buffer, sizeof(buffer), "%s at line %zu, column %zu", message, line, column);
      *error_ = buffer;
    }
    return false;
  }

  void SkipSpace() {
    while (pos_ < text_.size() &&
           (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r')) {
      ++pos_;
    }
  }

  bool Literal(std::string_view word) {
    if (text_.substr(pos_, word.size()) != word) return Fail("invalid literal");
    pos_ += word.size();
    return true;
  }

  bool ParseValue(Json* out, int depth) {
    if (depth > kMaxDepth) return Fail("nesting too deep");
    if (pos_ >= text_.size()) return Fail("unexpected end of input");
    switch (text_[pos_]) {
      case 'n': *out = Json(); return Literal("null");
      case 't': *out = Json(true); return Literal("true");
      case 'f': *out = Json(false); return Literal("false");
      case '"': {
        std::string value;
        if (!ParseString(&value)) return false;
        *out = Json(std::move(value));
        return true;
      }
      case '[': return ParseArray(out, depth);
      case '{': return ParseObject(out, depth);
      default:
        if (text_[pos_] == '-' || (text_[pos_] >= '0' && text_[pos_] <= '9')) return ParseNumber(out);
        return Fail("unexpected character");
    }
  }

  bool ParseNumber(Json* out) {
    const std::size_t start = pos_;
    bool integral = true;
    if (text_[pos_] == '-') ++pos_;
    if (pos_ >= text_.size()) return Fail("invalid number");
    if (text_[pos_] == '0') {
      ++pos_;
    } else if (text_[pos_] >= '1' && text_[pos_] <= '9') {
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
    } else {
      return Fail("invalid number");
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      integral = false;
      ++pos_;
      if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') return Fail("invalid number");
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      integral = false;
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
      if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') return Fail("invalid number");
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
    }
    const char* first = text_.data() + start;
    const char* last = text_.data() + pos_;
    if (integral) {
      std::int64_t value = 0;
      const auto result = std::from_chars(first, last, value);
      if (result.ec == std::errc() && result.ptr == last) {
        *out = Json(value);
        return true;
      }
    }
    double value = 0.0;
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc() || result.ptr != last || !std::isfinite(value)) return Fail("number out of range");
    *out = Json(value);
    return true;
  }

  bool ParseHex4(unsigned* value) {
    if (pos_ + 4 > text_.size()) return Fail("truncated \\u escape");
    unsigned result = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = text_[pos_++];
      result <<= 4;
      if (c >= '0' && c <= '9') result |= static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f') result |= static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') result |= static_cast<unsigned>(c - 'A' + 10);
      else return Fail("invalid \\u escape");
    }
    *value = result;
    return true;
  }

  bool ParseString(std::string* out) {
    ++pos_;
    for (;;) {
      if (pos_ >= text_.size()) return Fail("unterminated string");
      const unsigned char c = static_cast<unsigned char>(text_[pos_]);
      if (c == '"') {
        ++pos_;
        return true;
      }
      if (c < 0x20) return Fail("control character in string");
      if (c == '\\') {
        ++pos_;
        if (pos_ >= text_.size()) return Fail("unterminated escape");
        const char e = text_[pos_++];
        switch (e) {
          case '"': out->push_back('"'); break;
          case '\\': out->push_back('\\'); break;
          case '/': out->push_back('/'); break;
          case 'b': out->push_back('\b'); break;
          case 'f': out->push_back('\f'); break;
          case 'n': out->push_back('\n'); break;
          case 'r': out->push_back('\r'); break;
          case 't': out->push_back('\t'); break;
          case 'u': {
            unsigned cp = 0;
            if (!ParseHex4(&cp)) return false;
            if (cp >= 0xD800 && cp <= 0xDBFF) {
              if (text_.substr(pos_, 2) != "\\u") return Fail("unpaired surrogate");
              pos_ += 2;
              unsigned low = 0;
              if (!ParseHex4(&low)) return false;
              if (low < 0xDC00 || low > 0xDFFF) return Fail("unpaired surrogate");
              cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
              return Fail("unpaired surrogate");
            }
            AppendUtf8(out, cp);
            break;
          }
          default: --pos_; return Fail("invalid escape");
        }
        continue;
      }
      const std::size_t start = pos_;
      if (DecodeUtf8(text_, &pos_) < 0) {
        pos_ = start;
        return Fail("invalid UTF-8");
      }
      out->append(text_.substr(start, pos_ - start));
    }
  }

  bool ParseArray(Json* out, int depth) {
    ++pos_;
    *out = Json::MakeArray();
    SkipSpace();
    if (pos_ < text_.size() && text_[pos_] == ']') {
      ++pos_;
      return true;
    }
    for (;;) {
      Json item;
      SkipSpace();
      if (!ParseValue(&item, depth + 1)) return false;
      out->array_.push_back(std::move(item));
      SkipSpace();
      if (pos_ >= text_.size()) return Fail("unterminated array");
      if (text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (text_[pos_] == ']') {
        ++pos_;
        return true;
      }
      return Fail("expected ',' or ']'");
    }
  }

  bool ParseObject(Json* out, int depth) {
    ++pos_;
    *out = Json::MakeObject();
    SkipSpace();
    if (pos_ < text_.size() && text_[pos_] == '}') {
      ++pos_;
      return true;
    }
    for (;;) {
      SkipSpace();
      if (pos_ >= text_.size() || text_[pos_] != '"') return Fail("expected member name");
      std::string key;
      if (!ParseString(&key)) return false;
      SkipSpace();
      if (pos_ >= text_.size() || text_[pos_] != ':') return Fail("expected ':'");
      ++pos_;
      SkipSpace();
      Json value;
      if (!ParseValue(&value, depth + 1)) return false;
      out->Set(key, std::move(value));
      SkipSpace();
      if (pos_ >= text_.size()) return Fail("unterminated object");
      if (text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (text_[pos_] == '}') {
        ++pos_;
        return true;
      }
      return Fail("expected ',' or '}'");
    }
  }

  std::string_view text_;
  std::string* error_;
  std::size_t pos_ = 0;
};

std::optional<Json> Json::Parse(std::string_view text, std::string* error) {
  return JsonParser(text, error).Run();
}

}
