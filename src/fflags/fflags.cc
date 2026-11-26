// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "fflags/fflags.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace elixir::fflags {
namespace {

struct JsonValue {
  enum class Kind { kNull, kBool, kNumber, kString, kArray, kObject };
  Kind kind = Kind::kNull;
  bool boolean = false;
  bool integral = false;
  std::string text;
  std::vector<JsonValue> array;
  std::vector<std::pair<std::string, JsonValue>> object;
  std::size_t offset = 0;
};

class JsonReader {
 public:
  explicit JsonReader(std::string_view text) : text_(text) {}

  bool Parse(JsonValue* out) {
    if (!SkipSpace() || !Value(out, 0) || !SkipSpace()) return false;
    if (position_ != text_.size()) return Fail("unexpected characters after the JSON value");
    return true;
  }

  const std::string& error() const { return error_; }
  std::size_t error_offset() const { return error_offset_; }

 private:
  bool Fail(const char* message) {
    if (error_.empty()) {
      error_ = message;
      error_offset_ = position_;
    }
    return false;
  }

  bool SkipSpace() {
    while (position_ < text_.size()) {
      const char c = text_[position_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++position_;
      } else if (c == '/' && position_ + 1 < text_.size() && text_[position_ + 1] == '/') {
        position_ += 2;
        while (position_ < text_.size() && text_[position_] != '\n') ++position_;
      } else if (c == '/' && position_ + 1 < text_.size() && text_[position_ + 1] == '*') {
        const std::size_t end = text_.find("*/", position_ + 2);
        if (end == std::string_view::npos) return Fail("unterminated comment");
        position_ = end + 2;
      } else {
        break;
      }
    }
    return true;
  }

  bool Value(JsonValue* out, int depth) {
    if (depth > 32) return Fail("nesting too deep");
    if (position_ >= text_.size()) return Fail("unexpected end of input");
    out->offset = position_;
    const char c = text_[position_];
    if (c == '{') return Object(out, depth);
    if (c == '[') return Array(out, depth);
    if (c == '"') {
      out->kind = JsonValue::Kind::kString;
      return String(&out->text);
    }
    if (c == '-' || (c >= '0' && c <= '9')) return Number(out);
    if (Literal("true")) {
      out->kind = JsonValue::Kind::kBool;
      out->boolean = true;
      return true;
    }
    if (Literal("false")) {
      out->kind = JsonValue::Kind::kBool;
      out->boolean = false;
      return true;
    }
    if (Literal("null")) {
      out->kind = JsonValue::Kind::kNull;
      return true;
    }
    return Fail("invalid JSON value");
  }

  bool Literal(std::string_view word) {
    if (text_.substr(position_, word.size()) != word) return false;
    position_ += word.size();
    return true;
  }

  bool Object(JsonValue* out, int depth) {
    out->kind = JsonValue::Kind::kObject;
    ++position_;
    if (!SkipSpace()) return false;
    if (position_ < text_.size() && text_[position_] == '}') {
      ++position_;
      return true;
    }
    for (;;) {
      if (!SkipSpace()) return false;
      if (position_ >= text_.size() || text_[position_] != '"') return Fail("expected a string key");
      std::string key;
      if (!String(&key) || !SkipSpace()) return false;
      if (position_ >= text_.size() || text_[position_] != ':') return Fail("expected ':'");
      ++position_;
      if (!SkipSpace()) return false;
      JsonValue value;
      if (!Value(&value, depth + 1) || !SkipSpace()) return false;
      out->object.emplace_back(std::move(key), std::move(value));
      if (position_ < text_.size() && text_[position_] == ',') {
        ++position_;
        continue;
      }
      if (position_ < text_.size() && text_[position_] == '}') {
        ++position_;
        return true;
      }
      return Fail("expected ',' or '}'");
    }
  }

  bool Array(JsonValue* out, int depth) {
    out->kind = JsonValue::Kind::kArray;
    ++position_;
    if (!SkipSpace()) return false;
    if (position_ < text_.size() && text_[position_] == ']') {
      ++position_;
      return true;
    }
    for (;;) {
      if (!SkipSpace()) return false;
      JsonValue value;
      if (!Value(&value, depth + 1) || !SkipSpace()) return false;
      out->array.push_back(std::move(value));
      if (position_ < text_.size() && text_[position_] == ',') {
        ++position_;
        continue;
      }
      if (position_ < text_.size() && text_[position_] == ']') {
        ++position_;
        return true;
      }
      return Fail("expected ',' or ']'");
    }
  }

  static void AppendUtf8(std::uint32_t code_point, std::string* out) {
    if (code_point < 0x80) {
      out->push_back(static_cast<char>(code_point));
    } else if (code_point < 0x800) {
      out->push_back(static_cast<char>(0xC0 | (code_point >> 6)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point < 0x10000) {
      out->push_back(static_cast<char>(0xE0 | (code_point >> 12)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
      out->push_back(static_cast<char>(0xF0 | (code_point >> 18)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
  }

  bool Hex4(std::uint32_t* out) {
    if (position_ + 4 > text_.size()) return Fail("truncated \\u escape");
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = text_[position_++];
      value <<= 4;
      if (c >= '0' && c <= '9') value |= static_cast<std::uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint32_t>(c - 'A' + 10);
      else return Fail("invalid \\u escape");
    }
    *out = value;
    return true;
  }

  bool String(std::string* out) {
    ++position_;
    while (position_ < text_.size()) {
      const char c = text_[position_];
      if (c == '"') {
        ++position_;
        return true;
      }
      if (static_cast<unsigned char>(c) < 0x20) return Fail("control character in string");
      if (c != '\\') {
        out->push_back(c);
        ++position_;
        continue;
      }
      if (++position_ >= text_.size()) return Fail("truncated escape");
      const char escape = text_[position_++];
      switch (escape) {
        case '"': out->push_back('"'); break;
        case '\\': out->push_back('\\'); break;
        case '/': out->push_back('/'); break;
        case 'b': out->push_back('\b'); break;
        case 'f': out->push_back('\f'); break;
        case 'n': out->push_back('\n'); break;
        case 'r': out->push_back('\r'); break;
        case 't': out->push_back('\t'); break;
        case 'u': {
          std::uint32_t code = 0;
          if (!Hex4(&code)) return false;
          if (code >= 0xD800 && code <= 0xDBFF) {
            if (position_ + 2 > text_.size() || text_[position_] != '\\' || text_[position_ + 1] != 'u') {
              return Fail("unpaired surrogate");
            }
            position_ += 2;
            std::uint32_t low = 0;
            if (!Hex4(&low)) return false;
            if (low < 0xDC00 || low > 0xDFFF) return Fail("unpaired surrogate");
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
          } else if (code >= 0xDC00 && code <= 0xDFFF) {
            return Fail("unpaired surrogate");
          }
          AppendUtf8(code, out);
          break;
        }
        default:
          return Fail("invalid escape");
      }
    }
    return Fail("unterminated string");
  }

  bool Number(JsonValue* out) {
    out->kind = JsonValue::Kind::kNumber;
    const std::size_t start = position_;
    if (text_[position_] == '-') ++position_;
    if (position_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[position_]))) {
      return Fail("invalid number");
    }
    if (text_[position_] == '0' && position_ + 1 < text_.size() &&
        std::isdigit(static_cast<unsigned char>(text_[position_ + 1]))) {
      return Fail("leading zeros are not allowed");
    }
    while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
    out->integral = true;
    if (position_ < text_.size() && text_[position_] == '.') {
      out->integral = false;
      ++position_;
      if (position_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        return Fail("invalid number");
      }
      while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
    }
    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
      out->integral = false;
      ++position_;
      if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
      if (position_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        return Fail("invalid number");
      }
      while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) ++position_;
    }
    out->text.assign(text_.substr(start, position_ - start));
    return true;
  }

  std::string_view text_;
  std::size_t position_ = 0;
  std::string error_;
  std::size_t error_offset_ = 0;
};

std::string Lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

void AppendJsonString(std::string_view text, std::string* out) {
  out->push_back('"');
  for (const char c : text) {
    switch (c) {
      case '"': out->append("\\\""); break;
      case '\\': out->append("\\\\"); break;
      case '\n': out->append("\\n"); break;
      case '\r': out->append("\\r"); break;
      case '\t': out->append("\\t"); break;
      case '\b': out->append("\\b"); break;
      case '\f': out->append("\\f"); break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char escaped[8];
          std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
          out->append(escaped);
        } else {
          out->push_back(c);
        }
    }
  }
  out->push_back('"');
}

bool ReadWholeFile(const std::filesystem::path& path, std::string* out, bool* missing, std::string* error) {
  *missing = false;
  std::error_code ec;
  const auto status = std::filesystem::status(path, ec);
  if (ec || !std::filesystem::exists(status)) {
    *missing = true;
    return true;
  }
  if (!std::filesystem::is_regular_file(status)) {
    *error = "fflags file is not a regular file";
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    *error = "cannot read fflags file";
    return false;
  }
  std::ostringstream contents;
  contents << in.rdbuf();
  *out = contents.str();
  if (out->size() >= 3 && static_cast<unsigned char>((*out)[0]) == 0xEF &&
      static_cast<unsigned char>((*out)[1]) == 0xBB && static_cast<unsigned char>((*out)[2]) == 0xBF) {
    out->erase(0, 3);
  }
  return true;
}

struct BuiltinEntry {
  const char* group;
  const char* name;
};
constexpr BuiltinEntry kBuiltinAllowlist[] = {
    {"Geometry", "DFIntCSGLevelOfDetailSwitchingDistance"},
    {"Geometry", "DFIntCSGLevelOfDetailSwitchingDistanceL12"},
    {"Geometry", "DFIntCSGLevelOfDetailSwitchingDistanceL23"},
    {"Geometry", "DFIntCSGLevelOfDetailSwitchingDistanceL34"},
    {"Rendering", "FFlagHandleAltEnterFullscreenManually"},
    {"Rendering", "DFFlagTextureQualityOverrideEnabled"},
    {"Rendering", "DFIntTextureQualityOverride"},
    {"Rendering", "FIntDebugForceMSAASamples"},
    {"Rendering", "DFFlagDisableDPIScale"},
    {"Rendering", "FFlagDebugGraphicsPreferD3D11"},
    {"Rendering", "FFlagDebugSkyGray"},
    {"Rendering", "DFFlagDebugPauseVoxelizer"},
    {"Rendering", "DFIntDebugFRMQualityLevelOverride"},
    {"Rendering", "FIntFRMMaxGrassDistance"},
    {"Rendering", "FIntFRMMinGrassDistance"},
    {"Rendering", "FFlagDebugGraphicsPreferVulkan"},
    {"Rendering", "FFlagDebugGraphicsPreferOpenGL"},
    {"User Interface", "FIntGrassMovementReducedMotionFactor"},
};
constexpr const char* kAllowlistSource =
    "https://devforum.roblox.com/t/allowlist-for-local-client-configuration-via-fast-flags/3966569";
constexpr const char* kAllowlistUpdated = "2025-10-16T23:33:24Z";

}

FlagType ClassifyFlagName(std::string_view name) {
  std::string_view rest = name;
  if (!rest.empty() && (rest.front() == 'D' || rest.front() == 'S')) rest.remove_prefix(1);
  if (rest.size() < 2 || rest.front() != 'F') return FlagType::kUnknown;
  rest.remove_prefix(1);
  auto starts = [&](std::string_view prefix) {
    return rest.size() > prefix.size() && rest.substr(0, prefix.size()) == prefix;
  };
  if (starts("Flag")) return FlagType::kBool;
  if (starts("Int")) return FlagType::kInt;
  if (starts("String")) return FlagType::kString;
  if (starts("Log")) return FlagType::kLog;
  return FlagType::kUnknown;
}

const char* FlagTypeName(FlagType type) {
  switch (type) {
    case FlagType::kBool: return "boolean";
    case FlagType::kInt: return "integer";
    case FlagType::kString: return "string";
    case FlagType::kLog: return "log level";
    case FlagType::kUnknown: return "unknown";
  }
  return "unknown";
}

bool IsValidFlagName(std::string_view name) {
  if (name.empty() || name.size() > 256) return false;
  return std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
}

ValueCheck NormalizeValue(FlagType type, std::string_view value) {
  ValueCheck check;
  switch (type) {
    case FlagType::kBool: {
      const std::string lowered = Lower(value);
      if (lowered == "true" || lowered == "1") {
        check.valid = true;
        check.normalized = "True";
      } else if (lowered == "false" || lowered == "0") {
        check.valid = true;
        check.normalized = "False";
      } else {
        check.message = "expected True or False";
      }
      if (check.valid && check.normalized != value) check.message = "normalized to " + check.normalized;
      return check;
    }
    case FlagType::kInt:
    case FlagType::kLog: {
      std::string text(value);
      if (text.empty()) {
        check.message = "expected a whole number";
        return check;
      }
      errno = 0;
      char* end = nullptr;
      const long long parsed = std::strtoll(text.c_str(), &end, 10);
      if (end == text.c_str() || *end != '\0' || std::isspace(static_cast<unsigned char>(text.front()))) {
        check.message = "expected a whole number";
        return check;
      }
      if (errno == ERANGE || parsed < INT32_MIN || parsed > INT32_MAX) {
        check.message = "out of the 32-bit integer range";
        return check;
      }
      if (type == FlagType::kLog && parsed < 0) {
        check.message = "log levels cannot be negative";
        return check;
      }
      check.valid = true;
      check.normalized = std::to_string(parsed);
      if (check.normalized != value) check.message = "normalized to " + check.normalized;
      return check;
    }
    case FlagType::kString:
    case FlagType::kUnknown:
      for (const char c : value) {
        if (static_cast<unsigned char>(c) < 0x20) {
          check.message = "control characters are not allowed";
          return check;
        }
      }
      check.valid = true;
      check.normalized.assign(value);
      return check;
  }
  return check;
}

void FlagSet::Set(std::string name, std::string value) {
  const auto existing = index_.find(name);
  if (existing != index_.end()) {
    flags_[existing->second].value = std::move(value);
    return;
  }
  index_.emplace(name, flags_.size());
  flags_.push_back({std::move(name), std::move(value)});
}

bool FlagSet::Remove(std::string_view name) {
  const auto existing = index_.find(std::string(name));
  if (existing == index_.end()) return false;
  flags_.erase(flags_.begin() + static_cast<std::ptrdiff_t>(existing->second));
  index_.clear();
  for (std::size_t i = 0; i < flags_.size(); ++i) index_.emplace(flags_[i].name, i);
  return true;
}

const std::string* FlagSet::Find(std::string_view name) const {
  const auto existing = index_.find(std::string(name));
  return existing == index_.end() ? nullptr : &flags_[existing->second].value;
}

void FlagSet::Clear() {
  flags_.clear();
  index_.clear();
}

ParseResult ParseFlagsJson(std::string_view text) {
  ParseResult result;
  JsonValue root;
  JsonReader reader(text);
  if (!reader.Parse(&root)) {
    result.error = reader.error();
    result.error_offset = reader.error_offset();
    return result;
  }
  if (root.kind != JsonValue::Kind::kObject) {
    result.error = "fflags file must contain a JSON object";
    return result;
  }
  for (const auto& [key, value] : root.object) {
    if (key.empty()) {
      result.error = "fflags file contains an invalid key";
      result.error_offset = value.offset;
      return result;
    }
    std::string normalized;
    switch (value.kind) {
      case JsonValue::Kind::kString:
        normalized = value.text;
        break;
      case JsonValue::Kind::kBool:
        normalized = value.boolean ? "True" : "False";
        break;
      case JsonValue::Kind::kNumber:
        if (!value.integral) {
          result.error = "fflags file values must be strings, booleans, or integers (\"" + key + "\")";
          result.error_offset = value.offset;
          return result;
        }
        normalized = value.text;
        break;
      default:
        result.error = "fflags file values must be strings, booleans, or integers (\"" + key + "\")";
        result.error_offset = value.offset;
        return result;
    }
    if (result.flags.Contains(key)) result.warnings.push_back("duplicate key \"" + key + "\": the last value wins");
    if (!IsValidFlagName(key)) {
      result.warnings.push_back("\"" + key + "\" is not a valid Fast Flag name");
    } else if (ClassifyFlagName(key) == FlagType::kUnknown) {
      result.warnings.push_back("\"" + key + "\" has no known Fast Flag prefix");
    }
    result.flags.Set(key, std::move(normalized));
  }
  result.ok = true;
  return result;
}

std::string SerializeFlagsJson(const FlagSet& flags) {
  if (flags.empty()) return "{}\n";
  std::string out = "{\n";
  for (std::size_t i = 0; i < flags.flags().size(); ++i) {
    const Flag& flag = flags.flags()[i];
    out.append("  ");
    AppendJsonString(flag.name, &out);
    out.append(": ");
    AppendJsonString(flag.value, &out);
    if (i + 1 < flags.flags().size()) out.push_back(',');
    out.push_back('\n');
  }
  out.append("}\n");
  return out;
}

ParseResult LoadFlagsFile(const std::filesystem::path& path) {
  ParseResult result;
  std::string contents;
  bool missing = false;
  if (!ReadWholeFile(path, &contents, &missing, &result.error)) return result;
  if (missing) {
    result.ok = true;
    return result;
  }
  return ParseFlagsJson(contents);
}

bool SaveFlagsFile(const std::filesystem::path& path, const FlagSet& flags, std::string* error) {
  std::error_code ec;
  if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
  std::filesystem::path temporary = path;
  temporary += ".tmp";
  const std::string text = SerializeFlagsJson(flags);
#if defined(_WIN32)
  {
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      if (error != nullptr) *error = "cannot create " + temporary.u8string();
      return false;
    }
    DWORD written = 0;
    const bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
                    written == text.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) {
      if (error != nullptr) *error = "cannot write " + temporary.u8string();
      std::filesystem::remove(temporary, ec);
      return false;
    }
  }
#else
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) {
      if (error != nullptr) *error = "cannot create " + temporary.u8string();
      return false;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.flush();
    if (!out) {
      if (error != nullptr) *error = "cannot write " + temporary.u8string();
      return false;
    }
  }
#endif
  if (std::filesystem::exists(path, ec)) {
    std::filesystem::path backup = path;
    backup += ".bak";
    std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, ec);
  }
#if defined(_WIN32)
  if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    if (error != nullptr) *error = "cannot replace " + path.u8string();
    std::filesystem::remove(temporary, ec);
    return false;
  }
#else
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    if (error != nullptr) *error = "cannot replace " + path.u8string() + ": " + ec.message();
    std::filesystem::remove(temporary, ec);
    return false;
  }
#endif
  return true;
}

Allowlist Allowlist::Builtin() {
  Allowlist list;
  for (const BuiltinEntry& entry : kBuiltinAllowlist) list.entries_.push_back({entry.name, entry.group});
  list.source_ = kAllowlistSource;
  list.source_updated_ = kAllowlistUpdated;
  return list;
}

bool Allowlist::Parse(std::string_view json, Allowlist* out, std::string* error) {
  JsonValue root;
  JsonReader reader(json);
  if (!reader.Parse(&root)) {
    if (error != nullptr) *error = reader.error();
    return false;
  }
  if (root.kind != JsonValue::Kind::kObject) {
    if (error != nullptr) *error = "allowlist must be a JSON object";
    return false;
  }
  Allowlist list;
  bool has_groups = false;
  for (const auto& [key, value] : root.object) {
    if (key == "source" && value.kind == JsonValue::Kind::kString) list.source_ = value.text;
    if (key == "source_post_updated" && value.kind == JsonValue::Kind::kString) list.source_updated_ = value.text;
    if (key != "groups") continue;
    if (value.kind != JsonValue::Kind::kObject) {
      if (error != nullptr) *error = "\"groups\" must be an object";
      return false;
    }
    has_groups = true;
    for (const auto& [group, names] : value.object) {
      if (names.kind != JsonValue::Kind::kArray) {
        if (error != nullptr) *error = "allowlist group \"" + group + "\" must be an array";
        return false;
      }
      for (const JsonValue& name : names.array) {
        if (name.kind != JsonValue::Kind::kString || !IsValidFlagName(name.text)) {
          if (error != nullptr) *error = "allowlist group \"" + group + "\" contains an invalid flag name";
          return false;
        }
        if (!list.Contains(name.text)) list.entries_.push_back({name.text, group});
      }
    }
  }
  if (!has_groups) {
    if (error != nullptr) *error = "allowlist has no \"groups\"";
    return false;
  }
  *out = std::move(list);
  return true;
}

bool Allowlist::LoadFile(const std::filesystem::path& path, Allowlist* out, std::string* error) {
  std::string contents;
  bool missing = false;
  std::string read_error;
  if (!ReadWholeFile(path, &contents, &missing, &read_error) || missing) {
    if (error != nullptr) *error = missing ? "allowlist file not found" : read_error;
    return false;
  }
  return Parse(contents, out, error);
}

bool Allowlist::Contains(std::string_view name) const { return Find(name) != nullptr; }

const AllowlistEntry* Allowlist::Find(std::string_view name) const {
  for (const AllowlistEntry& entry : entries_) {
    if (entry.name == name) return &entry;
  }
  return nullptr;
}

const char* FlagStatusName(FlagStatus status) {
  switch (status) {
    case FlagStatus::kApplied: return "applied";
    case FlagStatus::kNotAllowlisted: return "not on Roblox's allowlist";
    case FlagStatus::kInvalidValue: return "invalid value";
    case FlagStatus::kOverriddenByRuntime: return "managed by Elixir";
  }
  return "unknown";
}

Resolution ResolveOverrides(const FlagSet& runtime_mandatory, const FlagSet& runtime_defaults,
                            const FlagSet& user, const Allowlist& allowlist) {
  Resolution resolution;
  for (const Flag& flag : runtime_defaults.flags()) resolution.effective.Set(flag.name, flag.value);
  for (const Flag& flag : user.flags()) {
    FlagReport report{flag.name, flag.value, FlagStatus::kApplied, {}};
    if (const std::string* owned = runtime_mandatory.Find(flag.name)) {
      report.status = FlagStatus::kOverriddenByRuntime;
      report.detail = "Elixir's runtime policy sets this flag to \"" + *owned + "\"";
    } else if (!allowlist.Contains(flag.name)) {
      report.status = FlagStatus::kNotAllowlisted;
      report.detail = "Roblox ignores locally set Fast Flags that are not on its allowlist, so Elixir does not apply it";
    } else {
      const ValueCheck check = NormalizeValue(ClassifyFlagName(flag.name), flag.value);
      if (!check.valid) {
        report.status = FlagStatus::kInvalidValue;
        report.detail = check.message;
      } else {
        resolution.effective.Set(flag.name, check.normalized);
        ++resolution.applied_count;
      }
    }
    resolution.user.push_back(std::move(report));
  }
  for (const Flag& flag : runtime_mandatory.flags()) resolution.effective.Set(flag.name, flag.value);
  return resolution;
}

int ActivePresetOption(const Preset& preset, const FlagSet& user) {
  for (std::size_t i = 0; i < preset.options.size(); ++i) {
    const PresetOption& option = preset.options[i];
    bool matches = true;
    for (const Flag& flag : option.set) {
      const std::string* value = user.Find(flag.name);
      if (value == nullptr) {
        matches = false;
        break;
      }
      const ValueCheck check = NormalizeValue(ClassifyFlagName(flag.name), *value);
      if (!check.valid || check.normalized != flag.value) {
        matches = false;
        break;
      }
    }
    for (const std::string& name : option.remove) {
      if (user.Contains(name)) matches = false;
    }
    if (matches) return static_cast<int>(i);
  }
  return -1;
}

void ApplyPresetOption(const PresetOption& option, FlagSet* user) {
  for (const std::string& name : option.remove) user->Remove(name);
  for (const Flag& flag : option.set) user->Set(flag.name, flag.value);
}

}
