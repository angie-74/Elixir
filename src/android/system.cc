// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <new>
#include <string>

#include "android/android_internal.h"
#include "bionic/runtime.h"
#include "platform/windows/guest_tls.h"

namespace guest = elixir::bionic::guest;

struct AStorageManager {};
struct AObbInfo {};
struct AThermalManager {};

namespace {

constexpr std::int32_t kObbCouldNotMount = 21;
constexpr std::int32_t kObbNotMounted = 23;
constexpr int kThermalStatusError = -1;

using ObbCallback = elixir::abi::SysvFn<void(const char*, std::int32_t, void*)>;

struct ObbNotification {
  ObbCallback callback;
  std::string filename;
  std::int32_t state;
  void* data;
};

DWORD WINAPI DeliverObbNotification(void* parameter) {
  auto* notification = static_cast<ObbNotification*>(parameter);
  elixir::platform::EnsureGuestThread();
  notification->callback(notification->filename.c_str(), notification->state, notification->data);
  delete notification;
  return 0;
}

void NotifyObb(ObbCallback callback, const char* filename, std::int32_t state, void* data) noexcept {
  if (callback == nullptr) return;
  elixir::android::GuardedVoid([&] {
    auto* notification = new ObbNotification{callback, filename != nullptr ? filename : "", state, data};
    HANDLE thread = CreateThread(nullptr, 0, &DeliverObbNotification, notification, 0, nullptr);
    if (thread == nullptr) {
      delete notification;
      elixir::android::Log(elixir::android::kLogError, "cannot start the OBB callback thread");
      return;
    }
    CloseHandle(thread);
  });
}

}

ELIXIR_ANDROID_FN(AStorageManager*, AStorageManager_new)() noexcept { return new (std::nothrow) AStorageManager(); }

ELIXIR_ANDROID_FN(void, AStorageManager_delete)(AStorageManager* manager) noexcept { delete manager; }

ELIXIR_ANDROID_FN(void, AStorageManager_mountObb)(AStorageManager* manager, const char* filename, const char* key,
                                                  ObbCallback callback, void* data) noexcept {
  if (manager != nullptr) NotifyObb(callback, filename, kObbCouldNotMount, data);
}

ELIXIR_ANDROID_FN(void, AStorageManager_unmountObb)(AStorageManager* manager, const char* filename, int force,
                                                    ObbCallback callback, void* data) noexcept {
  if (manager != nullptr) NotifyObb(callback, filename, kObbNotMounted, data);
}

ELIXIR_ANDROID_FN(int, AStorageManager_isObbMounted)(AStorageManager* manager, const char* filename) noexcept {
  return 0;
}

ELIXIR_ANDROID_FN(const char*, AStorageManager_getMountedObbPath)(AStorageManager* manager,
                                                                  const char* filename) noexcept {
  return nullptr;
}

ELIXIR_ANDROID_FN(AObbInfo*, AObbScanner_getObbInfo)(const char* filename) noexcept { return nullptr; }
ELIXIR_ANDROID_FN(void, AObbInfo_delete)(AObbInfo* info) noexcept { delete info; }
ELIXIR_ANDROID_FN(const char*, AObbInfo_getPackageName)(AObbInfo* info) noexcept { return nullptr; }
ELIXIR_ANDROID_FN(std::int32_t, AObbInfo_getVersion)(AObbInfo* info) noexcept { return 0; }
ELIXIR_ANDROID_FN(std::int32_t, AObbInfo_getFlags)(AObbInfo* info) noexcept { return 0; }

ELIXIR_ANDROID_FN(AThermalManager*, AThermal_acquireManager)() noexcept {
  return new (std::nothrow) AThermalManager();
}

ELIXIR_ANDROID_FN(void, AThermal_releaseManager)(AThermalManager* manager) noexcept { delete manager; }

ELIXIR_ANDROID_FN(int, AThermal_getCurrentThermalStatus)(AThermalManager* manager) noexcept {
  return kThermalStatusError;
}

ELIXIR_ANDROID_FN(int, AThermal_registerThermalStatusListener)(AThermalManager* manager, void* callback,
                                                               void* data) noexcept {
  if (manager == nullptr || callback == nullptr) return guest::kEINVAL;
  return guest::kEPIPE;
}

ELIXIR_ANDROID_FN(int, AThermal_unregisterThermalStatusListener)(AThermalManager* manager, void* callback,
                                                                 void* data) noexcept {
  return guest::kEINVAL;
}

ELIXIR_ANDROID_FN(float, AThermal_getThermalHeadroom)(AThermalManager* manager, int forecast_seconds) noexcept {
  return std::nanf("");
}

ELIXIR_ANDROID_FN(int, AThermal_getThermalHeadroomThresholds)(AThermalManager* manager, const void** thresholds,
                                                              std::uint64_t* size) noexcept {
  if (manager == nullptr || thresholds == nullptr || size == nullptr || *thresholds != nullptr) return guest::kEINVAL;
  return guest::kENOSYS;
}

ELIXIR_ANDROID_FN(bool, ATrace_isEnabled)() noexcept { return false; }
ELIXIR_ANDROID_FN(void, ATrace_beginSection)(const char* name) noexcept {}
ELIXIR_ANDROID_FN(void, ATrace_endSection)() noexcept {}
ELIXIR_ANDROID_FN(void, ATrace_beginAsyncSection)(const char* name, std::int32_t cookie) noexcept {}
ELIXIR_ANDROID_FN(void, ATrace_endAsyncSection)(const char* name, std::int32_t cookie) noexcept {}
ELIXIR_ANDROID_FN(void, ATrace_setCounter)(const char* name, std::int64_t value) noexcept {}

namespace elixir::android {

void RegisterServiceExports(bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##name))
  ELIXIR_ADD(AStorageManager_new);
  ELIXIR_ADD(AStorageManager_delete);
  ELIXIR_ADD(AStorageManager_mountObb);
  ELIXIR_ADD(AStorageManager_unmountObb);
  ELIXIR_ADD(AStorageManager_isObbMounted);
  ELIXIR_ADD(AStorageManager_getMountedObbPath);
  ELIXIR_ADD(AObbScanner_getObbInfo);
  ELIXIR_ADD(AObbInfo_delete);
  ELIXIR_ADD(AObbInfo_getPackageName);
  ELIXIR_ADD(AObbInfo_getVersion);
  ELIXIR_ADD(AObbInfo_getFlags);
  ELIXIR_ADD(AThermal_acquireManager);
  ELIXIR_ADD(AThermal_releaseManager);
  ELIXIR_ADD(AThermal_getCurrentThermalStatus);
  ELIXIR_ADD(AThermal_registerThermalStatusListener);
  ELIXIR_ADD(AThermal_unregisterThermalStatusListener);
  ELIXIR_ADD(AThermal_getThermalHeadroom);
  ELIXIR_ADD(AThermal_getThermalHeadroomThresholds);
  ELIXIR_ADD(ATrace_isEnabled);
  ELIXIR_ADD(ATrace_beginSection);
  ELIXIR_ADD(ATrace_endSection);
  ELIXIR_ADD(ATrace_beginAsyncSection);
  ELIXIR_ADD(ATrace_endAsyncSection);
  ELIXIR_ADD(ATrace_setCounter);
#undef ELIXIR_ADD
}

}
