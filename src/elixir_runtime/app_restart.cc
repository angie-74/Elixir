// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/app_restart.h"

#include <windows.h>

#include <string>
#include <utility>

#include "jnivm/jnivm.h"

namespace elixir::runtime {
namespace {

void OnRestart(void* context, jnivm::RobloxAppRestartReason reason) {
  auto* restart = static_cast<AppRestart*>(context);
  if (restart == nullptr) return;
  static_cast<void>(restart->Request(reason == jnivm::RobloxAppRestartReason::kAccountSwitched
                                         ? RestartCause::kAccountSwitched
                                         : RestartCause::kRequestedByRoblox));
}

void OnAccountEvent(void* context, const char* type) {
  auto* restart = static_cast<AppRestart*>(context);
  if (restart != nullptr && type != nullptr) restart->NoteAccountEvent(type);
}

}

AppRestart::AppRestart(Options options) : options_(std::move(options)) {
  if (!options_.clock_ms) options_.clock_ms = [] { return static_cast<std::uint64_t>(GetTickCount64()); };
  started_ms_ = options_.clock_ms();
}

std::shared_ptr<AppRestart> AppRestart::Install(jnivm::VM& vm, Options options) {
  std::shared_ptr<AppRestart> restart(new AppRestart(std::move(options)));
  restart->vm_.store(&vm);
  vm.SetRobloxAppRestartSink(restart, jnivm::RobloxAppRestartCallbacks{&OnRestart, &OnAccountEvent});
  return restart;
}

bool AppRestart::Request(RestartCause cause) {
  if (cause == RestartCause::kNone) return false;
  const auto log = [this](const std::string& line) {
    if (!options_.log) return;
    try {
      options_.log(line);
    } catch (...) {
    }
  };
  if (cause == RestartCause::kRequestedByRoblox) {
    const std::uint64_t age = options_.clock_ms() - started_ms_;
    if (age < kQuietStartMs) {
      log("restart: Roblox asked to be started again " + std::to_string(age) +
          " ms after it started; not restarting this early");
      return false;
    }
  }
  RestartCause none = RestartCause::kNone;
  if (!cause_.compare_exchange_strong(none, cause)) return true;
  requested_.store(true);
  log(std::string("restart: ") + RestartReason(cause));
  return true;
}

void AppRestart::NoteAccountEvent(std::string_view type) {
  if (!options_.log) return;
  try {
    options_.log("account: Roblox reported " + std::string(type));
  } catch (...) {
  }
}

void AppRestart::Detach() {
  jnivm::VM* vm = vm_.exchange(nullptr);
  if (vm != nullptr) vm->ClearRobloxAppRestartSink();
}

const char* RestartReason(RestartCause cause) {
  switch (cause) {
    case RestartCause::kAccountSwitched:
      return "Roblox switched accounts; it starts again with the new account";
    case RestartCause::kRequestedByRoblox:
      return "Roblox asked to be started again";
    case RestartCause::kNone:
      break;
  }
  return "no restart";
}

}
