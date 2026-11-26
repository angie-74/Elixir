// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "fflags/editor.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace elixir::fflags {
namespace {

bool SameFlags(const FlagSet& a, const FlagSet& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a.flags()[i].name != b.flags()[i].name || a.flags()[i].value != b.flags()[i].value) return false;
  }
  return true;
}

std::string_view Trim(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
  return text;
}

std::string Lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::vector<std::string> Words(std::string_view query) {
  std::vector<std::string> words;
  std::string current;
  for (char c : query) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) words.push_back(Lower(current));
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) words.push_back(Lower(current));
  return words;
}

bool MatchesAll(const std::vector<std::string>& words, std::string_view a, std::string_view b) {
  const std::string la = Lower(a);
  const std::string lb = Lower(b);
  for (const std::string& word : words) {
    if (la.find(word) == std::string::npos && lb.find(word) == std::string::npos) return false;
  }
  return true;
}

std::string StoredValue(std::string_view name, std::string_view value, ValueCheck* check_out) {
  ValueCheck check = NormalizeValue(ClassifyFlagName(name), value);
  std::string stored = check.valid ? check.normalized : std::string(value);
  if (check_out != nullptr) *check_out = std::move(check);
  return stored;
}

const Preset* FindPreset(std::string_view id) {
  for (const Preset& preset : BuiltinPresets()) {
    if (preset.id == id) return &preset;
  }
  return nullptr;
}

constexpr const char* kNameRule =
    "Fast Flag names use only letters, digits and underscores (1 to 256 characters)";

}

std::string DescribeTextPosition(std::string_view text, std::size_t offset) {
  offset = std::min(offset, text.size());
  std::size_t line = 1;
  std::size_t column = 1;
  for (std::size_t i = 0; i < offset; ++i) {
    if (text[i] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  return "line " + std::to_string(line) + ", column " + std::to_string(column);
}

FlagEditor::FlagEditor(Allowlist allowlist) : allowlist_(std::move(allowlist)) {
  history_.push_back(flags_);
}

void FlagEditor::SetRuntimeFlags(FlagSet mandatory, FlagSet defaults) {
  runtime_mandatory_ = std::move(mandatory);
  runtime_defaults_ = std::move(defaults);
}

void FlagEditor::SetAllowlist(Allowlist allowlist) { allowlist_ = std::move(allowlist); }

bool FlagEditor::Load(const std::filesystem::path& path, std::string* error) {
  std::error_code ec;
  std::string text;
  bool missing = false;
  if (std::filesystem::exists(path, ec)) {
    if (!std::filesystem::is_regular_file(path, ec)) {
      if (error != nullptr) *error = path.u8string() + " is not a regular file";
      return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      if (error != nullptr) *error = "cannot open " + path.u8string();
      return false;
    }
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (in.bad()) {
      if (error != nullptr) *error = "cannot read " + path.u8string();
      return false;
    }
  } else if (ec) {
    if (error != nullptr) *error = "cannot inspect " + path.u8string() + ": " + ec.message();
    return false;
  } else {
    missing = true;
  }
  ParseResult parsed;
  if (missing) {
    parsed.ok = true;
  } else {
    parsed = ParseFlagsJson(text);
  }
  if (!parsed.ok) {
    if (error != nullptr) {
      *error = path.u8string() + ": " + parsed.error + " (" + DescribeTextPosition(text, parsed.error_offset) + ")";
    }
    return false;
  }
  path_ = path;
  flags_ = std::move(parsed.flags);
  saved_ = flags_;
  load_warnings_ = std::move(parsed.warnings);
  history_.assign(1, flags_);
  position_ = 0;
  unsaved_document_ = false;
  return true;
}

void FlagEditor::StartOver(const std::filesystem::path& path) {
  path_ = path;
  flags_.Clear();
  saved_.Clear();
  load_warnings_.clear();
  history_.assign(1, flags_);
  position_ = 0;
  unsaved_document_ = true;
}

bool FlagEditor::Save(std::string* error) {
  if (path_.empty()) {
    if (error != nullptr) *error = "no file has been chosen for these flags";
    return false;
  }
  if (!SaveFlagsFile(path_, flags_, error)) return false;
  saved_ = flags_;
  unsaved_document_ = false;
  return true;
}

bool FlagEditor::SaveAs(const std::filesystem::path& path, std::string* error) {
  if (!SaveFlagsFile(path, flags_, error)) return false;
  path_ = path;
  saved_ = flags_;
  unsaved_document_ = false;
  return true;
}

bool FlagEditor::dirty() const { return unsaved_document_ || !SameFlags(flags_, saved_); }

void FlagEditor::Commit(FlagSet next) {
  if (SameFlags(next, flags_)) return;
  history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(position_) + 1, history_.end());
  history_.push_back(next);
  if (history_.size() > kMaxUndoSteps + 1) history_.erase(history_.begin());
  position_ = history_.size() - 1;
  flags_ = std::move(next);
}

bool FlagEditor::Undo() {
  if (!CanUndo()) return false;
  --position_;
  flags_ = history_[position_];
  return true;
}

bool FlagEditor::Redo() {
  if (!CanRedo()) return false;
  ++position_;
  flags_ = history_[position_];
  return true;
}

FlagStatus FlagEditor::StatusOf(std::string_view name) const {
  for (const FlagReport& report : Resolve().user) {
    if (report.name == name) return report.status;
  }
  return FlagStatus::kApplied;
}

EditResult FlagEditor::SetFlag(std::string_view raw_name, std::string_view value) {
  EditResult result;
  const std::string_view name = Trim(raw_name);
  if (!IsValidFlagName(name)) {
    result.message = kNameRule;
    return result;
  }
  ValueCheck check;
  std::string stored = StoredValue(name, value, &check);
  FlagSet next = flags_;
  next.Set(std::string(name), stored);
  Commit(std::move(next));
  result.ok = true;
  result.status = StatusOf(name);
  if (!check.valid) {
    result.message = check.message;
  } else if (stored != value) {
    result.message = "stored as \"" + stored + "\"";
  }
  return result;
}

EditResult FlagEditor::RenameFlag(std::string_view from, std::string_view raw_to) {
  EditResult result;
  const std::string_view to = Trim(raw_to);
  if (!flags_.Contains(from)) {
    result.message = "\"" + std::string(from) + "\" is not set";
    return result;
  }
  if (!IsValidFlagName(to)) {
    result.message = kNameRule;
    return result;
  }
  if (to != from && flags_.Contains(to)) {
    result.message = "\"" + std::string(to) + "\" is already set";
    return result;
  }
  FlagSet next;
  for (const Flag& flag : flags_.flags()) {
    if (flag.name == from) {
      next.Set(std::string(to), StoredValue(to, flag.value, nullptr));
    } else {
      next.Set(flag.name, flag.value);
    }
  }
  Commit(std::move(next));
  result.ok = true;
  result.status = StatusOf(to);
  return result;
}

std::size_t FlagEditor::RemoveFlags(const std::vector<std::string>& names) {
  const std::unordered_set<std::string> remove(names.begin(), names.end());
  FlagSet next;
  std::size_t removed = 0;
  for (const Flag& flag : flags_.flags()) {
    if (remove.count(flag.name) != 0) {
      ++removed;
    } else {
      next.Set(flag.name, flag.value);
    }
  }
  Commit(std::move(next));
  return removed;
}

std::size_t FlagEditor::RemoveNotApplied() {
  std::vector<std::string> names;
  for (const FlagReport& report : Resolve().user) {
    if (report.status != FlagStatus::kApplied) names.push_back(report.name);
  }
  return RemoveFlags(names);
}

bool FlagEditor::ClearAll() {
  if (flags_.empty()) return false;
  Commit(FlagSet{});
  return true;
}

EditResult FlagEditor::ApplyPreset(std::string_view preset_id, int option) {
  EditResult result;
  const Preset* preset = FindPreset(preset_id);
  if (preset == nullptr) {
    result.message = "unknown preset \"" + std::string(preset_id) + "\"";
    return result;
  }
  if (option < 0 || static_cast<std::size_t>(option) >= preset->options.size()) {
    result.message = "preset \"" + preset->title + "\" has no option " + std::to_string(option);
    return result;
  }
  FlagSet next = flags_;
  ApplyPresetOption(preset->options[static_cast<std::size_t>(option)], &next);
  Commit(std::move(next));
  result.ok = true;
  result.message = preset->title + ": " + preset->options[static_cast<std::size_t>(option)].label;
  return result;
}

ImportSummary FlagEditor::Import(std::string_view json, ImportMode mode) {
  ImportSummary summary;
  ParseResult parsed = ParseFlagsJson(json);
  if (!parsed.ok) {
    summary.error = parsed.error + " (" + DescribeTextPosition(json, parsed.error_offset) + ")";
    return summary;
  }
  summary.warnings = std::move(parsed.warnings);
  FlagSet next = mode == ImportMode::kMerge ? flags_ : FlagSet{};
  for (const Flag& flag : parsed.flags.flags()) {
    const std::string stored = StoredValue(flag.name, flag.value, nullptr);
    const std::string* existing = flags_.Find(flag.name);
    if (existing == nullptr) {
      ++summary.added;
    } else if (*existing == stored) {
      ++summary.unchanged;
    } else {
      ++summary.changed;
    }
    next.Set(flag.name, stored);
  }
  if (mode == ImportMode::kReplace) {
    for (const Flag& flag : flags_.flags()) {
      if (!parsed.flags.Contains(flag.name)) ++summary.removed;
    }
  }
  Commit(std::move(next));
  summary.ok = true;
  return summary;
}

std::string FlagEditor::Export(bool applied_only) const {
  if (!applied_only) return SerializeFlagsJson(flags_);
  FlagSet applied;
  for (const FlagReport& report : Resolve().user) {
    if (report.status == FlagStatus::kApplied) applied.Set(report.name, report.value);
  }
  return SerializeFlagsJson(applied);
}

Resolution FlagEditor::Resolve() const {
  return ResolveOverrides(runtime_mandatory_, runtime_defaults_, flags_, allowlist_);
}

std::vector<EditorRow> FlagEditor::Rows(std::string_view query, RowFilter filter) const {
  const std::vector<std::string> words = Words(query);
  std::vector<EditorRow> rows;
  for (FlagReport& report : Resolve().user) {
    const bool applied = report.status == FlagStatus::kApplied;
    if (filter == RowFilter::kApplied && !applied) continue;
    if (filter == RowFilter::kNotApplied && applied) continue;
    if (!MatchesAll(words, report.name, report.value)) continue;
    EditorRow row;
    row.type = ClassifyFlagName(report.name);
    if (const AllowlistEntry* entry = allowlist_.Find(report.name)) row.group = entry->group;
    row.name = std::move(report.name);
    row.value = std::move(report.value);
    row.status = report.status;
    row.detail = std::move(report.detail);
    rows.push_back(std::move(row));
  }
  return rows;
}

std::vector<AllowlistEntry> FlagEditor::Suggestions(std::string_view query) const {
  const std::vector<std::string> words = Words(query);
  std::vector<AllowlistEntry> out;
  for (const AllowlistEntry& entry : allowlist_.entries()) {
    if (flags_.Contains(entry.name)) continue;
    if (!MatchesAll(words, entry.name, entry.group)) continue;
    out.push_back(entry);
  }
  return out;
}

int FlagEditor::ActiveOption(std::string_view preset_id) const {
  const Preset* preset = FindPreset(preset_id);
  return preset == nullptr ? -2 : ActivePresetOption(*preset, flags_);
}

}
