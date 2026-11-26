// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_JSON_H_
#define ELIXIR_LAUNCHER_CORE_JSON_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elixir::launcher {

class Json {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };
  using Member = std::pair<std::string, Json>;

  Json() = default;
  Json(std::nullptr_t) {}
  Json(bool value) : type_(Type::kBool), bool_(value) {}
  Json(int value) : Json(static_cast<std::int64_t>(value)) {}
  Json(std::int64_t value)
      : type_(Type::kNumber), number_(static_cast<double>(value)), integer_(value), is_integer_(true) {}
  Json(double value) : type_(Type::kNumber), number_(value) {}
  Json(const char* value) : type_(Type::kString), string_(value) {}
  Json(std::string value) : type_(Type::kString), string_(std::move(value)) {}
  Json(std::string_view value) : type_(Type::kString), string_(value) {}

  static Json MakeArray();
  static Json MakeObject();

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::kNull; }
  bool is_bool() const { return type_ == Type::kBool; }
  bool is_number() const { return type_ == Type::kNumber; }
  bool is_integer() const { return type_ == Type::kNumber && is_integer_; }
  bool is_string() const { return type_ == Type::kString; }
  bool is_array() const { return type_ == Type::kArray; }
  bool is_object() const { return type_ == Type::kObject; }

  bool AsBool(bool fallback) const { return is_bool() ? bool_ : fallback; }
  double AsDouble(double fallback) const { return is_number() ? number_ : fallback; }
  std::int64_t AsInt(std::int64_t fallback) const;
  std::string AsString(std::string_view fallback = {}) const {
    return is_string() ? string_ : std::string(fallback);
  }
  const std::string& string_ref() const { return string_; }

  const std::vector<Json>& items() const { return array_; }
  void Push(Json value);

  const std::vector<Member>& members() const { return object_; }
  const Json* Find(std::string_view key) const;
  Json& Set(std::string_view key, Json value);

  std::string GetString(std::string_view key, std::string_view fallback = {}) const;
  bool GetBool(std::string_view key, bool fallback) const;
  double GetDouble(std::string_view key, double fallback) const;
  std::int64_t GetInt(std::string_view key, std::int64_t fallback) const;

  std::string Dump(int indent = 2) const;

  static std::optional<Json> Parse(std::string_view text, std::string* error);

 private:
  void DumpTo(std::string* out, int indent, int depth) const;

  Type type_ = Type::kNull;
  bool bool_ = false;
  double number_ = 0.0;
  std::int64_t integer_ = 0;
  bool is_integer_ = false;
  std::string string_;
  std::vector<Json> array_;
  std::vector<Member> object_;

  friend class JsonParser;
};

void AppendJsonString(std::string* out, std::string_view value);
std::string JsonQuote(std::string_view value);

bool IsValidUtf8(std::string_view text);

}

#endif
