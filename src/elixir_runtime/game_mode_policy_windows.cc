// Copyright 2026 Mocktail Project Authors
// Licensed under the Apache License, Version 2.0
// Modified by Elixir.
#include "runtime/game_mode.h"

#if defined(_WIN32)

namespace mocktail {
namespace runtime {

bool ParseGameModePolicy(std::string_view value, GameModePolicy* policy) {
  if (policy == nullptr) {
    return false;
  }
  if (value == "auto") {
    *policy = GameModePolicy::kAuto;
    return true;
  }
  if (value == "on" || value == "1" || value == "true") {
    *policy = GameModePolicy::kOn;
    return true;
  }
  if (value == "off" || value == "0" || value == "false") {
    *policy = GameModePolicy::kOff;
    return true;
  }
  return false;
}

const char* GameModePolicyName(GameModePolicy policy) {
  switch (policy) {
    case GameModePolicy::kAuto:
      return "auto";
    case GameModePolicy::kOn:
      return "on";
    case GameModePolicy::kOff:
      return "off";
  }
  return "invalid";
}

}
}

#endif
