// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_FFLAGS_EDITOR_H_
#define ELIXIR_FFLAGS_EDITOR_H_

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "fflags/fflags.h"

namespace elixir::fflags {

struct EditorRow {
  std::string name;
  std::string value;
  FlagType type = FlagType::kUnknown;
  FlagStatus status = FlagStatus::kApplied;
  std::string detail;
  std::string group;
};

enum class RowFilter { kAll, kApplied, kNotApplied };

struct EditResult {
  bool ok = false;
  FlagStatus status = FlagStatus::kApplied;
  std::string message;
};

enum class ImportMode {
  kMerge,
  kReplace,
};

struct ImportSummary {
  bool ok = false;
  std::string error;
  std::size_t added = 0;
  std::size_t changed = 0;
  std::size_t unchanged = 0;
  std::size_t removed = 0;
  std::vector<std::string> warnings;
};

std::string DescribeTextPosition(std::string_view text, std::size_t offset);

class FlagEditor {
 public:
  explicit FlagEditor(Allowlist allowlist = Allowlist::Builtin());

  void SetRuntimeFlags(FlagSet mandatory, FlagSet defaults);
  const FlagSet& runtime_mandatory() const { return runtime_mandatory_; }
  const FlagSet& runtime_defaults() const { return runtime_defaults_; }

  void SetAllowlist(Allowlist allowlist);
  const Allowlist& allowlist() const { return allowlist_; }

  bool Load(const std::filesystem::path& path, std::string* error);
  void StartOver(const std::filesystem::path& path);
  bool Save(std::string* error);
  bool SaveAs(const std::filesystem::path& path, std::string* error);
  const std::filesystem::path& path() const { return path_; }
  bool dirty() const;
  const FlagSet& flags() const { return flags_; }
  const std::vector<std::string>& load_warnings() const { return load_warnings_; }

  EditResult SetFlag(std::string_view name, std::string_view value);
  EditResult RenameFlag(std::string_view from, std::string_view to);
  std::size_t RemoveFlags(const std::vector<std::string>& names);
  bool RemoveFlag(std::string_view name) { return RemoveFlags({std::string(name)}) == 1; }
  std::size_t RemoveNotApplied();
  bool ClearAll();
  EditResult ApplyPreset(std::string_view preset_id, int option);
  ImportSummary Import(std::string_view json, ImportMode mode);
  std::string Export(bool applied_only) const;

  bool CanUndo() const { return position_ > 0; }
  bool CanRedo() const { return position_ + 1 < history_.size(); }
  bool Undo();
  bool Redo();

  Resolution Resolve() const;
  std::vector<EditorRow> Rows(std::string_view query, RowFilter filter) const;
  std::vector<AllowlistEntry> Suggestions(std::string_view query) const;
  int ActiveOption(std::string_view preset_id) const;

  static constexpr std::size_t kMaxUndoSteps = 200;

 private:
  void Commit(FlagSet next);
  FlagStatus StatusOf(std::string_view name) const;

  Allowlist allowlist_;
  FlagSet runtime_mandatory_;
  FlagSet runtime_defaults_;
  std::filesystem::path path_;
  FlagSet flags_;
  FlagSet saved_;
  std::vector<std::string> load_warnings_;
  std::vector<FlagSet> history_;
  std::size_t position_ = 0;
  bool unsaved_document_ = false;
};

}

#endif
