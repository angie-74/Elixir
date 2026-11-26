// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_GAME_SESSION_H_
#define ELIXIR_RUNTIME_GAME_SESSION_H_

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace elixir::window {
class GameWindow;
}
namespace jnivm {
class VM;
}
namespace mocktail::audio {
struct WebRtcMicrophonePicker;
}
namespace mocktail::runtime {
class RobloxWindowInputRuntime;
class RobloxTextInputJniBridge;
}

namespace elixir::runtime {

class ExperienceSession;

struct GameSessionOptions {
  int width = 1280;
  int height = 720;
  std::wstring title = L"Roblox";
  std::filesystem::path state_path;
  bool offscreen = false;
};

struct GameSessionGeometry {
  int width = 0;
  int height = 0;
  float dpi_scale = 1.0f;
  float refresh_hz = 0.0f;
  std::vector<float> supported_refresh_hz;
  float monitor_hz = 0.0f;
};

bool ApplyFrameRateCap(unsigned cap, float* current_hz, std::vector<float>* supported_hz);

enum class GameSessionEnd { kClosed, kStopped, kTimeLimit, kFailed };

class GameSession {
 public:
  GameSession();
  ~GameSession();
  GameSession(const GameSession&) = delete;
  GameSession& operator=(const GameSession&) = delete;

  bool Open(const GameSessionOptions& options, std::string* error);
  void* native_window() const { return native_window_; }
  GameSessionGeometry Geometry() const;

  void PumpOnce();

  void AttachExperience(ExperienceSession* experience) { experience_ = experience; }

  bool StartInput(void* roblox, jnivm::VM* vm, bool controllers, std::string* error);
  void StopInput();

  void SetMicrophonePicker(mocktail::audio::WebRtcMicrophonePicker picker);

  void SetFrameRateCap(unsigned cap) { frame_rate_cap_ = cap; }
  void FollowDisplayRefresh(void* roblox, jnivm::VM* vm);

  GameSessionEnd Run(const std::atomic<bool>& stop, std::uint64_t time_limit_ms);

  bool presented() const;
  std::uint64_t frames() const;

  void Close(void* surface);

 private:
  void HandleEvents();
  void NoteFirstFrame();
  void CheckDisplayRefresh();

  std::unique_ptr<window::GameWindow> window_;
  std::shared_ptr<mocktail::runtime::RobloxWindowInputRuntime> input_;
  std::unique_ptr<mocktail::runtime::RobloxTextInputJniBridge> text_input_;
  ExperienceSession* experience_ = nullptr;
  void* native_window_ = nullptr;
  std::uint64_t opened_ms_ = 0;
  bool first_frame_logged_ = false;
  void* refresh_roblox_ = nullptr;
  jnivm::VM* refresh_vm_ = nullptr;
  void* refresh_monitor_ = nullptr;
  unsigned refresh_hz_ = 0;
  unsigned frame_rate_cap_ = 0;
};

}

#endif
