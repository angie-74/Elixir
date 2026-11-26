// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_WINDOW_WIN32_GAMEPAD_H_
#define ELIXIR_WINDOW_WIN32_GAMEPAD_H_

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mocktail/platform/platform_runtime.h"
#include "mocktail/status.h"

struct _XINPUT_GAMEPAD;
struct GameInputGamepadState;

namespace elixir::window {

enum class GamepadBackend { kNone, kGameInput, kXInput, kSynthetic };
const char* GamepadBackendName(GamepadBackend backend);

enum class GamepadBackendPreference { kAuto, kGameInput, kXInput };

struct GamepadSample {
  std::uint32_t buttons = 0;
  std::array<std::int16_t, 6> axes{};
};

GamepadSample MapXInputGamepad(const _XINPUT_GAMEPAD& gamepad);
GamepadSample MapGameInputGamepad(const GameInputGamepadState& state);
std::uint32_t StandardGamepadButtonMask();
std::uint32_t StandardGamepadAxisMask();
mocktail::platform::GamepadFamily GamepadFamilyFromUsbIds(std::uint16_t vendor_id,
                                                          std::uint16_t product_id);

struct GamepadDeviceReading {
  std::uint64_t key = 0;
  mocktail::platform::GamepadDescriptor descriptor;
  GamepadSample sample;
};

class GamepadSource {
 public:
  virtual ~GamepadSource() = default;
  virtual GamepadBackend kind() const = 0;
  virtual void Read(bool rescan, std::vector<GamepadDeviceReading>* devices) = 0;
};

std::unique_ptr<GamepadSource> CreateGameInputSource(std::string* error);
std::unique_ptr<GamepadSource> CreateXInputSource(std::string* error);

struct GamepadManagerOptions {
  bool enabled = true;
  GamepadBackendPreference preference = GamepadBackendPreference::kAuto;
  std::unique_ptr<GamepadSource> source;
};

class Win32GamepadManager final {
 public:
  using EventFn = void (*)(void*, const mocktail::platform::PlatformEvent&);

  static constexpr std::uint64_t kXInputRescanIntervalNs = 2'000'000'000ULL;

  Win32GamepadManager() = default;
  ~Win32GamepadManager();
  Win32GamepadManager(const Win32GamepadManager&) = delete;
  Win32GamepadManager& operator=(const Win32GamepadManager&) = delete;

  mocktail::Status Initialize(void* context, EventFn emit);
  mocktail::Status Initialize(void* context, EventFn emit,
                              GamepadManagerOptions options);
  void Shutdown();
  bool backend_pending() const;
  std::string backend_error() const;

  void Poll(std::uint64_t now_ns);
  void NotifyDeviceChange();
  bool HandleEvent(const mocktail::platform::PlatformEvent& event);
  void ResendState();

  bool initialized() const { return initialized_; }
  GamepadBackend backend() const;
  std::size_t connected_count() const { return devices_.size(); }
  std::uint64_t polls() const { return polls_; }
  std::uint64_t rescans() const { return rescans_; }

 private:
  struct Tracked {
    std::int64_t instance_id = 0;
    mocktail::platform::GamepadDescriptor descriptor;
    GamepadSample last;
  };

  void Emit(mocktail::platform::PlatformEventPayload payload);
  void EmitFullState(const Tracked& device);
  void AdoptPendingBackend();

  std::unique_ptr<GamepadSource> source_;
  mutable std::mutex backend_mutex_;
  std::unique_ptr<GamepadSource> opened_source_;
  std::string backend_error_;
  std::atomic<bool> backend_opening_{false};
  std::atomic<bool> backend_opened_{false};
  std::thread backend_thread_;
  std::map<std::uint64_t, Tracked> devices_;
  std::vector<GamepadDeviceReading> scratch_;
  std::atomic<bool> device_change_pending_{true};
  std::uint64_t last_rescan_ns_ = 0;
  std::int64_t next_instance_id_ = 1;
  std::uint64_t polls_ = 0;
  std::uint64_t rescans_ = 0;
  void* context_ = nullptr;
  EventFn emit_ = nullptr;
  bool initialized_ = false;
};

}

#endif
