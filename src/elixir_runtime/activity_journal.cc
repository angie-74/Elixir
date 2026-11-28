// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/activity_journal.h"

#include <windows.h>

#include <ctime>
#include <mutex>
#include <random>
#include <system_error>
#include <thread>
#include <utility>

#include "elixir_runtime/host_log.h"

namespace elixir::runtime {

namespace mr = mocktail::runtime;

namespace {

bool IsHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

std::string Lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

std::string NewSessionId() {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::random_device device;
  std::string id;
  for (int word = 0; word < 2; ++word) {
    unsigned value = device();
    for (int i = 0; i < 8; ++i) {
      id.push_back(kDigits[value & 0xF]);
      value >>= 4;
    }
  }
  return id;
}

}

bool IsServerId(std::string_view text) {
  if (text.size() != 36) return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
    if (dash ? text[i] != '-' : !IsHex(text[i])) return false;
  }
  return true;
}

bool ParseJoiningGameLine(std::string_view line, std::string* job_id, std::int64_t* place_id) {
  constexpr std::string_view kMarker = "! Joining game '";
  constexpr std::string_view kPlace = "' place ";
  const std::size_t at = line.find(kMarker);
  if (at == std::string_view::npos) return false;
  std::string_view rest = line.substr(at + kMarker.size());
  if (rest.size() < 36 + kPlace.size() || rest.substr(36, kPlace.size()) != kPlace) return false;
  const std::string_view job = rest.substr(0, 36);
  if (!IsServerId(job)) return false;
  rest.remove_prefix(36 + kPlace.size());
  std::int64_t place = 0;
  std::size_t digits = 0;
  while (digits < rest.size() && digits < 18 && rest[digits] >= '0' && rest[digits] <= '9') {
    place = place * 10 + (rest[digits] - '0');
    ++digits;
  }
  if (digits == 0 || place <= 0) return false;
  *job_id = Lower(job);
  *place_id = place;
  return true;
}

class ActivityJournal::Impl {
 public:
  explicit Impl(ActivityJournalOptions options) : options_(std::move(options)) {
    file_ = options_.folder / kActivityJournalFileName;
    stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stop_ != nullptr) worker_ = std::thread([this] { Run(); });
    AddGuestLineObserver(&Impl::GuestLine, this);
  }

  ~Impl() {
    Close();
    if (stop_ != nullptr) CloseHandle(stop_);
  }

  void Close() {
    RemoveGuestLineObserver(&Impl::GuestLine, this);
    if (worker_.joinable()) {
      SetEvent(stop_);
      worker_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (open_) EndLocked();
  }

  static void Notify(void* context, mr::RobloxExperiencePresencePhase phase,
                     const mr::RobloxExperienceLaunchRequest* request) {
    auto& self = *static_cast<Impl*>(context);
    try {
      self.OnPhase(phase, request);
    } catch (...) {
    }
    const mr::RobloxExperiencePresenceObserver& next = self.options_.next;
    if (next.valid()) next.notify(next.context, phase, request);
  }

  static void GuestLine(void* context, const char* tag, std::string_view message) {
    static_cast<Impl*>(context)->OnGuestLine(tag != nullptr ? tag : "", message);
  }

  void OnGuestLine(std::string_view tag, std::string_view message) {
    if (tag != "Roblox" || message.find("! Joining game '") == std::string_view::npos) return;
    try {
      std::string job;
      std::int64_t place = 0;
      if (!ParseJoiningGameLine(message, &job, &place)) return;
      std::lock_guard<std::mutex> lock(mutex_);
      if (!open_) {
        seen_ = Server{place, job, {}, true};
        return;
      }
      if (session_.job.empty() && (session_.place == place || session_.place == 0)) {
        session_.place = place;
        session_.job = job;
        Write("{\"v\":1,\"e\":\"server\",\"s\":\"" + session_.id + "\",\"t\":" + std::to_string(Now()) +
              ",\"place\":" + std::to_string(place) + ",\"job\":\"" + job + "\"}");
        return;
      }
      if (session_.job == job) return;
      EndLocked();
      asked_ = Server{};
      seen_ = Server{place, job, {}, true};
      StartLocked();
    } catch (...) {
    }
  }

 private:
  struct Server {
    std::int64_t place = 0;
    std::string job;
    std::string kind;
    bool set = false;
  };
  struct Session {
    std::string id;
    std::int64_t place = 0;
    std::string job;
  };

  void OnPhase(mr::RobloxExperiencePresencePhase phase, const mr::RobloxExperienceLaunchRequest* request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (request != nullptr) {
      asked_.place = request->place_id;
      asked_.job = IsServerId(request->game_instance_id) ? Lower(request->game_instance_id) : std::string();
      asked_.kind = !request->reserved_server_access_code.empty()                     ? "reserved"
                    : !request->access_code.empty() || !request->link_code.empty() ? "private"
                    : request->user_id != 0                                         ? "unknown"
                                                                                    : "public";
      asked_.set = true;
    }
    switch (phase) {
      case mr::RobloxExperiencePresencePhase::kBrowsing:
        if (open_) EndLocked();
        asked_ = Server{};
        seen_ = Server{};
        break;
      case mr::RobloxExperiencePresencePhase::kJoining:
        break;
      case mr::RobloxExperiencePresencePhase::kPlaying:
        if (!open_) StartLocked();
        break;
    }
  }

  void StartLocked() {
    session_ = Session{};
    session_.id = NewSessionId();
    session_.place = seen_.set ? seen_.place : asked_.place;
    session_.job = seen_.set ? seen_.job : asked_.job;
    const std::string kind = asked_.set && !asked_.kind.empty() ? asked_.kind : std::string("unknown");
    const long long user = options_.user ? options_.user() : 0;
    seen_ = Server{};
    open_ = true;
    std::string line = "{\"v\":1,\"e\":\"start\",\"s\":\"" + session_.id + "\",\"t\":" + std::to_string(Now()) +
                       ",\"place\":" + std::to_string(session_.place) + ",\"kind\":\"" + kind + "\"";
    if (!session_.job.empty()) line += ",\"job\":\"" + session_.job + "\"";
    if (user > 0) line += ",\"user\":" + std::to_string(user);
    Write(line + "}");
  }

  void EndLocked() {
    open_ = false;
    Write("{\"v\":1,\"e\":\"end\",\"s\":\"" + session_.id + "\",\"t\":" + std::to_string(Now()) + "}");
    session_ = Session{};
  }

  void Run() {
    while (WaitForSingleObject(stop_, options_.alive_every_ms) == WAIT_TIMEOUT) {
      try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!open_) continue;
        Write("{\"v\":1,\"e\":\"alive\",\"s\":\"" + session_.id + "\",\"t\":" + std::to_string(Now()) + "}", true);
      } catch (...) {
      }
    }
  }

  std::int64_t Now() const {
    return options_.now ? options_.now() : static_cast<std::int64_t>(std::time(nullptr));
  }

  void Write(std::string line, bool optional = false) {
    if (options_.folder.empty()) return;
    std::error_code ignored;
    std::filesystem::create_directories(options_.folder, ignored);
    const HANDLE file = CreateFileW(file_.c_str(), FILE_APPEND_DATA,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    const bool quiet = optional && GetFileSizeEx(file, &size) &&
                       static_cast<std::uint64_t>(size.QuadPart) > kActivityJournalQuietBytes;
    if (!quiet) {
      line.push_back('\n');
      DWORD written = 0;
      WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    }
    CloseHandle(file);
  }

  ActivityJournalOptions options_;
  std::filesystem::path file_;
  HANDLE stop_ = nullptr;
  std::thread worker_;
  std::mutex mutex_;
  Server asked_;
  Server seen_;
  Session session_;
  bool open_ = false;
};

ActivityJournal::ActivityJournal(ActivityJournalOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

ActivityJournal::~ActivityJournal() = default;

mr::RobloxExperiencePresenceObserver ActivityJournal::observer() {
  return mr::RobloxExperiencePresenceObserver{impl_.get(), &Impl::Notify};
}

void ActivityJournal::OnGuestLine(std::string_view tag, std::string_view message) {
  impl_->OnGuestLine(tag, message);
}

void ActivityJournal::Close() { impl_->Close(); }

}
