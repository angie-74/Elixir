// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "window/win32_gamepad.h"

#include <GameInput.h>
#include <Xinput.h>

#include <SDL3/SDL_gamepad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <utility>

namespace elixir::window {

namespace platform = mocktail::platform;
using mocktail::Status;
using mocktail::StatusCode;

namespace {

constexpr std::uint32_t Bit(int index) { return std::uint32_t{1} << index; }

std::int16_t FlipStickY(std::int16_t value) {
  return value == -32768 ? std::int16_t{32767} : static_cast<std::int16_t>(-value);
}

std::int16_t StickFromFloat(float value) {
  if (!std::isfinite(value)) {
    return 0;
  }
  const float clamped = std::clamp(value, -1.0F, 1.0F);
  const float scaled = clamped < 0.0F ? clamped * 32768.0F : clamped * 32767.0F;
  return static_cast<std::int16_t>(std::lround(std::clamp(scaled, -32768.0F, 32767.0F)));
}

std::int16_t TriggerFromFloat(float value) {
  if (!std::isfinite(value)) {
    return 0;
  }
  return static_cast<std::int16_t>(
      std::lround(std::clamp(value, 0.0F, 1.0F) * 32767.0F));
}

std::uint64_t NowNs() {
  static const std::uint64_t frequency = [] {
    LARGE_INTEGER value{};
    QueryPerformanceFrequency(&value);
    return static_cast<std::uint64_t>(value.QuadPart);
  }();
  LARGE_INTEGER counter{};
  QueryPerformanceCounter(&counter);
  const auto ticks = static_cast<std::uint64_t>(counter.QuadPart);
  return (ticks / frequency) * 1'000'000'000ULL +
         (ticks % frequency) * 1'000'000'000ULL / frequency;
}

HMODULE LoadSystemLibrary(const wchar_t* name) {
  return LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

class XInputSource final : public GamepadSource {
 public:
  using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
  using GetCapabilitiesFn = DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);

  XInputSource(HMODULE module, GetStateFn get_state,
               GetCapabilitiesFn get_capabilities)
      : module_(module),
        get_state_(get_state),
        get_capabilities_(get_capabilities) {}
  ~XInputSource() override {
    if (module_ != nullptr) {
      FreeLibrary(module_);
    }
  }

  GamepadBackend kind() const override { return GamepadBackend::kXInput; }

  void Read(bool rescan, std::vector<GamepadDeviceReading>* devices) override {
    devices->clear();
    for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot) {
      if (!connected_[slot] && !rescan) {
        continue;
      }
      XINPUT_STATE state{};
      if (get_state_(slot, &state) != ERROR_SUCCESS) {
        connected_[slot] = false;
        continue;
      }
      if (!connected_[slot]) {
        connected_[slot] = true;
        ++generation_[slot];
        Describe(slot);
      }
      GamepadDeviceReading reading;
      reading.key = (std::uint64_t{generation_[slot]} << 8) | slot;
      reading.descriptor = descriptors_[slot];
      reading.sample = MapXInputGamepad(state.Gamepad);
      devices->push_back(std::move(reading));
    }
  }

 private:
  void Describe(DWORD slot) {
    platform::GamepadDescriptor descriptor;
    descriptor.family = platform::GamepadFamily::kXbox;
    descriptor.buttons = StandardGamepadButtonMask();
    descriptor.axes = StandardGamepadAxisMask();
    descriptor.name = "XInput controller " + std::to_string(slot + 1);
    XINPUT_CAPABILITIES capabilities{};
    if (get_capabilities_ != nullptr &&
        get_capabilities_(slot, XINPUT_FLAG_GAMEPAD, &capabilities) ==
            ERROR_SUCCESS &&
        capabilities.SubType != XINPUT_DEVSUBTYPE_GAMEPAD) {
      descriptor.name += " (subtype " + std::to_string(capabilities.SubType) + ")";
    }
    descriptors_[slot] = std::move(descriptor);
  }

  HMODULE module_ = nullptr;
  GetStateFn get_state_ = nullptr;
  GetCapabilitiesFn get_capabilities_ = nullptr;
  std::array<bool, XUSER_MAX_COUNT> connected_{};
  std::array<std::uint32_t, XUSER_MAX_COUNT> generation_{};
  std::array<platform::GamepadDescriptor, XUSER_MAX_COUNT> descriptors_{};
};

class SharedGameInput final {
 public:
  static IGameInput* Acquire(std::string* error) {
    SharedGameInput& shared = Instance();
    std::lock_guard<std::mutex> lock(shared.mutex_);
    if (shared.game_input_ == nullptr && !shared.Open(error)) {
      return nullptr;
    }
    shared.game_input_->AddRef();
    return shared.game_input_;
  }

 private:
  static SharedGameInput& Instance() {
    static SharedGameInput* const instance = new SharedGameInput();
    return *instance;
  }

  bool Open(std::string* error) {
    if (module_ == nullptr) {
      module_ = LoadSystemLibrary(L"GameInput.dll");
      if (module_ == nullptr) {
        if (error != nullptr) {
          *error = "GameInput.dll is not installed";
        }
        return false;
      }
    }
    using CreateFn = HRESULT(WINAPI*)(IGameInput**);
    auto create = reinterpret_cast<CreateFn>(
        reinterpret_cast<void*>(GetProcAddress(module_, "GameInputCreate")));
    if (create == nullptr) {
      if (error != nullptr) {
        *error = "GameInput.dll has no GameInputCreate";
      }
      return false;
    }
    IGameInput* game_input = nullptr;
    const HRESULT result = create(&game_input);
    if (FAILED(result) || game_input == nullptr) {
      if (error != nullptr) {
        char text[64];
        std::snprintf(text, sizeof(text), "GameInputCreate failed (0x%08lx)",
                      static_cast<unsigned long>(result));
        *error = text;
      }
      return false;
    }
    game_input_ = game_input;
    return true;
  }

  std::mutex mutex_;
  HMODULE module_ = nullptr;
  IGameInput* game_input_ = nullptr;
};

class GameInputSource final : public GamepadSource {
 public:
  explicit GameInputSource(IGameInput* game_input) : game_input_(game_input) {}

  ~GameInputSource() override {
    if (game_input_ != nullptr) {
      if (token_ != 0) {
        if (!game_input_->UnregisterCallback(token_, 500'000)) {
          std::fprintf(stderr, "  [gamepad] GameInput device callback did not stop in 500 ms\n");
        }
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : devices_) {
          entry.second.device->Release();
        }
        devices_.clear();
      }
      game_input_->Release();
    }
  }

  bool Start(std::string* error) {
    const HRESULT result = game_input_->RegisterDeviceCallback(
        nullptr, GameInputKindGamepad, GameInputDeviceConnected,
        GameInputAsyncEnumeration, this, &GameInputSource::OnDevice, &token_);
    if (FAILED(result)) {
      if (error != nullptr) {
        char text[96];
        std::snprintf(text, sizeof(text),
                      "GameInput RegisterDeviceCallback failed (0x%08lx)",
                      static_cast<unsigned long>(result));
        *error = text;
      }
      token_ = 0;
      return false;
    }
    return true;
  }

  GamepadBackend kind() const override { return GamepadBackend::kGameInput; }

  void Read(bool , std::vector<GamepadDeviceReading>* devices) override {
    devices->clear();
    std::vector<Entry> snapshot;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot.reserve(devices_.size());
      for (auto& entry : devices_) {
        entry.second.device->AddRef();
        snapshot.push_back(entry.second);
      }
    }
    for (const Entry& entry : snapshot) {
      GamepadDeviceReading reading;
      reading.key = entry.key;
      reading.descriptor = entry.descriptor;
      const auto last = last_samples_.find(entry.key);
      if (last != last_samples_.end()) reading.sample = last->second;
      IGameInputReading* current = nullptr;
      if (SUCCEEDED(game_input_->GetCurrentReading(GameInputKindGamepad,
                                                   entry.device, &current)) &&
          current != nullptr) {
        GameInputGamepadState state{};
        if (current->GetGamepadState(&state)) {
          reading.sample = MapGameInputGamepad(state);
          last_samples_[entry.key] = reading.sample;
        }
        current->Release();
      }
      entry.device->Release();
      devices->push_back(std::move(reading));
    }
    for (auto it = last_samples_.begin(); it != last_samples_.end();) {
      const bool present =
          std::any_of(devices->begin(), devices->end(),
                      [&](const GamepadDeviceReading& r) { return r.key == it->first; });
      it = present ? std::next(it) : last_samples_.erase(it);
    }
  }

 private:
  struct Entry {
    std::uint64_t key = 0;
    IGameInputDevice* device = nullptr;
    platform::GamepadDescriptor descriptor;
  };

  static void CALLBACK OnDevice(GameInputCallbackToken, void* context,
                                IGameInputDevice* device, uint64_t,
                                GameInputDeviceStatus current,
                                GameInputDeviceStatus) {
    auto* self = static_cast<GameInputSource*>(context);
    const bool now_connected = (current & GameInputDeviceConnected) != 0;
    const auto key = reinterpret_cast<std::uint64_t>(device);
    std::lock_guard<std::mutex> lock(self->mutex_);
    if (now_connected) {
      if (self->devices_.count(key) != 0) {
        return;
      }
      Entry entry;
      entry.key = ++self->next_device_key_;
      entry.device = device;
      device->AddRef();
      const GameInputDeviceInfo* info = device->GetDeviceInfo();
      entry.descriptor.buttons = StandardGamepadButtonMask();
      entry.descriptor.axes = StandardGamepadAxisMask();
      if (info != nullptr) {
        entry.descriptor.family =
            GamepadFamilyFromUsbIds(info->vendorId, info->productId);
        if (entry.descriptor.family == platform::GamepadFamily::kUnknown &&
            (info->deviceFamily == GameInputFamilyXboxOne ||
             info->deviceFamily == GameInputFamilyXbox360)) {
          entry.descriptor.family = platform::GamepadFamily::kXbox;
        }
        if (info->displayName != nullptr && info->displayName->data != nullptr) {
          entry.descriptor.name.assign(info->displayName->data,
                                       info->displayName->sizeInBytes);
          while (!entry.descriptor.name.empty() &&
                 entry.descriptor.name.back() == '\0') {
            entry.descriptor.name.pop_back();
          }
        }
        if (entry.descriptor.name.empty()) {
          char name[64];
          std::snprintf(name, sizeof(name), "GameInput gamepad %04x:%04x",
                        info->vendorId, info->productId);
          entry.descriptor.name = name;
        }
      } else {
        entry.descriptor.family = platform::GamepadFamily::kXbox;
        entry.descriptor.name = "GameInput gamepad";
      }
      self->devices_.emplace(key, std::move(entry));
    } else {
      const auto found = self->devices_.find(key);
      if (found != self->devices_.end()) {
        found->second.device->Release();
        self->devices_.erase(found);
      }
    }
  }

  IGameInput* game_input_ = nullptr;
  GameInputCallbackToken token_ = 0;
  std::mutex mutex_;
  std::uint64_t next_device_key_ = 0;
  std::map<std::uint64_t, Entry> devices_;
  std::map<std::uint64_t, GamepadSample> last_samples_;
};

}

const char* GamepadBackendName(GamepadBackend backend) {
  switch (backend) {
    case GamepadBackend::kNone:
      return "none";
    case GamepadBackend::kGameInput:
      return "GameInput";
    case GamepadBackend::kXInput:
      return "XInput";
    case GamepadBackend::kSynthetic:
      return "synthetic";
  }
  return "unknown";
}

std::uint32_t StandardGamepadButtonMask() {
  return Bit(SDL_GAMEPAD_BUTTON_SOUTH) | Bit(SDL_GAMEPAD_BUTTON_EAST) |
         Bit(SDL_GAMEPAD_BUTTON_WEST) | Bit(SDL_GAMEPAD_BUTTON_NORTH) |
         Bit(SDL_GAMEPAD_BUTTON_BACK) | Bit(SDL_GAMEPAD_BUTTON_START) |
         Bit(SDL_GAMEPAD_BUTTON_LEFT_STICK) | Bit(SDL_GAMEPAD_BUTTON_RIGHT_STICK) |
         Bit(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) |
         Bit(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) | Bit(SDL_GAMEPAD_BUTTON_DPAD_UP) |
         Bit(SDL_GAMEPAD_BUTTON_DPAD_DOWN) | Bit(SDL_GAMEPAD_BUTTON_DPAD_LEFT) |
         Bit(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
}

std::uint32_t StandardGamepadAxisMask() {
  return (std::uint32_t{1} << SDL_GAMEPAD_AXIS_COUNT) - 1;
}

GamepadSample MapXInputGamepad(const _XINPUT_GAMEPAD& gamepad) {
  struct Pair {
    WORD xinput;
    int sdl;
  };
  static constexpr Pair kButtons[] = {
      {XINPUT_GAMEPAD_A, SDL_GAMEPAD_BUTTON_SOUTH},
      {XINPUT_GAMEPAD_B, SDL_GAMEPAD_BUTTON_EAST},
      {XINPUT_GAMEPAD_X, SDL_GAMEPAD_BUTTON_WEST},
      {XINPUT_GAMEPAD_Y, SDL_GAMEPAD_BUTTON_NORTH},
      {XINPUT_GAMEPAD_BACK, SDL_GAMEPAD_BUTTON_BACK},
      {XINPUT_GAMEPAD_START, SDL_GAMEPAD_BUTTON_START},
      {XINPUT_GAMEPAD_LEFT_THUMB, SDL_GAMEPAD_BUTTON_LEFT_STICK},
      {XINPUT_GAMEPAD_RIGHT_THUMB, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
      {XINPUT_GAMEPAD_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
      {XINPUT_GAMEPAD_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
      {XINPUT_GAMEPAD_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_UP},
      {XINPUT_GAMEPAD_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
      {XINPUT_GAMEPAD_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
      {XINPUT_GAMEPAD_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
  };
  GamepadSample sample;
  for (const Pair& pair : kButtons) {
    if ((gamepad.wButtons & pair.xinput) != 0) {
      sample.buttons |= Bit(pair.sdl);
    }
  }
  sample.axes[SDL_GAMEPAD_AXIS_LEFTX] = gamepad.sThumbLX;
  sample.axes[SDL_GAMEPAD_AXIS_LEFTY] = FlipStickY(gamepad.sThumbLY);
  sample.axes[SDL_GAMEPAD_AXIS_RIGHTX] = gamepad.sThumbRX;
  sample.axes[SDL_GAMEPAD_AXIS_RIGHTY] = FlipStickY(gamepad.sThumbRY);
  sample.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] =
      static_cast<std::int16_t>((gamepad.bLeftTrigger * 32767 + 127) / 255);
  sample.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] =
      static_cast<std::int16_t>((gamepad.bRightTrigger * 32767 + 127) / 255);
  return sample;
}

GamepadSample MapGameInputGamepad(const GameInputGamepadState& state) {
  struct Pair {
    std::uint32_t game_input;
    int sdl;
  };
  static constexpr Pair kButtons[] = {
      {GameInputGamepadA, SDL_GAMEPAD_BUTTON_SOUTH},
      {GameInputGamepadB, SDL_GAMEPAD_BUTTON_EAST},
      {GameInputGamepadX, SDL_GAMEPAD_BUTTON_WEST},
      {GameInputGamepadY, SDL_GAMEPAD_BUTTON_NORTH},
      {GameInputGamepadView, SDL_GAMEPAD_BUTTON_BACK},
      {GameInputGamepadMenu, SDL_GAMEPAD_BUTTON_START},
      {GameInputGamepadLeftThumbstick, SDL_GAMEPAD_BUTTON_LEFT_STICK},
      {GameInputGamepadRightThumbstick, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
      {GameInputGamepadLeftShoulder, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
      {GameInputGamepadRightShoulder, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
      {GameInputGamepadDPadUp, SDL_GAMEPAD_BUTTON_DPAD_UP},
      {GameInputGamepadDPadDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
      {GameInputGamepadDPadLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
      {GameInputGamepadDPadRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
  };
  GamepadSample sample;
  const auto buttons = static_cast<std::uint32_t>(state.buttons);
  for (const Pair& pair : kButtons) {
    if ((buttons & pair.game_input) != 0) {
      sample.buttons |= Bit(pair.sdl);
    }
  }
  sample.axes[SDL_GAMEPAD_AXIS_LEFTX] = StickFromFloat(state.leftThumbstickX);
  sample.axes[SDL_GAMEPAD_AXIS_LEFTY] =
      FlipStickY(StickFromFloat(state.leftThumbstickY));
  sample.axes[SDL_GAMEPAD_AXIS_RIGHTX] = StickFromFloat(state.rightThumbstickX);
  sample.axes[SDL_GAMEPAD_AXIS_RIGHTY] =
      FlipStickY(StickFromFloat(state.rightThumbstickY));
  sample.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = TriggerFromFloat(state.leftTrigger);
  sample.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] =
      TriggerFromFloat(state.rightTrigger);
  return sample;
}

platform::GamepadFamily GamepadFamilyFromUsbIds(std::uint16_t vendor_id,
                                                std::uint16_t product_id) {
  switch (vendor_id) {
    case 0x045E:
      return platform::GamepadFamily::kXbox;
    case 0x054C:
      switch (product_id) {
        case 0x0CE6:
        case 0x0DF2:
          return platform::GamepadFamily::kPlayStation5;
        default:
          return platform::GamepadFamily::kPlayStation4;
      }
    case 0x057E:
      return platform::GamepadFamily::kNintendoSwitch;
    default:
      return platform::GamepadFamily::kUnknown;
  }
}

std::unique_ptr<GamepadSource> CreateXInputSource(std::string* error) {
  HMODULE module = LoadSystemLibrary(L"xinput1_4.dll");
  if (module == nullptr) {
    if (error != nullptr) {
      *error = "xinput1_4.dll is not available";
    }
    return nullptr;
  }
  auto get_state = reinterpret_cast<XInputSource::GetStateFn>(
      reinterpret_cast<void*>(GetProcAddress(module, "XInputGetState")));
  auto get_capabilities = reinterpret_cast<XInputSource::GetCapabilitiesFn>(
      reinterpret_cast<void*>(GetProcAddress(module, "XInputGetCapabilities")));
  if (get_state == nullptr) {
    FreeLibrary(module);
    if (error != nullptr) {
      *error = "xinput1_4.dll has no XInputGetState";
    }
    return nullptr;
  }
  return std::make_unique<XInputSource>(module, get_state, get_capabilities);
}

std::unique_ptr<GamepadSource> CreateGameInputSource(std::string* error) {
  IGameInput* game_input = SharedGameInput::Acquire(error);
  if (game_input == nullptr) {
    return nullptr;
  }
  auto source = std::make_unique<GameInputSource>(game_input);
  if (!source->Start(error)) {
    return nullptr;
  }
  return source;
}

Win32GamepadManager::~Win32GamepadManager() { Shutdown(); }

Status Win32GamepadManager::Initialize(void* context, EventFn emit) {
  return Initialize(context, emit, GamepadManagerOptions{});
}

Status Win32GamepadManager::Initialize(void* context, EventFn emit,
                                       GamepadManagerOptions options) {
  if (initialized_ || emit == nullptr) {
    return Status::Error(StatusCode::kFailedPrecondition,
                         "gamepad owner is already initialized or has no sink");
  }
  if (!options.enabled) {
    return Status::Error(StatusCode::kUnavailable,
                         "controllers are disabled (--controller off)");
  }
  context_ = context;
  emit_ = emit;
  initialized_ = true;
  device_change_pending_.store(true, std::memory_order_relaxed);
  last_rescan_ns_ = 0;
  if (options.source != nullptr) {
    source_ = std::move(options.source);
    Poll(NowNs());
    return Status::Ok();
  }
  {
    std::lock_guard<std::mutex> lock(backend_mutex_);
    opened_source_.reset();
    backend_error_.clear();
  }
  backend_opened_.store(false, std::memory_order_relaxed);
  backend_opening_.store(true, std::memory_order_release);
  const GamepadBackendPreference preference = options.preference;
  backend_thread_ = std::thread([this, preference] {
    std::string game_input_error;
    std::string xinput_error;
    std::unique_ptr<GamepadSource> source;
    if (preference != GamepadBackendPreference::kXInput) {
      source = CreateGameInputSource(&game_input_error);
    }
    if (source == nullptr && preference != GamepadBackendPreference::kGameInput) {
      source = CreateXInputSource(&xinput_error);
    }
    std::lock_guard<std::mutex> lock(backend_mutex_);
    if (source == nullptr) {
      std::string message = "no controller backend";
      if (!game_input_error.empty()) message += ": " + game_input_error;
      if (!xinput_error.empty()) message += "; " + xinput_error;
      backend_error_ = std::move(message);
      std::fprintf(stderr, "  [gamepad] unavailable: %s\n", backend_error_.c_str());
    } else if (!game_input_error.empty()) {
      std::fprintf(stderr, "  [gamepad] GameInput unavailable (%s); using %s\n",
                   game_input_error.c_str(), GamepadBackendName(source->kind()));
    } else {
      std::fprintf(stderr, "  [gamepad] backend: %s\n", GamepadBackendName(source->kind()));
    }
    opened_source_ = std::move(source);
    backend_opened_.store(true, std::memory_order_release);
    backend_opening_.store(false, std::memory_order_release);
  });
  return Status::Ok();
}

bool Win32GamepadManager::backend_pending() const {
  return backend_opening_.load(std::memory_order_acquire);
}

std::string Win32GamepadManager::backend_error() const {
  std::lock_guard<std::mutex> lock(backend_mutex_);
  return backend_error_;
}

void Win32GamepadManager::AdoptPendingBackend() {
  if (source_ != nullptr || !backend_opened_.load(std::memory_order_acquire)) {
    return;
  }
  std::lock_guard<std::mutex> lock(backend_mutex_);
  source_ = std::move(opened_source_);
  backend_opened_.store(false, std::memory_order_relaxed);
  device_change_pending_.store(true, std::memory_order_relaxed);
}

void Win32GamepadManager::Shutdown() {
  if (backend_thread_.joinable()) {
    backend_thread_.join();
  }
  {
    std::lock_guard<std::mutex> lock(backend_mutex_);
    opened_source_.reset();
  }
  backend_opened_.store(false, std::memory_order_relaxed);
  if (!initialized_) {
    return;
  }
  for (const auto& entry : devices_) {
    Emit(platform::GamepadConnectionEvent{entry.second.instance_id, false, {}, false});
  }
  devices_.clear();
  source_.reset();
  emit_ = nullptr;
  context_ = nullptr;
  initialized_ = false;
}

GamepadBackend Win32GamepadManager::backend() const {
  return source_ != nullptr ? source_->kind() : GamepadBackend::kNone;
}

void Win32GamepadManager::NotifyDeviceChange() {
  device_change_pending_.store(true, std::memory_order_release);
}

void Win32GamepadManager::Emit(platform::PlatformEventPayload payload) {
  if (emit_ != nullptr) {
    emit_(context_, {NowNs(), std::move(payload)});
  }
}

void Win32GamepadManager::EmitFullState(const Tracked& device) {
  for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
    if ((device.descriptor.axes & Bit(axis)) != 0) {
      Emit(platform::GamepadAxisEvent{device.instance_id,
                                      static_cast<std::uint8_t>(axis),
                                      device.last.axes[axis]});
    }
  }
  for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
    if ((device.descriptor.buttons & Bit(button)) != 0) {
      Emit(platform::GamepadButtonEvent{device.instance_id,
                                        static_cast<std::uint8_t>(button),
                                        (device.last.buttons & Bit(button)) != 0});
    }
  }
}

void Win32GamepadManager::Poll(std::uint64_t now_ns) {
  if (!initialized_) {
    return;
  }
  AdoptPendingBackend();
  if (source_ == nullptr) {
    return;
  }
  ++polls_;
  bool rescan = device_change_pending_.exchange(false, std::memory_order_acq_rel);
  if (!rescan && (last_rescan_ns_ == 0 || now_ns < last_rescan_ns_ ||
                  now_ns - last_rescan_ns_ >= kXInputRescanIntervalNs)) {
    rescan = true;
  }
  if (rescan) {
    last_rescan_ns_ = now_ns;
    ++rescans_;
  }
  source_->Read(rescan, &scratch_);

  for (auto it = devices_.begin(); it != devices_.end();) {
    const bool present =
        std::any_of(scratch_.begin(), scratch_.end(),
                    [&](const GamepadDeviceReading& r) { return r.key == it->first; });
    if (present) {
      ++it;
      continue;
    }
    Emit(platform::GamepadConnectionEvent{it->second.instance_id, false, {}, false});
    it = devices_.erase(it);
  }

  for (const GamepadDeviceReading& reading : scratch_) {
    auto found = devices_.find(reading.key);
    if (found == devices_.end()) {
      Tracked tracked;
      tracked.instance_id = next_instance_id_++;
      tracked.descriptor = reading.descriptor;
      tracked.last = reading.sample;
      found = devices_.emplace(reading.key, std::move(tracked)).first;
      Emit(platform::GamepadConnectionEvent{found->second.instance_id, true,
                                            found->second.descriptor, false,
                                            true});
      EmitFullState(found->second);
      continue;
    }
    Tracked& tracked = found->second;
    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
      if ((tracked.descriptor.axes & Bit(axis)) != 0 &&
          tracked.last.axes[axis] != reading.sample.axes[axis]) {
        Emit(platform::GamepadAxisEvent{tracked.instance_id,
                                        static_cast<std::uint8_t>(axis),
                                        reading.sample.axes[axis]});
      }
    }
    const std::uint32_t changed = tracked.last.buttons ^ reading.sample.buttons;
    for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
      if ((changed & Bit(button)) != 0 &&
          (tracked.descriptor.buttons & Bit(button)) != 0) {
        Emit(platform::GamepadButtonEvent{tracked.instance_id,
                                          static_cast<std::uint8_t>(button),
                                          (reading.sample.buttons & Bit(button)) != 0});
      }
    }
    tracked.last = reading.sample;
  }
}

bool Win32GamepadManager::HandleEvent(const platform::PlatformEvent& event) {
  return initialized_ &&
         (std::holds_alternative<platform::GamepadConnectionEvent>(event.payload) ||
          std::holds_alternative<platform::GamepadAxisEvent>(event.payload) ||
          std::holds_alternative<platform::GamepadButtonEvent>(event.payload));
}

void Win32GamepadManager::ResendState() {
  for (const auto& entry : devices_) {
    EmitFullState(entry.second);
  }
}

}
