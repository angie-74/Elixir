// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_APP_RESTART_H_
#define ELIXIR_RUNTIME_APP_RESTART_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace jnivm {
class VM;
}

namespace elixir::runtime {

enum class RestartCause { kNone, kAccountSwitched, kRequestedByRoblox };

class AppRestart {
 public:
  static constexpr std::uint64_t kQuietStartMs = 15000;

  struct Options {
    std::function<std::uint64_t()> clock_ms;
    std::function<void(std::string_view)> log;
  };

  static std::shared_ptr<AppRestart> Install(jnivm::VM& vm, Options options);

  bool Request(RestartCause cause);
  void NoteAccountEvent(std::string_view type);
  const std::atomic<bool>& requested() const { return requested_; }
  RestartCause cause() const { return cause_.load(); }
  void Detach();

 private:
  explicit AppRestart(Options options);

  Options options_;
  std::uint64_t started_ms_ = 0;
  std::atomic<bool> requested_{false};
  std::atomic<RestartCause> cause_{RestartCause::kNone};
  std::atomic<jnivm::VM*> vm_{nullptr};
};

const char* RestartReason(RestartCause cause);

}

#endif
