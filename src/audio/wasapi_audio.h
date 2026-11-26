// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_AUDIO_WASAPI_AUDIO_H_
#define ELIXIR_AUDIO_WASAPI_AUDIO_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio/pcm_render_queue.h"
#include "mocktail/audio/audio_sink.h"
#include "mocktail/audio/sdl_audio_capture.h"
#include "mocktail/status.h"

namespace mocktail::audio {

enum class AudioOutputDriver {
  kWasapi,
  kNull,
};

struct WasapiAudioSubsystemOptions {
  AudioOutputDriver driver = AudioOutputDriver::kWasapi;
  bool mmcss = true;
  bool low_latency = true;
};

Status InitializeWasapiAudioSubsystem(const WasapiAudioSubsystemOptions& options = {});
Status ShutdownWasapiAudioSubsystem();
bool WasapiAudioSubsystemInitialized();
AudioOutputDriver WasapiAudioDriver();

struct WasapiAudioDevice {
  std::wstring id;
  std::string name;
  bool is_default = false;
};

Status ListWasapiPlaybackDevices(std::vector<WasapiAudioDevice>* devices);
Status ListWasapiRecordingDevices(std::vector<WasapiAudioDevice>* devices);
bool HasWasapiCaptureEndpoint() noexcept;

Status ResolveWasapiDevice(std::string_view requested,
                          const std::vector<WasapiAudioDevice>& available_devices,
                          std::wstring* endpoint_id, std::string* resolved_name, bool allow_device_name = true);

Status ConfigureWasapiPlaybackDevice(std::string_view requested,
                                     std::vector<WasapiAudioDevice>* available_devices,
                                     std::string* resolved_name);
Status ConfigureWasapiRecordingDevice(std::string_view requested,
                                      std::vector<WasapiAudioDevice>* available_devices,
                                      std::string* resolved_name, bool allow_device_name = true);
Status GetConfiguredWasapiPlaybackDevice(std::wstring* endpoint_id, std::string* resolved_name);
Status GetConfiguredWasapiRecordingDevice(std::wstring* endpoint_id, std::string* resolved_name);
Status SwitchWasapiPlaybackDevice(std::wstring_view endpoint_id, std::string* resolved_name);
Status SwitchWasapiRecordingDevice(std::wstring_view endpoint_id, std::string* resolved_name);

struct AudioEndpointTarget {
  enum class Kind {
    kConfigured,
    kSystemDefault,
    kEndpoint,
  };
  Kind kind = Kind::kConfigured;
  std::wstring endpoint_id;
};

struct WasapiAudioSinkOptions {
  PcmSpec source_spec;
  AudioEndpointTarget target;
  bool start_paused = true;
  AudioDataNeededCallback data_needed_callback = nullptr;
  void* data_needed_context = nullptr;
  bool render_thread_enters_guest = false;
};

Status CreateWasapiAudioSink(const WasapiAudioSinkOptions& options,
                             std::unique_ptr<AudioSink>* sink);

struct WasapiStreamInfo {
  std::string driver;
  std::wstring endpoint_id;
  std::string endpoint_name;
  bool follows_default = false;
  PcmLayout device_layout;
  std::uint32_t device_valid_bits = 0;
  bool low_latency_stream = false;
  std::uint32_t engine_default_period_frames = 0;
  std::uint32_t engine_fundamental_period_frames = 0;
  std::uint32_t engine_min_period_frames = 0;
  std::uint32_t engine_max_period_frames = 0;
  std::uint32_t stream_period_frames = 0;
  double device_default_period_ms = 0;
  double device_minimum_period_ms = 0;
  std::uint32_t buffer_frames = 0;
  double stream_latency_ms = 0;
  bool mmcss_registered = false;
  std::uint64_t opens = 0;
  std::uint64_t reopens = 0;
  std::uint64_t device_invalidations = 0;
  std::uint64_t default_change_notifications = 0;
  std::uint64_t render_events = 0;
  double last_device_queue_ms = 0;
  double max_device_queue_ms = 0;
  double mean_device_queue_ms = 0;
  PcmRenderQueueStats queue;
  std::size_t queued_source_bytes = 0;
  std::size_t converter_buffered_frames = 0;
};

Status GetWasapiSinkInfo(const AudioSink* sink, WasapiStreamInfo* info);

struct WasapiAudioCaptureOptions {
  PcmSpec output_spec;
  AudioEndpointTarget target;
  std::uint32_t frame_duration_ms = 10;
  AudioCaptureDataCallback data_callback = nullptr;
  void* data_context = nullptr;
  bool capture_thread_enters_guest = false;
};

Status CreateWasapiAudioCapture(const WasapiAudioCaptureOptions& options,
                                std::unique_ptr<AudioCapture>* capture);

struct WasapiCaptureInfo {
  std::string driver;
  std::wstring endpoint_id;
  PcmLayout device_layout;
  std::uint32_t buffer_frames = 0;
  double stream_latency_ms = 0;
  bool mmcss_registered = false;
  std::uint64_t opens = 0;
  std::uint64_t delivered_buffers = 0;
  std::uint64_t captured_device_frames = 0;
  std::uint64_t discontinuities = 0;
};
Status GetWasapiCaptureInfo(const AudioCapture* capture, WasapiCaptureInfo* info);

void SetMicrophoneCapturePermitted(bool permitted);
bool MicrophoneCapturePermitted();

void NotifyWasapiDefaultDeviceChanged(bool render, std::wstring_view new_default_id,
                                      bool force_reopen = false);

std::string WideToUtf8(std::wstring_view text);
std::wstring Utf8ToWide(std::string_view text);

}

#endif
