// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_ACTIVITY_JOURNAL_H_
#define ELIXIR_RUNTIME_ACTIVITY_JOURNAL_H_

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "runtime/roblox_experience_presence.h"

namespace elixir::runtime {

inline constexpr wchar_t kActivityJournalFileName[] = L"journal.jsonl";
inline constexpr std::uint64_t kActivityJournalQuietBytes = 8ull * 1024 * 1024;

struct ActivityJournalOptions {
  std::filesystem::path folder;
  mocktail::runtime::RobloxExperiencePresenceObserver next;
  std::function<long long()> user;
  std::function<std::int64_t()> now;
  unsigned alive_every_ms = 60000;
};

bool IsServerId(std::string_view text);
bool ParseJoiningGameLine(std::string_view line, std::string* job_id, std::int64_t* place_id);

class ActivityJournal final {
 public:
  explicit ActivityJournal(ActivityJournalOptions options);
  ~ActivityJournal();
  ActivityJournal(const ActivityJournal&) = delete;
  ActivityJournal& operator=(const ActivityJournal&) = delete;

  mocktail::runtime::RobloxExperiencePresenceObserver observer();
  void OnGuestLine(std::string_view tag, std::string_view message);
  void Close();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}

#endif
