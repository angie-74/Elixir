// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from Mocktail src/audio/roblox_output_device_bridge.cc (Apache-2.0).
// Modified by Elixir.
#include "audio/audio_device_menu_windows.h"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <utility>

#include "audio/wasapi_audio.h"

namespace mocktail::audio {
namespace {

constexpr std::size_t kMaximumMenuDevices = 128;
constexpr std::size_t kMaximumDeviceNameBytes = 512;

Status BuildRoutes(bool input, std::vector<AudioMenuDevice>* routes, int* selected) {
  std::vector<WasapiAudioDevice> devices;
  Status status = input ? ListWasapiRecordingDevices(&devices) : ListWasapiPlaybackDevices(&devices);
  if (!status.ok()) return status;
  if (devices.size() + 1 > kMaximumMenuDevices) {
    return Status::Error(StatusCode::kInvalidArgument,
                         input ? "host exposes too many audio input devices"
                               : "host exposes too many audio output devices");
  }
  std::wstring configured_id;
  std::string configured_name;
  status = input ? GetConfiguredWasapiRecordingDevice(&configured_id, &configured_name)
                 : GetConfiguredWasapiPlaybackDevice(&configured_id, &configured_name);
  if (!status.ok()) return status;
  routes->clear();
  routes->push_back({std::wstring(), "System Default", input ? "mocktail:input:default" : "mocktail:default"});
  *selected = 0;
  for (const WasapiAudioDevice& device : devices) {
    if (device.name.empty() || device.name.size() > kMaximumDeviceNameBytes) {
      routes->clear();
      return Status::Error(StatusCode::kInvalidArgument, "audio endpoint name is empty or too long");
    }
    const std::string guid = MakeEndpointGuid(device.id, device.name);
    routes->push_back({device.id, device.name, input ? "input:" + guid : guid});
    if (!configured_id.empty() && device.id == configured_id) {
      *selected = static_cast<int>(routes->size() - 1);
    }
  }
  if (!configured_id.empty() && *selected == 0) {
    routes->clear();
    return Status::Error(StatusCode::kFailedPrecondition,
                         input ? "configured audio input disappeared" : "configured audio output disappeared");
  }
  return Status::Ok();
}

}

std::string MakeEndpointGuid(std::wstring_view endpoint_id, std::string_view name) {
  if (endpoint_id.empty()) return "mocktail:default";
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char byte : name) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  for (const wchar_t unit : endpoint_id) {
    hash ^= static_cast<std::uint8_t>(unit & 0xff);
    hash *= 1099511628211ULL;
    hash ^= static_cast<std::uint8_t>((unit >> 8) & 0xff);
    hash *= 1099511628211ULL;
  }
  std::array<char, 20> encoded{};
  std::snprintf(encoded.data(), encoded.size(), "mt:%016" PRIx64, hash);
  return encoded.data();
}

Status HostAudioDeviceMenu::Refresh() {
  std::vector<AudioMenuDevice> outputs;
  int selected_output = 0;
  Status status = BuildRoutes(false, &outputs, &selected_output);
  if (!status.ok()) return status;
  std::vector<AudioMenuDevice> inputs;
  int selected_input = 0;
  if (MicrophoneCapturePermitted()) {
    status = BuildRoutes(true, &inputs, &selected_input);
    if (!status.ok()) return status;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  outputs_ = std::move(outputs);
  inputs_ = std::move(inputs);
  selected_output_ = selected_output;
  selected_input_ = selected_input;
  return Status::Ok();
}

int HostAudioDeviceMenu::DeviceCount(bool input) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<int>((input ? inputs_ : outputs_).size());
}

bool HostAudioDeviceMenu::Device(int index, AudioMenuDevice* device, bool input) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto& routes = input ? inputs_ : outputs_;
  if (device == nullptr || index < 0 || static_cast<std::size_t>(index) >= routes.size()) return false;
  *device = routes[static_cast<std::size_t>(index)];
  return true;
}

int HostAudioDeviceMenu::CurrentDevice(bool input) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return input ? selected_input_ : selected_output_;
}

Status HostAudioDeviceMenu::SelectDevice(int index, bool input) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto& routes = input ? inputs_ : outputs_;
  if (index < 0 || static_cast<std::size_t>(index) >= routes.size()) {
    return Status::Error(StatusCode::kInvalidArgument, "audio route index is out of range");
  }
  const AudioMenuDevice& route = routes[static_cast<std::size_t>(index)];
  std::string resolved_name;
  const Status status = input ? SwitchWasapiRecordingDevice(route.endpoint_id, &resolved_name)
                              : SwitchWasapiPlaybackDevice(route.endpoint_id, &resolved_name);
  if (!status.ok()) {
    std::fprintf(stderr, "  [audio-menu] cannot select host %s '%s': %s\n", input ? "input" : "output",
                 route.name.c_str(), status.message().c_str());
    return status;
  }
  (input ? selected_input_ : selected_output_) = index;
  std::fprintf(stderr, "  [audio-menu] selected host %s=%s\n", input ? "input" : "output",
               resolved_name.c_str());
  return Status::Ok();
}

}
