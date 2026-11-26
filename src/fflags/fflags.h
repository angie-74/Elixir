// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_FFLAGS_FFLAGS_H_
#define ELIXIR_FFLAGS_FFLAGS_H_

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elixir::fflags {

enum class FlagType { kBool, kInt, kString, kLog, kUnknown };

FlagType ClassifyFlagName(std::string_view name);
const char* FlagTypeName(FlagType type);
bool IsValidFlagName(std::string_view name);

struct ValueCheck {
  bool valid = false;
  std::string normalized;
  std::string message;
};

ValueCheck NormalizeValue(FlagType type, std::string_view value);

struct Flag {
  std::string name;
  std::string value;
};

class FlagSet {
 public:
  void Set(std::string name, std::string value);
  bool Remove(std::string_view name);
  const std::string* Find(std::string_view name) const;
  bool Contains(std::string_view name) const { return Find(name) != nullptr; }
  const std::vector<Flag>& flags() const { return flags_; }
  std::size_t size() const { return flags_.size(); }
  bool empty() const { return flags_.empty(); }
  void Clear();

 private:
  std::vector<Flag> flags_;
  std::unordered_map<std::string, std::size_t> index_;
};

struct ParseResult {
  bool ok = false;
  FlagSet flags;
  std::string error;
  std::size_t error_offset = 0;
  std::vector<std::string> warnings;
};

ParseResult ParseFlagsJson(std::string_view text);
std::string SerializeFlagsJson(const FlagSet& flags);

ParseResult LoadFlagsFile(const std::filesystem::path& path);
bool SaveFlagsFile(const std::filesystem::path& path, const FlagSet& flags, std::string* error);

struct AllowlistEntry {
  std::string name;
  std::string group;
};

class Allowlist {
 public:
  static Allowlist Builtin();
  static bool LoadFile(const std::filesystem::path& path, Allowlist* out, std::string* error);
  static bool Parse(std::string_view json, Allowlist* out, std::string* error);

  bool Contains(std::string_view name) const;
  const AllowlistEntry* Find(std::string_view name) const;
  const std::vector<AllowlistEntry>& entries() const { return entries_; }
  const std::string& source() const { return source_; }
  const std::string& source_updated() const { return source_updated_; }

 private:
  std::vector<AllowlistEntry> entries_;
  std::string source_;
  std::string source_updated_;
};

enum class FlagStatus {
  kApplied,
  kNotAllowlisted,
  kInvalidValue,
  kOverriddenByRuntime
};
const char* FlagStatusName(FlagStatus status);

struct FlagReport {
  std::string name;
  std::string value;
  FlagStatus status = FlagStatus::kApplied;
  std::string detail;
};

struct Resolution {
  FlagSet effective;
  std::vector<FlagReport> user;
  std::size_t applied_count = 0;
};

Resolution ResolveOverrides(const FlagSet& runtime_mandatory, const FlagSet& runtime_defaults,
                            const FlagSet& user, const Allowlist& allowlist);

struct PresetOption {
  std::string label;
  std::vector<Flag> set;
  std::vector<std::string> remove;
};

enum class PresetControl {
  kToggle,
  kSlider,
  kToggleSlider,
  kChoice,
};

struct Preset {
  std::string id;
  std::string group;
  std::string title;
  std::string description;
  std::vector<PresetOption> options;
  PresetControl control = PresetControl::kToggle;
  std::string low_label;
  std::string high_label;
  std::string note;
  bool show_value = false;
};

const std::vector<Preset>& BuiltinPresets();
int ActivePresetOption(const Preset& preset, const FlagSet& user);
void ApplyPresetOption(const PresetOption& option, FlagSet* user);

}

#endif
