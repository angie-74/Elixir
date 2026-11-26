// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <cmath>
#include <cstdint>
#include <mutex>
#include <new>

#include "android/android_internal.h"
#include "bionic/runtime.h"

struct ASensorManager {};
struct ASensor {};
struct ASensorEventQueue {
  ALooper* looper;
};

namespace {

constexpr int kInvalid = -1;
constexpr int kFifoCountInvalid = -1;
constexpr int kDelayInvalid = INT32_MIN;
constexpr int kTypeInvalid = -1;
constexpr int kReportingModeInvalid = -1;
constexpr int kDirectRateStop = 0;
constexpr int kBadValue = -22;

ASensorManager g_manager;
const ASensor* const g_empty_list[1] = {nullptr};

}

ELIXIR_ANDROID_FN(ASensorManager*, ASensorManager_getInstance)() noexcept { return &g_manager; }

ELIXIR_ANDROID_FN(ASensorManager*, ASensorManager_getInstanceForPackage)(const char* package) noexcept {
  return &g_manager;
}

ELIXIR_ANDROID_FN(int, ASensorManager_getSensorList)(ASensorManager* manager, const ASensor* const** list) noexcept {
  if (list != nullptr) *list = g_empty_list;
  return 0;
}

ELIXIR_ANDROID_FN(std::int64_t, ASensorManager_getDynamicSensorList)(ASensorManager* manager,
                                                                     const ASensor* const** list) noexcept {
  if (list != nullptr) *list = g_empty_list;
  return 0;
}

ELIXIR_ANDROID_FN(const ASensor*, ASensorManager_getDefaultSensor)(ASensorManager* manager, int type) noexcept {
  return nullptr;
}

ELIXIR_ANDROID_FN(const ASensor*, ASensorManager_getDefaultSensorEx)(ASensorManager* manager, int type,
                                                                    bool wake_up) noexcept {
  return nullptr;
}

ELIXIR_ANDROID_FN(ASensorEventQueue*, ASensorManager_createEventQueue)(ASensorManager* manager, ALooper* looper,
                                                                      int ident, void* callback, void* data) noexcept {
  if (manager == nullptr || looper == nullptr) return nullptr;
  auto* queue = new (std::nothrow) ASensorEventQueue{looper};
  if (queue != nullptr) elixir::android::looper::AcquireLooper(looper);
  return queue;
}

ELIXIR_ANDROID_FN(int, ASensorManager_destroyEventQueue)(ASensorManager* manager, ASensorEventQueue* queue) noexcept {
  if (manager == nullptr || queue == nullptr) return kBadValue;
  elixir::android::looper::ReleaseLooper(queue->looper);
  delete queue;
  return 0;
}

ELIXIR_ANDROID_FN(int, ASensorManager_createSharedMemoryDirectChannel)(ASensorManager* manager, int fd,
                                                                      std::uint64_t size) noexcept {
  return kBadValue;
}

ELIXIR_ANDROID_FN(int, ASensorManager_createHardwareBufferDirectChannel)(ASensorManager* manager, const void* buffer,
                                                                        std::uint64_t size) noexcept {
  return kBadValue;
}

ELIXIR_ANDROID_FN(void, ASensorManager_destroyDirectChannel)(ASensorManager* manager, int channel) noexcept {}

ELIXIR_ANDROID_FN(int, ASensorManager_configureDirectReport)(ASensorManager* manager, const ASensor* sensor,
                                                            int channel, int rate) noexcept {
  return kBadValue;
}

ELIXIR_ANDROID_FN(int, ASensorEventQueue_registerSensor)(ASensorEventQueue* queue, const ASensor* sensor,
                                                        std::int32_t sampling_period_us,
                                                        std::int64_t max_batch_latency_us) noexcept {
  return kBadValue;
}

ELIXIR_ANDROID_FN(int, ASensorEventQueue_enableSensor)(ASensorEventQueue* queue, const ASensor* sensor) noexcept {
  return kBadValue;
}

ELIXIR_ANDROID_FN(int, ASensorEventQueue_disableSensor)(ASensorEventQueue* queue, const ASensor* sensor) noexcept {
  return kBadValue;
}

ELIXIR_ANDROID_FN(int, ASensorEventQueue_setEventRate)(ASensorEventQueue* queue, const ASensor* sensor,
                                                      std::int32_t usec) noexcept {
  return kBadValue;
}

ELIXIR_ANDROID_FN(int, ASensorEventQueue_hasEvents)(ASensorEventQueue* queue) noexcept {
  return queue != nullptr ? 0 : kBadValue;
}

ELIXIR_ANDROID_FN(std::int64_t, ASensorEventQueue_getEvents)(ASensorEventQueue* queue, void* events,
                                                             std::uint64_t count) noexcept {
  return queue != nullptr ? 0 : kBadValue;
}

ELIXIR_ANDROID_FN(int, ASensorEventQueue_requestAdditionalInfoEvents)(ASensorEventQueue* queue, bool enable) noexcept {
  return kBadValue;
}

ELIXIR_ANDROID_FN(const char*, ASensor_getName)(const ASensor* sensor) noexcept { return nullptr; }
ELIXIR_ANDROID_FN(const char*, ASensor_getVendor)(const ASensor* sensor) noexcept { return nullptr; }
ELIXIR_ANDROID_FN(int, ASensor_getType)(const ASensor* sensor) noexcept { return kTypeInvalid; }
ELIXIR_ANDROID_FN(float, ASensor_getResolution)(const ASensor* sensor) noexcept { return std::nanf(""); }
ELIXIR_ANDROID_FN(int, ASensor_getMinDelay)(const ASensor* sensor) noexcept { return kDelayInvalid; }
ELIXIR_ANDROID_FN(int, ASensor_getFifoMaxEventCount)(const ASensor* sensor) noexcept { return kFifoCountInvalid; }
ELIXIR_ANDROID_FN(int, ASensor_getFifoReservedEventCount)(const ASensor* sensor) noexcept { return kFifoCountInvalid; }
ELIXIR_ANDROID_FN(const char*, ASensor_getStringType)(const ASensor* sensor) noexcept { return nullptr; }
ELIXIR_ANDROID_FN(int, ASensor_getReportingMode)(const ASensor* sensor) noexcept { return kReportingModeInvalid; }
ELIXIR_ANDROID_FN(bool, ASensor_isWakeUpSensor)(const ASensor* sensor) noexcept { return false; }
ELIXIR_ANDROID_FN(bool, ASensor_isDirectChannelTypeSupported)(const ASensor* sensor, int channel_type) noexcept {
  return false;
}
ELIXIR_ANDROID_FN(int, ASensor_getHighestDirectReportRateLevel)(const ASensor* sensor) noexcept {
  return kDirectRateStop;
}
ELIXIR_ANDROID_FN(int, ASensor_getHandle)(const ASensor* sensor) noexcept { return kInvalid; }

namespace elixir::android {

void RegisterSensorExports(bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##name))
  ELIXIR_ADD(ASensorManager_getInstance);
  ELIXIR_ADD(ASensorManager_getInstanceForPackage);
  ELIXIR_ADD(ASensorManager_getSensorList);
  ELIXIR_ADD(ASensorManager_getDynamicSensorList);
  ELIXIR_ADD(ASensorManager_getDefaultSensor);
  ELIXIR_ADD(ASensorManager_getDefaultSensorEx);
  ELIXIR_ADD(ASensorManager_createEventQueue);
  ELIXIR_ADD(ASensorManager_destroyEventQueue);
  ELIXIR_ADD(ASensorManager_createSharedMemoryDirectChannel);
  ELIXIR_ADD(ASensorManager_createHardwareBufferDirectChannel);
  ELIXIR_ADD(ASensorManager_destroyDirectChannel);
  ELIXIR_ADD(ASensorManager_configureDirectReport);
  ELIXIR_ADD(ASensorEventQueue_registerSensor);
  ELIXIR_ADD(ASensorEventQueue_enableSensor);
  ELIXIR_ADD(ASensorEventQueue_disableSensor);
  ELIXIR_ADD(ASensorEventQueue_setEventRate);
  ELIXIR_ADD(ASensorEventQueue_hasEvents);
  ELIXIR_ADD(ASensorEventQueue_getEvents);
  ELIXIR_ADD(ASensorEventQueue_requestAdditionalInfoEvents);
  ELIXIR_ADD(ASensor_getName);
  ELIXIR_ADD(ASensor_getVendor);
  ELIXIR_ADD(ASensor_getType);
  ELIXIR_ADD(ASensor_getResolution);
  ELIXIR_ADD(ASensor_getMinDelay);
  ELIXIR_ADD(ASensor_getFifoMaxEventCount);
  ELIXIR_ADD(ASensor_getFifoReservedEventCount);
  ELIXIR_ADD(ASensor_getStringType);
  ELIXIR_ADD(ASensor_getReportingMode);
  ELIXIR_ADD(ASensor_isWakeUpSensor);
  ELIXIR_ADD(ASensor_isDirectChannelTypeSupported);
  ELIXIR_ADD(ASensor_getHighestDirectReportRateLevel);
  ELIXIR_ADD(ASensor_getHandle);
#undef ELIXIR_ADD
}

}
