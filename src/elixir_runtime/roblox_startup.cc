// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail src/legacy/legacy_runtime.cc (Apache-2.0, Copyright 2026
// Mocktail Project Authors). Modified by Elixir.
#include "elixir_runtime/roblox_startup.h"

#include <windows.h>
#undef RegisterClass

#include <jni.h>
#include <mcpelauncher/linker.h>

#include <filesystem>
#include <memory>
#include <string_view>
#include <system_error>

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "abi/sysv_abi.h"
#include "elixir_runtime/guest_environment.h"
#include "elixir_runtime/host_log.h"
#include "elixir_runtime/process_tuning.h"
#include "elixir_runtime/text.h"
#include "jnivm/jnivm.h"
#include "platform/windows/guest_tls.h"
#include "runtime/device_memory_profile.h"

#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
#include "android/android.h"
#include "android/media_decoder.h"
#endif

namespace elixir::runtime {
namespace {

constexpr SIZE_T kEngineStackReserve = 1024ULL * 1024 * 1024;

using NoArgFn = abi::SysvFn<void(JNIEnv*, jclass)>;
using StringFn = abi::SysvFn<void(JNIEnv*, jclass, jstring)>;
using TwoStringFn = abi::SysvFn<void(JNIEnv*, jclass, jstring, jstring)>;
using ObjectFn = abi::SysvFn<void(JNIEnv*, jclass, jobject)>;
using InitStorageManagerFn = abi::SysvFn<void(JNIEnv*, jobject, jobject, jstring, jstring)>;
using InitClientSettingsFn = abi::SysvFn<jint(JNIEnv*, jclass, jstring, jstring, jstring)>;
using FloatFn = abi::SysvFn<void(JNIEnv*, jclass, jfloat)>;
using FloatArrayFn = abi::SysvFn<void(JNIEnv*, jclass, jfloatArray)>;
using BackgroundModeFn = abi::SysvFn<void(JNIEnv*, jclass, jboolean, jstring)>;

struct Symbols {
  StringFn set_asset_path = nullptr;
  StringFn set_exception_reason_filename = nullptr;
  TwoStringFn set_base_url = nullptr;
  StringFn set_roblox_channel = nullptr;
  StringFn override_channel_platform_name = nullptr;
  StringFn set_roblox_version = nullptr;
  ObjectFn set_device_info = nullptr;
  TwoStringFn set_base_data_directories = nullptr;
  StringFn set_cache_directory = nullptr;
  StringFn set_files_directory = nullptr;
  StringFn set_external_directory = nullptr;
  StringFn set_preferences_file = nullptr;
  StringFn set_default_app_policy_file = nullptr;
  NoArgFn init_fast_log = nullptr;
  ObjectFn init_asset_manager = nullptr;
  InitStorageManagerFn init_storage_manager = nullptr;
  ObjectFn base_url_protocol_init = nullptr;
  NoArgFn game_global_init = nullptr;
  NoArgFn update_adapter_init = nullptr;
  InitClientSettingsFn init_client_settings = nullptr;
  ObjectFn post_client_settings = nullptr;
  ObjectFn init_with_params = nullptr;
  jnivm::RobloxCookieGetter get_cookies_for_domain = nullptr;
  TwoStringFn set_multiple_cookies = nullptr;
  NoArgFn set_active = nullptr;
  FloatFn pass_refresh_rate = nullptr;
  FloatArrayFn pass_refresh_rates = nullptr;
  NoArgFn start_lua_app_dm = nullptr;
  BackgroundModeFn set_background_mode = nullptr;
  ObjectFn start_app_with_params = nullptr;
};

template <typename Fn>
void Resolve(void* handle, const char* name, bool required, Fn* out, std::vector<std::string>* missing) {
  *out = reinterpret_cast<Fn>(linker::dlsym(handle, name));
  if (*out == nullptr && required) missing->push_back(name);
}

Symbols ResolveSymbols(void* handle, bool windowed, std::vector<std::string>* missing) {
  Symbols s;
  Resolve(handle, "Java_com_roblox_client_startup_MainGameActivity_nativeSetAssetPath", true, &s.set_asset_path,
          missing);
  constexpr const char* kSettings = "Java_com_roblox_engine_jni_NativeSettingsInterface_";
  const auto settings = [&](const char* method, bool required, auto* out) {
    Resolve(handle, (std::string(kSettings) + method).c_str(), required, out, missing);
  };
  settings("nativeSetExceptionReasonFilename", true, &s.set_exception_reason_filename);
  settings("nativeSetBaseUrl", true, &s.set_base_url);
  settings("nativeSetRobloxChannel", true, &s.set_roblox_channel);
  settings("nativeOverrideChannelPlatformName", false, &s.override_channel_platform_name);
  settings("nativeSetRobloxVersion", false, &s.set_roblox_version);
  settings("nativeSetDeviceInfo", true, &s.set_device_info);
  settings("nativeSetBaseDataDirectories", true, &s.set_base_data_directories);
  settings("nativeSetCacheDirectory", true, &s.set_cache_directory);
  settings("nativeSetFilesDirectory", true, &s.set_files_directory);
  settings("nativeSetExternalDirectory", true, &s.set_external_directory);
  settings("nativeSetPreferencesFile", true, &s.set_preferences_file);
  settings("nativeSetDefaultAppPolicyFile", true, &s.set_default_app_policy_file);
  settings("nativeInitFastLog", true, &s.init_fast_log);
  settings("nativeGetCookiesForDomain", false, &s.get_cookies_for_domain);
  settings("nativeSetMultipleCookies", false, &s.set_multiple_cookies);
  Resolve(handle, "Java_com_roblox_client_JNIAAssetManagerSetup_initNative", false, &s.init_asset_manager, missing);
  Resolve(handle, "Java_com_roblox_client_LocalStorageManager_initStorageManagerNativeV3", true,
          &s.init_storage_manager, missing);
  Resolve(handle, "Java_com_roblox_universalapp_linking_JNIBaseUrlProtocol_init", false, &s.base_url_protocol_init,
          missing);
  constexpr const char* kGl = "Java_com_roblox_engine_jni_NativeGLInterface_";
  const auto gl = [&](const char* method, auto* out) {
    Resolve(handle, (std::string(kGl) + method).c_str(), true, out, missing);
  };
  gl("nativeGameGlobalInit", &s.game_global_init);
  gl("nativeUpdateAdapterInit", &s.update_adapter_init);
  gl("nativeInitClientSettings", &s.init_client_settings);
  gl("nativePostClientSettingsLoadedInitialization3", &s.post_client_settings);
  gl("nativeAppBridgeV2InitWithParams", &s.init_with_params);
  const auto gl_windowed = [&](const char* method, bool required, auto* out) {
    Resolve(handle, (std::string(kGl) + method).c_str(), windowed && required, out, missing);
  };
  Resolve(handle, "Java_com_roblox_universalapp_applifecyclenativeadapter_JNIAppLifecycleNativeAdapter_setActive",
          false, &s.set_active, missing);
  gl_windowed("nativePassCurrentDisplayRefreshRate", false, &s.pass_refresh_rate);
  gl_windowed("nativePassSupportedRefreshRates", false, &s.pass_refresh_rates);
  gl_windowed("nativeAppBridgeStartLuaAppDM", true, &s.start_lua_app_dm);
  gl_windowed("setTaskSchedulerBackgroundMode", true, &s.set_background_mode);
  gl_windowed("nativeAppBridgeV2StartAppWithParams", true, &s.start_app_with_params);
  return s;
}

struct MainThreadWork {
  std::mutex mutex;
  std::condition_variable done;
  std::function<void()> task;
  bool pending = false;
  bool finished = false;
};

MainThreadWork& Work() {
  static MainThreadWork* const work = new MainThreadWork();
  return *work;
}

bool RunOnMainThread(std::function<void()> task, unsigned timeout_ms) {
  MainThreadWork& work = Work();
  std::unique_lock<std::mutex> lock(work.mutex);
  work.task = std::move(task);
  work.pending = true;
  work.finished = false;
  if (work.done.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] { return work.finished; })) return true;
  work.pending = false;
  work.task = nullptr;
  return false;
}

jobject NewObject(JNIEnv* env, const char* class_name) {
  jclass clazz = env->FindClass(class_name);
  return clazz != nullptr ? env->AllocObject(clazz) : nullptr;
}

void SetObjectField(JNIEnv* env, jobject object, const char* name, const char* signature, jobject value) {
  if (object == nullptr) return;
  env->SetObjectField(object, env->GetFieldID(env->GetObjectClass(object), name, signature), value);
}

void SetStringField(JNIEnv* env, jobject object, const char* name, const std::string& value) {
  SetObjectField(env, object, name, "Ljava/lang/String;", env->NewStringUTF(value.c_str()));
}

void SetJStringField(JNIEnv* env, jobject object, const char* name, jstring value) {
  SetObjectField(env, object, name, "Ljava/lang/String;", value);
}

void SetIntField(JNIEnv* env, jobject object, const char* name, jint value) {
  if (object == nullptr) return;
  env->SetIntField(object, env->GetFieldID(env->GetObjectClass(object), name, "I"), value);
}

void SetLongField(JNIEnv* env, jobject object, const char* name, jlong value) {
  if (object == nullptr) return;
  env->SetLongField(object, env->GetFieldID(env->GetObjectClass(object), name, "J"), value);
}

void SetFloatField(JNIEnv* env, jobject object, const char* name, jfloat value) {
  if (object == nullptr) return;
  env->SetFloatField(object, env->GetFieldID(env->GetObjectClass(object), name, "F"), value);
}

void SetBooleanField(JNIEnv* env, jobject object, const char* name, bool value) {
  if (object == nullptr) return;
  env->SetBooleanField(object, env->GetFieldID(env->GetObjectClass(object), name, "Z"), value ? JNI_TRUE : JNI_FALSE);
}

#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
jobjectArray QueryVideoCodecs(JNIEnv* env) {
  bool available = false;
  try {
    available = android::media::AvcDecoder::Create() != nullptr;
  } catch (...) {
  }
  if (env->PushLocalFrame(64) != JNI_OK) return nullptr;
  jclass capability = env->FindClass("com/roblox/engine/jni/video/VideoCodecCapability");
  jobjectArray list = env->NewObjectArray(available ? 1 : 0, capability, nullptr);
  if (available && list != nullptr) {
    jobject info = env->AllocObject(capability);
    SetStringField(env, info, "name", android::media::kAvcDecoderName);
    SetStringField(env, info, "codec", "AVC");
    SetBooleanField(env, info, "isEncoder", false);
    SetBooleanField(env, info, "isHardware", false);
    SetIntField(env, info, "minWidth", 48);
    SetIntField(env, info, "minHeight", 48);
    SetIntField(env, info, "maxWidth", 4096);
    SetIntField(env, info, "maxHeight", 2304);
    jclass string_type = env->FindClass("java/lang/String");
    const char* const names[] = {"AVCProfileBaseline", "AVCProfileMain", "AVCProfileHigh"};
    jobjectArray profiles = env->NewObjectArray(3, string_type, nullptr);
    jobjectArray levels = env->NewObjectArray(3, string_type, env->NewStringUTF("AVCLevel51"));
    for (jsize i = 0; i < 3; ++i) env->SetObjectArrayElement(profiles, i, env->NewStringUTF(names[i]));
    SetObjectField(env, info, "profiles", "[Ljava/lang/String;", profiles);
    SetObjectField(env, info, "levels", "[Ljava/lang/String;", levels);
    env->SetObjectArrayElement(list, 0, info);
  }
  return static_cast<jobjectArray>(env->PopLocalFrame(list));
}
#endif

void SetRobloxServiceUrlFields(JNIEnv* env, jobject object) {
  constexpr const char* kBaseUrl = "https://www.roblox.com";
  constexpr const char* kApiUrl = "https://apis.roblox.com";
  constexpr const char* kClientSettingsUrl =
      "https://clientsettingscdn.roblox.com/v2/settings-compressed/application/AndroidApp.zst";
  constexpr const char* kClientSettingsBaseUrl = "https://clientsettingscdn.roblox.com";
  constexpr const char* kEcsv2ClientUrl = "https://ecsv2.roblox.com/client/pbe";
  constexpr const char* kEcsv2TimespentUrl = "https://ecsv2.roblox.com/timespent/pbe";
  constexpr const char* kTelemetryUrl = "https://apis.roblox.com/experience-signals-ingest/public/v1/events/single";
  SetStringField(env, object, "baseUrl", kBaseUrl);
  SetStringField(env, object, "baseURL", kBaseUrl);
  SetStringField(env, object, "wwwBaseUrl", kBaseUrl);
  SetStringField(env, object, "apiBaseUrl", kApiUrl);
  SetStringField(env, object, "apiGatewayUrl", kApiUrl);
  SetStringField(env, object, "clientSettingsUrl", kClientSettingsUrl);
  SetStringField(env, object, "settingsUrl", kClientSettingsUrl);
  SetStringField(env, object, "clientSettingsBaseUrl", kClientSettingsBaseUrl);
  SetStringField(env, object, "clientSettingsHost", kClientSettingsBaseUrl);
  SetStringField(env, object, "ecsv2Url", kEcsv2ClientUrl);
  SetStringField(env, object, "ecsUrl", kEcsv2ClientUrl);
  SetStringField(env, object, "eventStreamUrl", kEcsv2ClientUrl);
  SetStringField(env, object, "telemetryUrl", kTelemetryUrl);
  SetStringField(env, object, "telemetryEndpoint", kTelemetryUrl);
  SetStringField(env, object, "timeSpentUrl", kEcsv2TimespentUrl);
}

std::string ResolutionText(const mocktail::runtime::DisplaySize& size) {
  return std::to_string(size.width) + "x" + std::to_string(size.height);
}

jobject BuildDeviceParams(JNIEnv* env, const RobloxStartupOptions& options) {
  jobject params = NewObject(env, "com/roblox/engine/jni/model/DeviceParams");
  if (params == nullptr) return nullptr;
  const mocktail::runtime::DeviceProfile& device = options.device;
  SetStringField(env, params, "osVersion", "33");
  SetStringField(env, params, "deviceName", device.display_name);
  SetStringField(env, params, "appVersion", options.version_name);
  SetStringField(env, params, "country", "US");
  SetStringField(env, params, "manufacturer", device.manufacturer);
  SetStringField(env, params, "displayResolution", ResolutionText(options.display));
  SetStringField(env, params, "networkType", "wifi");
  SetStringField(env, params, "deviceSku", device.device_sku);
  SetStringField(env, params, "socModel", device.soc_model);
  SetStringField(env, params, "appBuildVariant", "googleProdRelease");
  SetStringField(env, params, "testDeviceName", device.display_name);
  const mocktail::runtime::DeviceMemoryProfile memory =
      mocktail::runtime::BuildDeviceMemoryProfile(options.host_memory_bytes);
  SetIntField(env, params, "deviceTotalMemoryMB", memory.total_memory_mb);
  SetIntField(env, params, "displayPhysicalWidthPixels", options.display.width);
  SetIntField(env, params, "displayPhysicalHeightPixels", options.display.height);
  SetIntField(env, params, "memoryClass", memory.memory_class_mb);
  SetIntField(env, params, "largeMemoryClass", memory.large_memory_class_mb);
  SetLongField(env, params, "lowMemoryKillerBackgroundAppThreshold", memory.low_memory_killer_background_threshold);
  SetLongField(env, params, "lowMemoryKillerForegroundAppThreshold", memory.low_memory_killer_foreground_threshold);
  SetBooleanField(env, params, "cpu64Bit", true);
  SetBooleanField(env, params, "isChrome", false);
  SetBooleanField(env, params, "isLowRamDevice", memory.low_ram_device);
  return params;
}

jobject BuildPlatformParams(JNIEnv* env, jobject surface, const RobloxStartupOptions& options) {
  jobject params = NewObject(env, "com/roblox/engine/jni/model/PlatformParams");
  if (params == nullptr) return nullptr;
  SetObjectField(env, params, "surface", "Landroid/view/Surface;", surface);
  SetStringField(env, params, "platform", "Android");
  SetStringField(env, params, "deviceName", options.device.display_name);
  SetStringField(env, params, "locale", "en_us");
  SetStringField(env, params, "assetFolderPath", options.asset_path);
  SetIntField(env, params, "width", options.window.width);
  SetIntField(env, params, "height", options.window.height);
  SetIntField(env, params, "screenWidth", options.display.width);
  SetIntField(env, params, "screenHeight", options.display.height);
  SetIntField(env, params, "densityDpi", 160);
  SetIntField(env, params, "viewportWidthMm", mocktail::runtime::PixelsToMillimetersAt160Dpi(options.window.width));
  SetIntField(env, params, "viewportHeightMm", mocktail::runtime::PixelsToMillimetersAt160Dpi(options.window.height));
  SetFloatField(env, params, "dpiScale", options.dpi_scale);
  SetBooleanField(env, params, "isKeyboardDevice", options.input.keyboard_enabled);
  SetBooleanField(env, params, "isMouseDevice", options.input.mouse_enabled);
  SetBooleanField(env, params, "isTouchDevice", options.input.touch_enabled);
  SetBooleanField(env, params, "isLuaHomePageEnabled", true);
  SetBooleanField(env, params, "isLuaGamesPageEnabled", true);
  SetBooleanField(env, params, "isLuaChatEnabled", true);
  SetBooleanField(env, params, "isTablet", false);
  SetBooleanField(env, params, "headless", options.headless);
  SetBooleanField(env, params, "isHeadless", options.headless);
  return params;
}

jobject BuildAppBridgeInitParams(JNIEnv* env, const RobloxStartupOptions& options, jstring client_settings,
                                 jstring fast_flags, jstring app_params, jstring asset_path) {
  jobject params = NewObject(env, "com/roblox/engine/jni/autovalue/InitParams");
  if (params == nullptr) return nullptr;
  jobject activity = NewObject(env, "com/roblox/client/RobloxActivity");
  jobject context = NewObject(env, "android/content/Context");
  jobject asset_manager = NewObject(env, "android/content/res/AssetManager");
  jobject resources = NewObject(env, "android/content/res/Resources");
  jobject class_loader = NewObject(env, "java/lang/ClassLoader");
  jobject package_manager = NewObject(env, "android/content/pm/PackageManager");
  jobject window = NewObject(env, "android/view/Window");
  jobject display = NewObject(env, "android/view/Display");
  jobject surface = NewObject(env, "android/view/Surface");
  jobject platform_params = BuildPlatformParams(env, surface, options);
  jobject surface_holder = NewObject(env, "android/view/SurfaceHolder");
  jobject view = NewObject(env, "android/view/View");

  SetObjectField(env, params, "platformParams", "Lcom/roblox/engine/jni/model/PlatformParams;", platform_params);
  SetObjectField(env, params, "deviceParams", "Lcom/roblox/engine/jni/model/DeviceParams;", nullptr);
  SetStringField(env, params, "baseURL", "https://www.roblox.com");
  SetStringField(env, params, "userAgent", options.user_agent);
  SetBooleanField(env, params, "isTablet", false);
  SetBooleanField(env, params, "isPotato", false);
  SetBooleanField(env, params, "isVrDevice", false);
  SetStringField(env, params, "buildVariant", "googleProdRelease");
  SetObjectField(env, params, "vrContext", "Landroid/app/Activity;", activity);
  SetObjectField(env, params, "activity", "Lcom/roblox/client/RobloxActivity;", activity);
  SetObjectField(env, params, "robloxActivity", "Lcom/roblox/client/RobloxActivity;", activity);
  SetObjectField(env, params, "mainActivity", "Lcom/roblox/client/RobloxActivity;", activity);
  SetObjectField(env, params, "context", "Landroid/content/Context;", context);
  SetObjectField(env, params, "applicationContext", "Landroid/content/Context;", context);
  SetObjectField(env, params, "assetManager", "Landroid/content/res/AssetManager;", asset_manager);
  SetObjectField(env, params, "assets", "Landroid/content/res/AssetManager;", asset_manager);
  SetObjectField(env, params, "resources", "Landroid/content/res/Resources;", resources);
  SetObjectField(env, params, "classLoader", "Ljava/lang/ClassLoader;", class_loader);
  SetObjectField(env, params, "packageManager", "Landroid/content/pm/PackageManager;", package_manager);
  SetObjectField(env, params, "window", "Landroid/view/Window;", window);
  SetObjectField(env, params, "display", "Landroid/view/Display;", display);
  SetObjectField(env, params, "surface", "Landroid/view/Surface;", surface);
  SetObjectField(env, params, "surfaceHolder", "Landroid/view/SurfaceHolder;", surface_holder);
  SetObjectField(env, params, "view", "Landroid/view/View;", view);
  SetObjectField(env, params, "decorView", "Landroid/view/View;", view);
  SetObjectField(env, params, "rootView", "Landroid/view/View;", view);
  SetJStringField(env, params, "clientSettingsJson", client_settings);
  SetJStringField(env, params, "clientSettings", client_settings);
  SetJStringField(env, params, "fastFlagsJson", fast_flags);
  SetJStringField(env, params, "fastFlags", fast_flags);
  SetJStringField(env, params, "appParamsJson", app_params);
  SetJStringField(env, params, "appParams", app_params);
  SetJStringField(env, params, "launchParams", app_params);
  SetJStringField(env, params, "assetPath", asset_path);
  SetStringField(env, params, "packageName", options.package_name);
  SetStringField(env, params, "platformName", "Android");
  SetStringField(env, params, "appVersion", options.version_name);
  SetStringField(env, params, "deviceName", options.device.display_name);
  SetStringField(env, params, "locale", "en_us");
  SetRobloxServiceUrlFields(env, params);
  SetIntField(env, params, "screenWidth", options.display.width);
  SetIntField(env, params, "screenHeight", options.display.height);
  SetIntField(env, params, "densityDpi", 160);
  SetIntField(env, params, "sdkVersion", 33);
  SetBooleanField(env, params, "headless", options.headless);
  SetBooleanField(env, params, "isHeadless", options.headless);
  SetBooleanField(env, params, "isFirstInstall", false);
  SetBooleanField(env, params, "isLowMemoryDevice", false);
  return params;
}

jobject BuildApplicationExitInfoList(JNIEnv* env) {
  jobject list = NewObject(env, "java/util/ArrayList");
  return list != nullptr ? list : NewObject(env, "java/util/List");
}

jobject BuildSurface(JNIEnv* env, const RobloxStartupOptions& options) {
  jobject surface = NewObject(env, "android/view/Surface");
  if (surface == nullptr) return nullptr;
  SetIntField(env, surface, "width", options.window.width);
  SetIntField(env, surface, "height", options.window.height);
  SetBooleanField(env, surface, "valid", true);
  SetBooleanField(env, surface, "isValid", true);
  return surface;
}

jobject BuildStartAppParams(JNIEnv* env, const RobloxStartupOptions& options, jstring app_params,
                            jobject platform_params, jobject surface) {
  jobject params = NewObject(env, "com/roblox/engine/jni/autovalue/StartAppParams");
  if (params == nullptr) return nullptr;
  jobject activity = NewObject(env, "com/roblox/client/RobloxActivity");
  jobject context = NewObject(env, "android/content/Context");
  jobject asset_manager = NewObject(env, "android/content/res/AssetManager");
  jobject resources = NewObject(env, "android/content/res/Resources");
  jobject class_loader = NewObject(env, "java/lang/ClassLoader");
  jobject package_manager = NewObject(env, "android/content/pm/PackageManager");
  jobject window = NewObject(env, "android/view/Window");
  jobject display = NewObject(env, "android/view/Display");
  jobject surface_holder = NewObject(env, "android/view/SurfaceHolder");
  jobject view = NewObject(env, "android/view/View");

  SetObjectField(env, params, "surface", "Landroid/view/Surface;", surface);
  SetObjectField(env, params, "platformParams", "Lcom/roblox/engine/jni/model/PlatformParams;", platform_params);
  SetStringField(env, params, "appStarterPlace", "rbxasset://places/Mobile.rbxl");
  SetStringField(env, params, "appStarterScript", "LuaAppStarterScript");
  SetLongField(env, params, "appUserId", 0);
  SetBooleanField(env, params, "isUnder13", false);
  SetStringField(env, params, "username", "");
  SetIntField(env, params, "membershipType", 0);
  SetStringField(env, params, "selectedTheme", options.theme);
  SetObjectField(env, params, "vrContext", "Landroid/app/Activity;", activity);
  SetJStringField(env, params, "appParams", app_params);
  SetJStringField(env, params, "appParamsJson", app_params);
  SetJStringField(env, params, "launchParams", app_params);
  SetObjectField(env, params, "activity", "Lcom/roblox/client/RobloxActivity;", activity);
  SetObjectField(env, params, "robloxActivity", "Lcom/roblox/client/RobloxActivity;", activity);
  SetObjectField(env, params, "mainActivity", "Lcom/roblox/client/RobloxActivity;", activity);
  SetObjectField(env, params, "context", "Landroid/content/Context;", context);
  SetObjectField(env, params, "applicationContext", "Landroid/content/Context;", context);
  SetObjectField(env, params, "assetManager", "Landroid/content/res/AssetManager;", asset_manager);
  SetObjectField(env, params, "assets", "Landroid/content/res/AssetManager;", asset_manager);
  SetObjectField(env, params, "resources", "Landroid/content/res/Resources;", resources);
  SetObjectField(env, params, "classLoader", "Ljava/lang/ClassLoader;", class_loader);
  SetObjectField(env, params, "packageManager", "Landroid/content/pm/PackageManager;", package_manager);
  SetObjectField(env, params, "window", "Landroid/view/Window;", window);
  SetObjectField(env, params, "display", "Landroid/view/Display;", display);
  SetObjectField(env, params, "surfaceHolder", "Landroid/view/SurfaceHolder;", surface_holder);
  SetObjectField(env, params, "view", "Landroid/view/View;", view);
  SetObjectField(env, params, "decorView", "Landroid/view/View;", view);
  SetObjectField(env, params, "rootView", "Landroid/view/View;", view);
  SetStringField(env, params, "launchMode", options.headless ? "headless" : "normal");
  SetStringField(env, params, "joinData", "{}");
  SetStringField(env, params, "packageName", options.package_name);
  SetStringField(env, params, "platformName", "Android");
  SetStringField(env, params, "appVersion", options.version_name);
  SetStringField(env, params, "deviceName", options.device.display_name);
  SetStringField(env, params, "locale", "en_us");
  SetRobloxServiceUrlFields(env, params);
  SetIntField(env, params, "width", options.window.width);
  SetIntField(env, params, "height", options.window.height);
  SetIntField(env, params, "screenWidth", options.display.width);
  SetIntField(env, params, "screenHeight", options.display.height);
  SetIntField(env, params, "densityDpi", 160);
  SetIntField(env, params, "sdkVersion", 33);
  SetIntField(env, params, "placeId", 0);
  SetBooleanField(env, params, "headless", options.headless);
  SetBooleanField(env, params, "isHeadless", options.headless);
  SetBooleanField(env, params, "isFirstInstall", false);
  SetBooleanField(env, params, "isLowMemoryDevice", false);
  return params;
}

struct Shared {
  void* roblox = nullptr;
  jnivm::VM* vm = nullptr;
  RobloxStartupOptions options;
  Symbols symbols;
  const std::atomic<bool>* stop = nullptr;
  bool main_thread_pumps = false;
  std::atomic<const char*> current{"thread start"};
  RobloxStartupResult result;
};

class Sequence {
 public:
  Sequence(Shared* shared, JNIEnv* env) : shared_(shared), s_(shared->symbols), o_(shared->options), env_(env) {}

  void Run() {
    JNIEnv* env = env_;
    native_gl_ = env->FindClass("com/roblox/engine/jni/NativeGLInterface");
    settings_ = env->FindClass("com/roblox/engine/jni/NativeSettingsInterface");
    native_gl_java_ = env->FindClass("com/roblox/engine/jni/NativeGLJavaInterface");
    startup_activity_ = env->FindClass("com/roblox/client/startup/MainGameActivity");
    game_activity_ = NewObject(env, "com/roblox/client/startup/MainGameActivity");
    listener_ = NewObject(env, "com/roblox/engine/jni/OnAppBridgeNotificationListener");
    asset_path_ = env->NewStringUTF(o_.asset_path.c_str());
    client_settings_ = env->NewStringUTF(o_.client_settings_json.c_str());
    overrides_ = env->NewStringUTF(o_.client_settings_overrides_json.c_str());
    group_ = env->NewStringUTF("GoogleAndroidApp");
    fast_flags_ = env->NewStringUTF(o_.fast_flags_json.c_str());
    const std::string app_params = std::string("{\"launchMode\":\"") + (o_.headless ? "headless" : "normal") +
                                   "\",\"placeId\":0}";
    app_params_ = env->NewStringUTF(app_params.c_str());

    if (!Step("NativeGLJavaInterface.setImplementation", [&] { SetStaticListener("setImplementation"); })) return;
    if (!Step("MainGameActivity.nativeSetAssetPath",
              [&] { s_.set_asset_path(env, startup_activity_, asset_path_); })) {
      return;
    }
    if (!Step("NativeSettings", [&] { ConfigureNativeSettings(); })) return;
    if (!Step("LocalStorage", [&] { ConfigureLocalStorage(); })) return;
    if (s_.base_url_protocol_init != nullptr &&
        !Step("JNIBaseUrlProtocol.init", [&] {
          s_.base_url_protocol_init(env, env->FindClass("com/roblox/universalapp/linking/JNIBaseUrlProtocol"),
                                    game_activity_);
        })) {
      return;
    }
    if (!Step("NativeGLInterface.nativeGameGlobalInit", [&] { s_.game_global_init(env, native_gl_); })) return;
    if (!Step("NativeGLInterface.nativeUpdateAdapterInit", [&] { s_.update_adapter_init(env, native_gl_); })) {
      return;
    }
    if (!Step("NativeGLInterface.nativeInitClientSettings", [&] {
          const jint result = s_.init_client_settings(env, native_gl_, client_settings_, overrides_, group_);
          Log("Roblox start-up: nativeInitClientSettings returned " + std::to_string(result));
        })) {
      return;
    }
    if (!Step("NativeSettings (again, after the client settings)", [&] { ConfigureNativeSettings(); })) return;
    if (!Step("NativeGLInterface.nativePostClientSettingsLoadedInitialization3",
              [&] { s_.post_client_settings(env, native_gl_, BuildApplicationExitInfoList(env)); })) {
      return;
    }
    if (!Step("NativeGLJavaInterface.setAppBridgeNotificationListener",
              [&] { SetStaticListener("setAppBridgeNotificationListener"); })) {
      return;
    }
    if (!Step("NativeGLInterface.nativeAppBridgeV2InitWithParams", [&] {
          s_.init_with_params(env, native_gl_,
                              BuildAppBridgeInitParams(env, o_, client_settings_, fast_flags_, app_params_,
                                                       asset_path_));
        })) {
      return;
    }
    if (o_.native_window != nullptr && !RunWindowedSteps()) return;
    shared_->result.ok = true;
  }

 private:
  bool RunWindowedSteps() {
    JNIEnv* env = env_;
    if (s_.set_active != nullptr &&
        !Step("JNIAppLifecycleNativeAdapter.setActive", [&] { s_.set_active(env, native_gl_); })) {
      return false;
    }
    if (o_.refresh_hz > 0.0f && s_.pass_refresh_rate != nullptr &&
        !Step("NativeGLInterface.nativePassCurrentDisplayRefreshRate",
              [&] { s_.pass_refresh_rate(env, native_gl_, o_.refresh_hz); })) {
      return false;
    }
    if (!o_.supported_refresh_hz.empty() && s_.pass_refresh_rates != nullptr &&
        !Step("NativeGLInterface.nativePassSupportedRefreshRates", [&] {
          const jsize count = static_cast<jsize>(o_.supported_refresh_hz.size());
          jfloatArray rates = env->NewFloatArray(count);
          if (rates == nullptr) return;
          env->SetFloatArrayRegion(rates, 0, count, o_.supported_refresh_hz.data());
          s_.pass_refresh_rates(env, native_gl_, rates);
        })) {
      return false;
    }
    if (!Step("NativeGLInterface.nativeAppBridgeStartLuaAppDM", [&] { s_.start_lua_app_dm(env, native_gl_); })) {
      return false;
    }
    if (!Step("NativeGLInterface.setTaskSchedulerBackgroundMode(false, \"ASMA.start\")", [&] {
          jnivm::VM* vm = shared_->vm;
          const BackgroundModeFn set_mode = s_.set_background_mode;
          jclass native_gl = native_gl_;
          const auto call = [vm, set_mode, native_gl] {
            JNIEnv* main_env = vm->GetJNIEnv();
            main_env->PushLocalFrame(4);
            set_mode(main_env, native_gl, JNI_FALSE, main_env->NewStringUTF("ASMA.start"));
            main_env->PopLocalFrame(nullptr);
          };
          if (!shared_->main_thread_pumps) {
            call();
          } else if (!RunOnMainThread(call, 3000)) {
            Log("Roblox start-up: setTaskSchedulerBackgroundMode did not run on the main thread within 3000 ms "
                "(Mocktail continues without it)");
          }
        })) {
      return false;
    }
    return Step("NativeGLInterface.nativeAppBridgeV2StartAppWithParams", [&] {
      jobject surface = BuildSurface(env, o_);
      env->NewGlobalRef(surface);
      shared_->result.surface = surface;
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
      android::AssociateSurface(surface, static_cast<ANativeWindow*>(o_.native_window));
#endif
      jobject platform = BuildPlatformParams(env, surface, o_);
      s_.start_app_with_params(env, native_gl_, BuildStartAppParams(env, o_, app_params_, platform, surface));
    });
  }

 public:

 private:
  template <typename Body>
  bool Step(const char* name, Body body) {
    if (shared_->stop->load()) {
      shared_->result.stopped = true;
      shared_->result.step = name;
      Log(std::string("Roblox start-up: stop requested; ending before ") + name);
      return false;
    }
    shared_->current.store(name);
    Log(std::string("Roblox start-up: ") + name);
    const ULONGLONG start = GetTickCount64();
    body();
    Log(std::string("Roblox start-up: ") + name + " returned in " + std::to_string(GetTickCount64() - start) + " ms");
    ++shared_->result.steps_run;
    return true;
  }

  void SetStaticListener(const char* method) {
    JNIEnv* env = env_;
    const bool implementation = std::string_view(method) == "setImplementation";
    jobject value = implementation ? NewObject(env, "com/roblox/engine/jni/EngineJavaCallback2") : listener_;
    const char* signature = implementation ? "(Lcom/roblox/engine/jni/EngineJavaCallback2;)V"
                                           : "(Lcom/roblox/engine/jni/OnAppBridgeNotificationListener;)V";
    jmethodID setter = env->GetStaticMethodID(native_gl_java_, method, signature);
    if (setter == nullptr || value == nullptr) {
      Log(std::string("Roblox start-up: NativeGLJavaInterface.") + method + " skipped: no method or object");
      return;
    }
    env->CallStaticVoidMethod(native_gl_java_, setter, value);
  }

  std::string DataDir() const { return "/data/user/0/" + o_.package_name; }
  std::string FilesDir() const { return DataDir() + "/files"; }
  std::string CacheDir() const { return DataDir() + "/cache"; }
  std::string ExternalBase() const { return "/sdcard/Android/data/" + o_.package_name; }
  std::string ExternalDir() const { return ExternalBase() + "/files"; }

  void ConfigureNativeSettings() {
    JNIEnv* env = env_;
    const std::string data = DataDir(), files = FilesDir(), cache = CacheDir();
    for (const std::string& folder :
         {data, files, cache, ExternalBase(), ExternalDir(), data + "/rbx-storage", data + "/appData",
          data + "/appData/LocalStorage", data + "/appData/rbx-storage", files + "/rbx-storage", files + "/appData",
          files + "/appData/LocalStorage", files + "/appData/OTAPatchBackups", files + "/appData/rbx-storage",
          cache + "/ContentProvider_2", cache + "/rbx-storage", cache + "/sounds"}) {
      EnsureAndroidDirectory(folder);
    }
    const auto string = [env](const std::string& text) { return env->NewStringUTF(text.c_str()); };
    s_.set_exception_reason_filename(env, settings_, string("exception_reason.txt"));
    s_.set_base_url(env, settings_, string("https://www.roblox.com/"), string("https://api.roblox.com/"));
    s_.set_roblox_channel(env, settings_, string("production"));
    if (s_.override_channel_platform_name != nullptr) {
      s_.override_channel_platform_name(env, settings_, string("GoogleAndroidApp"));
    }
    if (s_.set_roblox_version != nullptr) s_.set_roblox_version(env, settings_, string(o_.version_name));
    s_.set_device_info(env, settings_, BuildDeviceParams(env, o_));
    s_.set_base_data_directories(env, settings_, string(data), string(ExternalBase()));
    s_.set_cache_directory(env, settings_, string(data));
    s_.set_files_directory(env, settings_, string(files));
    s_.set_external_directory(env, settings_, string(ExternalDir()));
    s_.set_preferences_file(env, settings_, string("prefs"));
    s_.set_default_app_policy_file(env, settings_,
                                   string("content/guac/defaultConfigs/GuacDefaultPolicy-GlobalDist.json"));
    s_.init_fast_log(env, settings_);
    RestoreSavedSession();
  }

  void RestoreSavedSession() {
    jnivm::VM* vm = shared_->vm;
    std::string credential;
    if (vm == nullptr || !vm->CopyRobloxCredentialFromProvider(&credential) || credential.empty()) return;
    if (s_.set_multiple_cookies == nullptr) {
      Log("Roblox start-up: nativeSetMultipleCookies is not exported; the saved session cannot be restored");
    } else {
      JNIEnv* env = env_;
      jstring url = env->NewStringUTF("https://www.roblox.com/");
      jstring cookies = env->NewStringUTF(credential.c_str());
      s_.set_multiple_cookies(env, settings_, url, cookies);
      env->DeleteLocalRef(cookies);
      env->DeleteLocalRef(url);
      Log("Roblox start-up: NativeSettings multipleCookies (saved session)");
    }
    SecureZeroMemory(credential.data(), credential.size());
    credential.clear();
  }

  void ConfigureLocalStorage() {
    JNIEnv* env = env_;
    const std::string data = DataDir(), files = FilesDir(), cache = CacheDir();
    for (const std::string& folder :
         {files + "/appData", files + "/appData/LocalStorage", files + "/appData/OTAPatchBackups",
          files + "/appData/rbx-storage", data + "/rbx-storage", data + "/shared_prefs", cache,
          cache + "/ContentProvider_2", cache + "/rbx-storage", cache + "/sounds"}) {
      EnsureAndroidDirectory(folder);
    }
    jobject asset_manager = NewObject(env, "android/content/res/AssetManager");
    if (s_.init_asset_manager != nullptr) {
      Log("Roblox start-up: JNIAAssetManagerSetup.initNative");
      s_.init_asset_manager(env, env->FindClass("com/roblox/client/JNIAAssetManagerSetup"), asset_manager);
    }
    Log("Roblox start-up: LocalStorageManager.initStorageManagerNativeV3 files=" + files + " cache=" + cache);
    s_.init_storage_manager(env, NewObject(env, "com/roblox/client/LocalStorageManager"), asset_manager,
                            env->NewStringUTF(files.c_str()), env->NewStringUTF(cache.c_str()));
  }

  Shared* shared_;
  const Symbols& s_;
  const RobloxStartupOptions& o_;
  JNIEnv* env_;
  jclass native_gl_ = nullptr;
  jclass settings_ = nullptr;
  jclass native_gl_java_ = nullptr;
  jclass startup_activity_ = nullptr;
  jobject game_activity_ = nullptr;
  jobject listener_ = nullptr;
  jstring asset_path_ = nullptr;
  jstring client_settings_ = nullptr;
  jstring overrides_ = nullptr;
  jstring group_ = nullptr;
  jstring fast_flags_ = nullptr;
  jstring app_params_ = nullptr;
};

DWORD WINAPI EngineThread(void* parameter) {
  auto* shared = static_cast<Shared*>(parameter);
  platform::EnsureGuestThread();
  Log("Roblox start-up thread " + EnsureThreadStackGuarantee(64 * 1024).detail);
  JavaVM* java_vm = shared->vm->GetJavaVM();
  void* raw_env = nullptr;
  const bool attached = java_vm->AttachCurrentThread(&raw_env, nullptr) == JNI_OK;
  shared->vm->RestoreFunctions();
  JNIEnv* env = shared->vm->GetJNIEnv();
  Sequence(shared, env).Run();
  if (attached) java_vm->DetachCurrentThread();
  return 0;
}

}

void RegisterRobloxJavaClasses(jnivm::VM* vm) {
#if defined(ELIXIR_RUNTIME_HAS_ANDROID)
  vm->RegisterClass("com/roblox/engine/jni/video/MediaCodecInfoUtils");
  vm->RegisterClass("com/roblox/engine/jni/video/VideoCodecCapability");
  vm->SetVideoCodecsQuery(QueryVideoCodecs);
#endif
  vm->RegisterClass("android/content/Context");
  vm->RegisterClass("com/roblox/client/RobloxActivity");
  vm->RegisterClass("rbx/JNIRobloxSettings")
      ->RegisterMethod("nativeInitClientSettings", "()V",
                       [](JNIEnv*, jobject) { Log("[JNI callback] JNIRobloxSettings.nativeInitClientSettings"); });
  for (const char* name : {
           "com/roblox/engine/jni/NativeGLInterface",
           "com/roblox/engine/jni/NativeReportingInterface",
           "com/roblox/engine/jni/NativeInputInterface",
           "com/roblox/engine/jni/NativeSettingsInterface",
           "com/roblox/engine/jni/NativeAppBridgeInterface",
           "com/roblox/engine/jni/NativeGLJavaInterface",
           "com/roblox/engine/jni/EngineJavaCallback2",
           "com/roblox/engine/jni/OnAppBridgeNotificationListener",
           "com/roblox/client/flags/FlagJniInterface",
           "com/roblox/client/flags/NativeFlagsInitResult",
           "com/roblox/engine/jni/model/DeviceStaticParams",
           "com/roblox/engine/jni/model/DeviceParams",
           "com/roblox/engine/jni/model/PlatformParams",
           "com/roblox/engine/jni/model/NativeTextBoxInfo",
           "com/roblox/engine/jni/autovalue/InitParams",
           "com/roblox/engine/jni/autovalue/StartAppParams",
           "com/roblox/client/startup/MainGameActivity",
           "com/roblox/client/startup/NativeHelper",
           "com/roblox/universalapp/activitylifecyclecallbacks/JNIActivityLifecycleCallbacks",
           "com/roblox/universalapp/messagebus/MessageBus",
           "com/roblox/universalapp/messagebus/Connection",
           "com/roblox/universalapp/systemtheme/SystemThemeProtocol",
           "com/roblox/universalapp/cookie/JNICookieManager",
           "com/roblox/universalapp/cookie/JNICookieProtocol",
           "com/roblox/universalapp/cookie/JNICookieProtocol$OnSetCookieHandler",
           "com/roblox/client/JNIAAssetManagerSetup",
           "org/fmod/FMOD",
           "android/content/res/AssetManager",
           "android/media/AudioManager",
           "com/roblox/protocols/systemdialog/PlatformSystemDialogHandler",
           "com/roblox/protocols/systemdialogplatforminterface/generated/SystemDialogRequest",
           "com/roblox/protocols/systemdialogplatforminterface/generated/ISystemDialogCallback",
           "com/google/androidgamesdk/GameActivity",
           "com/google/androidgamesdk/gametextinput/InputConnection",
           "com/google/androidgamesdk/gametextinput/State",
           "com/roblox/client/LocalStorageManager",
           "com/roblox/protocols/localstorageplatforminterface/generated/ILocalStorageHandlerCore",
           "com/roblox/protocols/localstorageplatforminterface/generated/ILocalStorageHandlerCore$CppProxy",
           "com/roblox/protocols/localstorageplatforminterface/generated/IPlatformLocalStorageHandler",
           "com/roblox/protocols/localstorageplatforminterface/generated/IPlatformLocalStorageHandler$CppProxy",
           "java/util/HashSet",
           "java/util/Iterator",
           "android/os/LocaleList",
           "java/util/Locale",
           "androidx/core/graphics/Insets",
           "androidx/core/view/WindowInsetsCompat$Type",
           "android/view/MotionEvent",
           "android/view/KeyEvent",
           "android/view/Surface",
           "java/lang/String",
           "java/lang/Object",
       }) {
    vm->RegisterClass(name);
  }
}

void RunPendingMainThreadWork() {
  MainThreadWork& work = Work();
  std::function<void()> task;
  {
    std::lock_guard<std::mutex> lock(work.mutex);
    if (!work.pending) return;
    task = std::move(work.task);
    work.task = nullptr;
    work.pending = false;
  }
  task();
  {
    std::lock_guard<std::mutex> lock(work.mutex);
    work.finished = true;
  }
  work.done.notify_all();
}

bool PassDisplayRefreshRates(void* roblox, jnivm::VM* vm, float current_hz, const std::vector<float>& supported_hz) {
  constexpr const char* kGl = "Java_com_roblox_engine_jni_NativeGLInterface_";
  std::vector<std::string> missing;
  FloatFn pass_rate = nullptr;
  FloatArrayFn pass_rates = nullptr;
  Resolve(roblox, (std::string(kGl) + "nativePassCurrentDisplayRefreshRate").c_str(), false, &pass_rate, &missing);
  Resolve(roblox, (std::string(kGl) + "nativePassSupportedRefreshRates").c_str(), false, &pass_rates, &missing);
  if (pass_rate == nullptr && pass_rates == nullptr) return false;
  JNIEnv* env = vm->GetJNIEnv();
  env->PushLocalFrame(8);
  jclass native_gl = env->FindClass("com/roblox/engine/jni/NativeGLInterface");
  if (pass_rate != nullptr && current_hz > 0.0f) pass_rate(env, native_gl, current_hz);
  if (pass_rates != nullptr && !supported_hz.empty()) {
    const jsize count = static_cast<jsize>(supported_hz.size());
    jfloatArray rates = env->NewFloatArray(count);
    if (rates != nullptr) {
      env->SetFloatArrayRegion(rates, 0, count, supported_hz.data());
      pass_rates(env, native_gl, rates);
    }
  }
  env->PopLocalFrame(nullptr);
  return true;
}

std::string DescribeRefreshRates(float current_hz, const std::vector<float>& supported_hz) {
  std::string text = std::to_string(static_cast<int>(current_hz + 0.5f)) + " Hz (supported:";
  for (const float rate : supported_hz) text += " " + std::to_string(static_cast<int>(rate + 0.5f));
  return text + ")";
}

bool ShutdownRobloxApp(void* roblox, jnivm::VM* vm, std::string* error) {
  constexpr const char* kGl = "Java_com_roblox_engine_jni_NativeGLInterface_";
  std::vector<std::string> missing;
  NoArgFn pause = nullptr;
  NoArgFn destroy = nullptr;
  BackgroundModeFn set_mode = nullptr;
  Resolve(roblox, (std::string(kGl) + "nativeAppBridgeV2PauseApp").c_str(), true, &pause, &missing);
  Resolve(roblox, (std::string(kGl) + "setTaskSchedulerBackgroundMode").c_str(), true, &set_mode, &missing);
  Resolve(roblox, (std::string(kGl) + "nativeAppBridgeV2DestroyApp").c_str(), true, &destroy, &missing);
  if (!missing.empty()) {
    if (error != nullptr) {
      *error = "libroblox.so does not export";
      for (const std::string& name : missing) *error += " " + name;
    }
    return false;
  }
  JNIEnv* env = vm->GetJNIEnv();
  env->PushLocalFrame(16);
  jclass native_gl = env->FindClass("com/roblox/engine/jni/NativeGLInterface");
  Log("Roblox shutdown: NativeGLInterface.nativeAppBridgeV2PauseApp");
  pause(env, native_gl);
  Log("Roblox shutdown: NativeGLInterface.setTaskSchedulerBackgroundMode(true, \"ASMA.stop\")");
  set_mode(env, native_gl, JNI_TRUE, env->NewStringUTF("ASMA.stop"));
  Log("Roblox shutdown: NativeGLInterface.nativeAppBridgeV2DestroyApp");
  destroy(env, native_gl);
  env->PopLocalFrame(nullptr);
  Log("Roblox shutdown: done");
  return true;
}

RobloxStartupResult RunRobloxStartup(void* roblox, jnivm::VM* vm, const RobloxStartupOptions& options,
                                     const std::atomic<bool>& stop, unsigned timeout_ms,
                                     const std::function<void()>& pump) {
  RobloxStartupResult result;
  const ULONGLONG start = GetTickCount64();
  std::vector<std::string> missing;
  Symbols symbols = ResolveSymbols(roblox, options.native_window != nullptr, &missing);
  if (!missing.empty()) {
    result.missing = std::move(missing);
    result.error = "libroblox.so does not export " + std::to_string(result.missing.size()) +
                   " of Roblox's start-up natives";
    return result;
  }
  if (symbols.get_cookies_for_domain != nullptr) vm->SetRobloxCookieGetter(symbols.get_cookies_for_domain);

  auto shared = std::make_unique<Shared>();
  shared->roblox = roblox;
  shared->vm = vm;
  shared->options = options;
  shared->symbols = symbols;
  shared->stop = &stop;
  shared->main_thread_pumps = static_cast<bool>(pump);
  HANDLE thread = CreateThread(nullptr, kEngineStackReserve, &EngineThread, shared.get(),
                               STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
  if (thread == nullptr) {
    result.error = "cannot create Roblox's start-up thread (CreateThread error " + std::to_string(GetLastError()) + ")";
    return result;
  }
  DWORD wait = WAIT_TIMEOUT;
  if (!pump) {
    wait = WaitForSingleObject(thread, timeout_ms);
  } else {
    for (;;) {
      wait = WaitForSingleObject(thread, 10);
      if (wait != WAIT_TIMEOUT) break;
      pump();
      RunPendingMainThreadWork();
      if (GetTickCount64() - start >= timeout_ms) break;
    }
  }
  if (wait != WAIT_OBJECT_0) {
    result.timed_out = true;
    result.step = shared->current.load();
    result.error = "Roblox's start-up did not finish within " + std::to_string(timeout_ms / 1000) + " s; it is still in " +
                   result.step;
    result.elapsed_ms = GetTickCount64() - start;
    static_cast<void>(shared.release());
    CloseHandle(thread);
    return result;
  }
  CloseHandle(thread);
  result = shared->result;
  if (!result.ok && !result.stopped && result.error.empty()) {
    result.step = shared->current.load();
    result.error = "Roblox's start-up ended in " + result.step;
  }
  result.elapsed_ms = GetTickCount64() - start;
  return result;
}

}
