// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/fflags_file.h"

#include "core/json.h"

namespace elixir::launcher {

std::string StripJsonComments(std::string_view text, bool* unterminated_comment) {
  std::string out(text);
  bool in_string = false;
  if (unterminated_comment != nullptr) *unterminated_comment = false;
  for (std::size_t i = 0; i < out.size(); ++i) {
    const char c = out[i];
    if (in_string) {
      if (c == '\\') {
        ++i;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
    } else if (c == '/' && i + 1 < out.size() && out[i + 1] == '/') {
      while (i < out.size() && out[i] != '\n') out[i++] = ' ';
    } else if (c == '/' && i + 1 < out.size() && out[i + 1] == '*') {
      out[i] = ' ';
      out[i + 1] = ' ';
      i += 2;
      bool closed = false;
      while (i < out.size()) {
        if (out[i] == '*' && i + 1 < out.size() && out[i + 1] == '/') {
          out[i] = ' ';
          out[i + 1] = ' ';
          ++i;
          closed = true;
          break;
        }
        if (out[i] != '\n') out[i] = ' ';
        ++i;
      }
      if (!closed && unterminated_comment != nullptr) *unterminated_comment = true;
    }
  }
  return out;
}

FflagsCheck CheckFflagsText(std::string_view text) {
  FflagsCheck check;
  bool unterminated = false;
  const std::string stripped = StripJsonComments(text, &unterminated);
  if (unterminated) {
    check.error = "unterminated /* comment";
    return check;
  }
  std::string error;
  const auto json = Json::Parse(stripped, &error);
  if (!json) {
    check.error = error;
    return check;
  }
  if (!json->is_object()) {
    check.error = "the file must contain one JSON object ({\"FlagName\": value, ...})";
    return check;
  }
  for (const Json::Member& member : json->members()) {
    if (member.first.empty()) {
      check.error = "a flag has an empty name";
      return check;
    }
    const Json& value = member.second;
    const bool allowed = value.is_string() || value.is_bool() || value.is_integer();
    if (!allowed) {
      const char* kind = value.is_null()     ? "null"
                         : value.is_number() ? "a non-integer number"
                         : value.is_array()  ? "an array"
                                             : "an object";
      check.error = "\"" + member.first + "\" is " + kind + "; values must be strings, booleans or integers";
      return check;
    }
  }
  check.count = json->members().size();
  check.ok = true;
  return check;
}

bool ParseRuntimeFlagsReport(std::string_view text, RuntimeFlagsReport* report, std::string* error) {
  const auto fail = [&](std::string message) {
    if (error != nullptr) *error = std::move(message);
    return false;
  };
  std::string parse_error;
  const auto json = Json::Parse(text, &parse_error);
  if (!json) return fail("not JSON: " + parse_error);
  if (!json->is_object()) return fail("the report is not a JSON object");
  if (json->GetInt("schema", 0) != 1) return fail("unsupported report schema (expected \"schema\": 1)");
  RuntimeFlagsReport parsed;
  const auto read = [&](const char* key, std::vector<std::pair<std::string, std::string>>* out) {
    const Json* section = json->Find(key);
    if (section == nullptr || !section->is_object()) return fail(std::string("\"") + key + "\" is missing or not an object");
    for (const Json::Member& member : section->members()) {
      if (member.first.empty() || !member.second.is_string()) {
        return fail(std::string("\"") + key + "\" must map flag names to string values");
      }
      out->emplace_back(member.first, member.second.string_ref());
    }
    return true;
  };
  if (!read("mandatory", &parsed.mandatory) || !read("defaults", &parsed.defaults)) return false;
  *report = std::move(parsed);
  return true;
}

}
