// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_AUDIO_DEVICES_H_
#define ELIXIR_LAUNCHER_CORE_AUDIO_DEVICES_H_

#include <string>
#include <vector>

namespace elixir::launcher {

struct AudioDevice {
  std::wstring id;
  std::wstring name;
  bool is_default = false;
};

std::vector<AudioDevice> EnumerateAudioOutputs(std::wstring* error);
std::vector<AudioDevice> EnumerateAudioInputs(std::wstring* error);

}

#endif
