// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_AUDIO_AUDIO_DEVICE_MENU_WINDOWS_H_
#define ELIXIR_AUDIO_AUDIO_DEVICE_MENU_WINDOWS_H_

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "mocktail/status.h"

namespace mocktail::audio {

struct AudioMenuDevice {
  std::wstring endpoint_id;
  std::string name;
  std::string guid;
};

std::string MakeEndpointGuid(std::wstring_view endpoint_id, std::string_view name);

class HostAudioDeviceMenu final {
 public:
  Status Refresh();
  int DeviceCount(bool input = false) const;
  bool Device(int index, AudioMenuDevice* device, bool input = false) const;
  int CurrentDevice(bool input = false) const;
  Status SelectDevice(int index, bool input = false);

 private:
  mutable std::mutex mutex_;
  std::vector<AudioMenuDevice> outputs_;
  std::vector<AudioMenuDevice> inputs_;
  int selected_output_ = 0;
  int selected_input_ = 0;
};

}

#endif
