// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The helpers in the anonymous namespace are Mocktail's
// (src/legacy/legacy_runtime.cc, Apache-2.0). Modified by Elixir.
#include "elixir_runtime/experience_session.h"

#include <jni.h>
#include <mcpelauncher/linker.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "elixir_runtime/host_log.h"
#include "elixir_runtime/link_joins.h"
#include "jnivm/jnivm.h"
#include "mocktail/platform/platform_runtime.h"
#include "mocktail/status.h"
#include "runtime/auth_runtime_composition.h"
#include "runtime/external_launch_broker.h"
#include "graphics/render_scale.h"
#include "runtime/jnivm_platform_web_callbacks.h"
#include "runtime/roblox_capability_resolver.h"
#include "runtime/roblox_experience_composition.h"
#include "runtime/roblox_experience_presence.h"
#include "runtime/roblox_game_session_native_adapter.h"
#include "runtime/roblox_platform_web_symbols.h"
#include "window/window.h"
#include "window/window_game_surface_bridge.h"

#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
#include "android/android.h"
#endif
#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
#include "graphics/vulkan/present_scaling.h"
#endif

namespace elixir::runtime {
namespace {

struct GameScaling {
  RenderScale wanted;
  bool applied = false;
};

std::string DescribeScale(const RenderScale& scale) {
  std::string text;
  if (RendersSmaller(scale.size)) {
    text = std::to_string(static_cast<int>(scale.size * 100.0f + 0.5f)) + "% of the window's size";
  }
  if (RendersSmaller(scale.width)) {
    if (!text.empty()) text += ", ";
    text += std::to_string(static_cast<int>((1.0f - scale.width) * 100.0f + 0.5f)) + "% narrower";
  }
  return text;
}

RenderScale GameRenderScaleForLaunch(void* context) {
  auto* scaling = static_cast<GameScaling*>(context);
  if (scaling == nullptr || !scaling->wanted.smaller()) return {};
#if defined(ELIXIR_RUNTIME_HAS_VULKAN) && defined(ELIXIR_RUNTIME_HAS_ANDROID)
  if (vulkan::PresentStretchState() == vulkan::PresentStretch::kSupported) {
    Log("[experience] resolution scale: this game renders at " + DescribeScale(scaling->wanted) +
        " and the graphics driver stretches it");
    scaling->applied = true;
    mocktail::platform::SurfaceHorizontalScale().store(scaling->wanted.stretch(), std::memory_order_relaxed);
    return scaling->wanted;
  }
#endif
  Log("[experience] resolution scale: " + DescribeScale(scaling->wanted) +
      " is not used, because this graphics driver cannot stretch a smaller picture to the window");
  return {};
}

void BindSurfaceRenderScale(void*, jobject surface, RenderScale render_scale) {
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  android::SetSurfaceRenderScale(surface, render_scale);
#else
  static_cast<void>(surface);
  static_cast<void>(render_scale);
#endif
}

void ResetSurfaceRenderScales(void* context) {
  if (context != nullptr) static_cast<GameScaling*>(context)->applied = false;
  mocktail::platform::SurfaceHorizontalScale().store(1.0f, std::memory_order_relaxed);
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  android::ResetSurfaceRenderScales();
#endif
}

namespace mr = mocktail::runtime;
namespace mw = mocktail::window;

constexpr char kRobloxBaseUrl[] = "https://www.roblox.com/";

void* ResolveRobloxSymbol(void* context, const char* name) {
  return context != nullptr && name != nullptr ? linker::dlsym(context, name) : nullptr;
}

void RestoreJniFunctions(void* context) {
  if (context != nullptr) static_cast<jnivm::VM*>(context)->RestoreFunctions();
}

template <typename Function>
Function ResolveNative(void* roblox, const char* name, std::vector<std::string>* missing) {
  const auto function = reinterpret_cast<Function>(linker::dlsym(roblox, name));
  if (function == nullptr) missing->emplace_back(name);
  return function;
}

std::string MissingNatives(const std::vector<std::string>& names) {
  std::string text = "libroblox.so does not export " + std::to_string(names.size()) + " of them:";
  for (const std::string& name : names) text += " " + name;
  return text;
}

bool RegisterFreshGamePresentObserver(void* context, mr::FreshLaunchPresentObserver observer,
                                      void* observer_context) {
  if (context == nullptr) return false;
  return static_cast<mw::ScopedPresentObserver*>(context)->Register(observer, observer_context);
}

void ClearFreshGamePresentObserver(void* context) {
  if (context != nullptr) static_cast<mw::ScopedPresentObserver*>(context)->Reset();
}

jobject CreateExperienceRawCallback(void* context, std::shared_ptr<void> callback_context,
                                    void (*run)(void*, JNIEnv*, jstring)) {
  if (context == nullptr || run == nullptr) return nullptr;
  return static_cast<jnivm::VM*>(context)->CreateMessageBusRawCallback(std::move(callback_context),
                                                                       jnivm::MessageBusRawCallbacks{run});
}

void ClearExperienceRawCallback(void* context, jobject callback) {
  if (context != nullptr) static_cast<jnivm::VM*>(context)->ClearMessageBusRawCallback(callback);
}

jobject CreateMessageBusRequestHandler(void* context, std::shared_ptr<void> callback_context,
                                       std::string (*run)(void*, JNIEnv*, jstring)) {
  if (context == nullptr || run == nullptr) return nullptr;
  return static_cast<jnivm::VM*>(context)->CreateMessageBusRequestHandler(
      std::move(callback_context), jnivm::MessageBusRequestHandlerCallbacks{run});
}

void ClearMessageBusRequestHandler(void* context, jobject handler) {
  if (context != nullptr) static_cast<jnivm::VM*>(context)->ClearMessageBusRequestHandler(handler);
}

jobject CreateBrowserServiceMemStorageCallback(void* context, std::shared_ptr<void> callback_context,
                                               void (*on_item_set)(void*, JNIEnv*, jstring)) {
  if (context == nullptr || on_item_set == nullptr) return nullptr;
  return static_cast<jnivm::VM*>(context)->CreateMemStorageCallback(std::move(callback_context),
                                                                    jnivm::MemStorageCallbackCallbacks{on_item_set});
}

void ClearBrowserServiceMemStorageCallback(void* context, jobject callback) {
  if (context != nullptr) static_cast<jnivm::VM*>(context)->ClearMemStorageCallback(callback);
}

jobject CreateAsyncMessageBusRequestHandler(void* context, std::shared_ptr<void> callback_context,
                                            void (*run)(void*, JNIEnv*, jstring, jstring)) {
  if (context == nullptr || run == nullptr) return nullptr;
  return static_cast<jnivm::VM*>(context)->CreateMessageBusAsyncRequestHandler(
      std::move(callback_context), jnivm::MessageBusAsyncRequestHandlerCallbacks{run});
}

void ClearAsyncMessageBusRequestHandler(void* context, jobject handler) {
  if (context != nullptr) static_cast<jnivm::VM*>(context)->ClearMessageBusAsyncRequestHandler(handler);
}

struct ExperienceLifecycleTarget {
  std::weak_ptr<mr::RobloxExperienceComposition> composition;
};

void NotifyLuaAppDidReturn(void* context) {
  if (context == nullptr) return;
  const std::shared_ptr<mr::RobloxExperienceComposition> composition =
      static_cast<ExperienceLifecycleTarget*>(context)->composition.lock();
  if (composition != nullptr) composition->NotifyLuaAppDidReturn();
}

mr::GameSessionUpdateResult ExperienceSurfaceCreated(void* context, std::uint64_t generation) {
  return static_cast<mr::RobloxExperienceComposition*>(context)->SurfaceCreated(generation);
}

mr::GameSessionUpdateResult ExperienceSurfaceChanged(void* context, mr::GameSurface surface) {
  return static_cast<mr::RobloxExperienceComposition*>(context)->SurfaceChanged(std::move(surface));
}

mr::GameSessionUpdateResult ExperienceSurfaceDestroyed(void* context, std::uint64_t generation) {
  return static_cast<mr::RobloxExperienceComposition*>(context)->SurfaceDestroyed(generation);
}

struct PresenceState {
  std::mutex mutex;
  std::uint64_t serial = 0;
  mr::RobloxExperiencePresencePhase phase = mr::RobloxExperiencePresencePhase::kBrowsing;
  std::int64_t place_id = 0;
  mr::RobloxExperiencePresenceObserver forward;
};

void RecordPresence(void* context, mr::RobloxExperiencePresencePhase phase,
                    const mr::RobloxExperienceLaunchRequest* request) {
  if (context == nullptr) return;
  auto& state = *static_cast<PresenceState*>(context);
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    ++state.serial;
    state.phase = phase;
    state.place_id = request != nullptr ? request->place_id : 0;
  }
  if (state.forward.valid()) state.forward.notify(state.forward.context, phase, request);
}

const char* JoinState(mr::RobloxExperiencePresencePhase phase) {
  switch (phase) {
    case mr::RobloxExperiencePresencePhase::kJoining:
      return "joining";
    case mr::RobloxExperiencePresencePhase::kPlaying:
      return "in-game";
    case mr::RobloxExperiencePresencePhase::kBrowsing:
      break;
  }
  return "left";
}

}

struct ExperienceSession::Impl {
  jnivm::VM* vm = nullptr;
  mw::ScopedPresentObserver present_observer;
  GameScaling scaling;
  PresenceState presence;
  std::uint64_t presence_reported = 0;
  std::function<void(bool)> in_game_changed;
  std::shared_ptr<mr::ExternalLaunchBroker> link_broker;
  std::size_t waiting_reported = 0;
  bool roblox_started = false;
  std::shared_ptr<mr::RobloxExperienceComposition> composition;
  std::shared_ptr<ExperienceLifecycleTarget> lifecycle_target;
  std::unique_ptr<mw::WindowGameSurfaceBridge> surface_bridge;
  bool subscribed_logged = false;
  bool shut_down = false;
  bool app_destroyed = false;
};

ExperienceSession::ExperienceSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

ExperienceSession::~ExperienceSession() {
  if (impl_ == nullptr) return;
  static_cast<void>(Shutdown());
  impl_->composition.reset();
}

std::unique_ptr<ExperienceSession> ExperienceSession::Create(void* roblox, jnivm::VM* vm,
                                                             const ExperienceSessionOptions& options,
                                                             std::string* unavailable) {
  const auto fail = [unavailable](std::string reason) {
    if (unavailable != nullptr) *unavailable = std::move(reason);
    return std::unique_ptr<ExperienceSession>();
  };
  if (roblox == nullptr || vm == nullptr) return fail("there is no library or pseudo-JVM");

  const mr::RobloxSymbolLookup lookup(&ResolveRobloxSymbol, roblox);
  const mr::RobloxGameSessionResolution game = mr::ResolveRobloxGameSessionSymbols(lookup);
  if (!game.ok()) {
    return fail("the game session's natives are missing; " + MissingNatives(game.missing_required_symbols()));
  }

  std::vector<std::string> missing;
  mr::RobloxExperienceMessageBusSymbols message_bus;
  message_bus.get_launch_id = ResolveNative<mr::GetExperienceLaunchIdFn>(
      roblox, "Java_com_roblox_universalapp_experience_JNIExperienceProtocol_getLaunchId", &missing);
  message_bus.subscribe_raw = ResolveNative<mr::SubscribeExperienceLaunchRawFn>(
      roblox, "Java_com_roblox_universalapp_messagebus_MessageBus_doSubscribeRaw", &missing);
  message_bus.delete_connection = ResolveNative<mr::DeleteMessageBusConnectionFn>(
      roblox, "Java_com_roblox_universalapp_messagebus_Connection_deleteSharedPtr", &missing);
  if (!message_bus.complete()) {
    return fail("ExperienceProtocol's MessageBus natives are missing; " + MissingNatives(missing));
  }

  const mr::RobloxPlatformWebSymbols web =
      mr::ResolveRobloxPlatformWebSymbols(roblox, message_bus.subscribe_raw, message_bus.delete_connection);
  if (!web.complete()) {
    std::string protocols;
    if (!web.web_view.complete()) protocols += " WebViewProtocol";
    if (!web.browser_service.complete()) protocols += " BrowserService (MemStorage)";
    if (!web.permissions.complete()) protocols += " PermissionsProtocol";
    return fail("libroblox.so does not export every native of these platform protocols:" + protocols);
  }

  auto impl = std::make_unique<Impl>();
  impl->vm = vm;
  impl->presence.forward = options.presence;
  impl->in_game_changed = options.in_game_changed;
  const mr::RobloxExperienceJniFactory jni_factory{vm,
                                                   &CreateExperienceRawCallback,
                                                   &ClearExperienceRawCallback,
                                                   &CreateMessageBusRequestHandler,
                                                   &ClearMessageBusRequestHandler,
                                                   &CreateBrowserServiceMemStorageCallback,
                                                   &ClearBrowserServiceMemStorageCallback,
                                                   &mr::SetJnivmPlatformWebCallbacks,
                                                   &mr::ClearJnivmPlatformWebCallbacks,
                                                   &CreateAsyncMessageBusRequestHandler,
                                                   &ClearAsyncMessageBusRequestHandler};
  const mr::RobloxFreshLaunchPresentBoundary present_boundary{
      &impl->present_observer, &RegisterFreshGamePresentObserver, &ClearFreshGamePresentObserver};
  mr::RobloxGameSurfaceJniConfig surface_config;
  surface_config.asset_folder_path = options.asset_folder_path;
  surface_config.dpi_scale = options.dpi_scale > 0.0f ? options.dpi_scale : 1.0f;
  surface_config.is_touch_device = options.touch_enabled;
  surface_config.is_mouse_device = options.mouse_enabled;
  surface_config.is_keyboard_device = options.keyboard_enabled;
  impl->scaling.wanted = Normalized(options.game_render_scale);
  surface_config.scaling_context = &impl->scaling;
  surface_config.game_render_scale = &GameRenderScaleForLaunch;
  surface_config.bind_surface_scale = &BindSurfaceRenderScale;
  surface_config.reset_surface_scales = &ResetSurfaceRenderScales;
  if (!surface_config.complete()) return fail("the game surface's PlatformParams are incomplete (no asset path)");
  const mr::JniEnvironmentProvider environment{vm->GetJavaVM(), vm, &RestoreJniFunctions};
  mr::SecureRobloxCredential web_credential;
  {
    constexpr std::string_view kCanonicalPrefix = ".ROBLOSECURITY=";
    std::string current;
    if (vm->CopyRobloxCredentialFromProvider(&current) && current.size() > kCanonicalPrefix.size() &&
        current.compare(0, kCanonicalPrefix.size(), kCanonicalPrefix) == 0) {
      web_credential = mr::SecureRobloxCredential(std::move(current));
    }
    mr::SecurelyClearString(&current);
  }
  impl->composition = std::make_shared<mr::RobloxExperienceComposition>(
      environment, message_bus, web.web_view, web.browser_service, web.permissions, *game.symbols(), jni_factory,
      present_boundary, std::move(surface_config),
      &web_credential,
      mr::RobloxExperienceSurfaceProvider{}, mr::RobloxExperiencePresenceObserver{&impl->presence, &RecordPresence},
      false, options.microphone_enabled);

  {
    JNIEnv* names_env = nullptr;
    if (environment.Acquire(&names_env).ok() && names_env != nullptr) {
      impl->composition->SetAccountProtocolNames(
          mr::ReadRobloxAccountProtocolNames(names_env, mr::ResolveRobloxAccountProtocolSymbols(roblox)));
    }
  }

  const mocktail::Status status = impl->composition->InitializePlatformProtocols();
  if (!status.ok()) {
    static_cast<void>(impl->composition->Shutdown());
    impl->composition.reset();
    return fail("Roblox's platform protocols did not initialize: " + status.message());
  }
  Log("[experience] platform protocols ready before Roblox's start-up (WebView, BrowserService, Permissions, Call)");

  std::shared_ptr<mr::ExternalLaunchBroker> broker;
  mocktail::Status broker_status =
      mr::ExternalLaunchBroker::StartOwnerAfterLockAcquired(mr::ExternalLaunchBrokerOptions{}, &broker);
  if (broker_status.ok()) broker_status = mr::InstallActiveExternalLaunchBroker(broker);
  if (broker_status.ok()) {
    impl->link_broker = broker;
    AttachLinkJoinBroker(std::move(broker));
  } else {
    if (broker != nullptr) static_cast<void>(broker->Shutdown());
    LogError("[experience] Roblox links cannot join experiences in this session: " + broker_status.message());
  }
  return std::unique_ptr<ExperienceSession>(new ExperienceSession(std::move(impl)));
}

bool ExperienceSession::OnRobloxStarted() {
  Impl& s = *impl_;
  if (s.composition == nullptr || s.shut_down) return true;
  s.roblox_started = true;
  const jnivm::RobloxAuthIdentity identity = s.vm->GetRobloxAuthIdentitySnapshot();
  const mw::WindowSurfaceSnapshot surface = mw::GetWindowSurfaceSnapshot();
  const bool surface_ready = surface.available && surface.generation != 0 && surface.native_window != 0 &&
                             surface.width != 0 && surface.height != 0;
  if (identity.user_id <= 0 || identity.username.empty() || !surface_ready) {
    Log("[experience] launch requests: waiting for the native sign-in");
    return true;
  }
  mr::RobloxLuaAppExperienceReadiness readiness;
  readiness.principal.kind = mr::GameSessionPrincipalKind::kAuthenticated;
  readiness.principal.generation = 1;
  readiness.principal.principal_id = std::to_string(identity.user_id);
  readiness.principal.base_url = kRobloxBaseUrl;
  readiness.surface = {surface.generation, surface.native_window, surface.width, surface.height, surface.dpi_scale};
  readiness.username = identity.username;
  const mocktail::Status status = s.composition->OnLuaAppReady(std::move(readiness));
  if (!status.ok()) {
    LogError("[experience] subscribing to launch requests failed: " + status.message());
    return false;
  }
  mw::WindowSurfaceEvent stale;
  while (mw::PollWindowSurfaceEvent(&stale)) {
  }
  s.lifecycle_target = std::make_shared<ExperienceLifecycleTarget>(ExperienceLifecycleTarget{s.composition});
  s.vm->SetRobloxExperienceLifecycleCallbacks(s.lifecycle_target,
                                              jnivm::RobloxExperienceLifecycleCallbacks{&NotifyLuaAppDidReturn});
  const mw::WindowGameSurfaceConsumer consumer{s.composition.get(), &ExperienceSurfaceCreated,
                                               &ExperienceSurfaceChanged, &ExperienceSurfaceDestroyed};
  s.surface_bridge = std::make_unique<mw::WindowGameSurfaceBridge>(
      mw::MakeWindowGameSurfaceEventSource(), consumer, mw::MakeWindowResizeReadinessCommitObserver());
  s.subscribed_logged = true;
  Log("[experience] launch requests: subscribed (the account was known when Roblox's start-up returned)");
  return true;
}

bool ExperienceSession::DrainSurfaceEvents() {
  Impl& s = *impl_;
  if (s.surface_bridge == nullptr || s.shut_down) return true;
  const mw::WindowGameSurfaceDrainResult result = s.surface_bridge->Drain();
  if (!result.ok()) {
    LogError("[experience] Roblox rejected a window surface change: " + result.status.message());
    return false;
  }
  if (result.drained_events != 0) {
    Log("[experience] window surface events committed=" + std::to_string(result.drained_events) +
        " last_generation=" + std::to_string(s.surface_bridge->Snapshot().last_generation));
  }
  return true;
}

bool ExperienceSession::DrainPlatformEvents() {
  Impl& s = *impl_;
  if (s.composition == nullptr || s.shut_down) return true;
#if defined(ELIXIR_RUNTIME_HAS_VULKAN)
  if (s.scaling.applied && vulkan::PresentStretchState() == vulkan::PresentStretch::kUnsupported &&
      s.composition->DropGameRenderScale()) {
    s.scaling.applied = false;
    Log("[experience] resolution scale: the graphics driver did not take the smaller picture, so this game "
        "renders at the window's size");
  }
#endif
  const mocktail::Status status = s.composition->DrainPlatformEvents();
  if (!status.ok()) {
    LogError("[experience] Roblox's platform events failed: " + status.message());
    return false;
  }
  if (!s.subscribed_logged && s.composition->subscribed()) {
    s.subscribed_logged = true;
    Log("[experience] launch requests: subscribed after the native sign-in");
  }
  return true;
}

bool ExperienceSession::DrainLaunchRequests() {
  Impl& s = *impl_;
  if (s.composition == nullptr || s.shut_down) return true;
  ReportPresence();
  if (!s.composition->subscribed()) {
    const std::size_t waiting = s.link_broker != nullptr ? s.link_broker->pending_launch_count() : 0;
    if (waiting == 0) {
      s.waiting_reported = 0;
    } else if (s.roblox_started && waiting > s.waiting_reported) {
      s.waiting_reported = waiting;
      Log("[experience] a Roblox link's join waits for the native sign-in");
      EmitEvent(3, "join", "waiting-for-sign-in");
    }
    return true;
  }
  s.waiting_reported = 0;
  const mocktail::Status status = s.composition->DrainLaunchRequests();
  ReportPresence();
  if (!status.ok()) {
    LogError("[experience] joining the experience failed: " + status.message());
    return false;
  }
  return true;
}

void ExperienceSession::ReportPresence() {
  Impl& s = *impl_;
  mr::RobloxExperiencePresencePhase phase;
  {
    std::lock_guard<std::mutex> lock(s.presence.mutex);
    if (s.presence.serial == s.presence_reported) return;
    s.presence_reported = s.presence.serial;
    phase = s.presence.phase;
  }
  EmitEvent(3, "join", JoinState(phase));
  if (s.in_game_changed) s.in_game_changed(phase != mr::RobloxExperiencePresencePhase::kBrowsing);
}

bool ExperienceSession::subscribed() const {
  return impl_->composition != nullptr && !impl_->shut_down && impl_->composition->subscribed();
}

bool ExperienceSession::Shutdown() {
  Impl& s = *impl_;
  if (s.shut_down) return s.app_destroyed;
  s.shut_down = true;
  if (s.composition == nullptr) return false;
  s.vm->ClearRobloxExperienceLifecycleCallbacks();
  s.lifecycle_target.reset();
  s.surface_bridge.reset();
  if (s.link_broker != nullptr) {
    DetachLinkJoinBroker();
    mr::ClearActiveExternalLaunchBroker(s.link_broker.get());
    static_cast<void>(s.link_broker->Shutdown());
    s.link_broker.reset();
  }
  const mr::GameSessionSnapshot snapshot = s.composition->Snapshot();
  s.app_destroyed = snapshot.game_running || snapshot.game_paused || snapshot.game_present_pending;
  const mocktail::Status status = s.composition->Shutdown();
  if (!status.ok()) LogError("[experience] shutdown: " + status.message());
  Log(s.app_destroyed ? "[experience] shut down; the running game was left and Roblox's app shut down with it"
                      : "[experience] shut down");
  return s.app_destroyed;
}

}
