// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail's legacy runtime (src/legacy/legacy_runtime.cc,
// Apache-2.0). Modified by Elixir.
#ifndef ELIXIR_RUNTIME_EXPERIENCE_SESSION_H_
#define ELIXIR_RUNTIME_EXPERIENCE_SESSION_H_

#include <functional>
#include <memory>
#include <string>

#include "graphics/render_scale.h"
#include "runtime/roblox_experience_presence.h"

namespace jnivm {
class VM;
}

namespace elixir::runtime {

struct ExperienceSessionOptions {
  std::string asset_folder_path;
  float dpi_scale = 1.0f;
  RenderScale game_render_scale;
  int dynamic_resolution_fps = 0;
  bool touch_enabled = false;
  bool mouse_enabled = true;
  bool keyboard_enabled = true;
  bool microphone_enabled = false;
  mocktail::runtime::RobloxExperiencePresenceObserver presence;
  std::function<void(bool in_game)> in_game_changed;
};

class ExperienceSession final {
 public:
  static std::unique_ptr<ExperienceSession> Create(void* roblox, jnivm::VM* vm, const ExperienceSessionOptions& options,
                                                   std::string* unavailable);

  ~ExperienceSession();
  ExperienceSession(const ExperienceSession&) = delete;
  ExperienceSession& operator=(const ExperienceSession&) = delete;

  bool OnRobloxStarted();

  bool DrainSurfaceEvents();
  bool DrainPlatformEvents();
  bool DrainLaunchRequests();

  bool subscribed() const;

  bool Shutdown();

 private:
  struct Impl;
  explicit ExperienceSession(std::unique_ptr<Impl> impl);

  void ReportPresence();

  std::unique_ptr<Impl> impl_;
};

}

#endif
