// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <algorithm>
#include <cstdint>
#include <deque>
#include <mutex>
#include <new>
#include <unordered_set>

#include "android/android_internal.h"
#include "bionic/runtime.h"

namespace {

constexpr std::int32_t kTypeKey = 1;
constexpr std::int32_t kTypeMotion = 2;
constexpr std::int32_t kNoEvent = -11;
constexpr std::int32_t kBadValue = -22;

}

struct AInputEvent {
  std::int32_t type = 0;
  bool owned_by_queue = false;
  elixir::android::KeyEventData key;
  elixir::android::MotionEventData motion;
};

struct AInputQueue {
  std::mutex mutex;
  std::deque<AInputEvent*> events;
  std::unordered_set<AInputEvent*> in_flight;
  ALooper* looper = nullptr;
  elixir::android::looper::HostSourceId source = 0;
};

namespace elixir::android {
namespace {

bool QueueReady(void* context) noexcept {
  auto* queue = static_cast<AInputQueue*>(context);
  std::lock_guard<std::mutex> lock(queue->mutex);
  return !queue->events.empty();
}

void Detach(AInputQueue* queue) noexcept {
  ALooper* looper = nullptr;
  looper::HostSourceId source = 0;
  {
    std::lock_guard<std::mutex> lock(queue->mutex);
    std::swap(looper, queue->looper);
    std::swap(source, queue->source);
  }
  if (looper != nullptr) {
    looper::RemoveHostSource(looper, source);
    looper::ReleaseLooper(looper);
  }
}

bool Post(AInputQueue* queue, AInputEvent* event) {
  ALooper* looper = nullptr;
  {
    std::lock_guard<std::mutex> lock(queue->mutex);
    queue->events.push_back(event);
    looper = queue->looper;
    if (looper != nullptr) looper::AcquireLooper(looper);
  }
  if (looper != nullptr) {
    looper::SignalLooper(looper);
    looper::ReleaseLooper(looper);
  }
  return true;
}

const PointerData* Pointer(const AInputEvent* event, std::uint64_t index) {
  if (event == nullptr || event->type != kTypeMotion || index >= event->motion.pointers.size()) return nullptr;
  return &event->motion.pointers[static_cast<std::size_t>(index)];
}

float Axis(const PointerData* pointer, std::int32_t axis) {
  if (pointer == nullptr) return 0.0f;
  switch (axis) {
    case 0: return pointer->x;
    case 1: return pointer->y;
    case 2: return pointer->pressure;
    case 3: return pointer->size;
    case 4: return pointer->touch_major;
    case 5: return pointer->touch_minor;
    case 6: return pointer->tool_major;
    case 7: return pointer->tool_minor;
    case 8: return pointer->orientation;
    case 9: return pointer->axis_vscroll;
    case 10: return pointer->axis_hscroll;
    default: return 0.0f;
  }
}

}

AInputQueue* CreateInputQueue() { return new (std::nothrow) AInputQueue(); }

void DestroyInputQueue(AInputQueue* queue) {
  if (queue == nullptr) return;
  Detach(queue);
  for (AInputEvent* event : queue->events) delete event;
  for (AInputEvent* event : queue->in_flight) delete event;
  delete queue;
}

bool PostKeyEvent(AInputQueue* queue, const KeyEventData& data) {
  if (queue == nullptr) return false;
  try {
    auto* event = new AInputEvent();
    event->type = kTypeKey;
    event->owned_by_queue = true;
    event->key = data;
    return Post(queue, event);
  } catch (...) {
    return false;
  }
}

bool PostMotionEvent(AInputQueue* queue, const MotionEventData& data) {
  if (queue == nullptr || data.pointers.empty()) return false;
  try {
    auto* event = new AInputEvent();
    event->type = kTypeMotion;
    event->owned_by_queue = true;
    event->motion = data;
    return Post(queue, event);
  } catch (...) {
    return false;
  }
}

}

using elixir::android::looper::GuestLooperCallback;

ELIXIR_ANDROID_FN(void, AInputQueue_attachLooper)(AInputQueue* queue, ALooper* looper, int ident,
                                                  GuestLooperCallback callback, void* data) noexcept {
  using namespace elixir::android;
  if (queue == nullptr || looper == nullptr) return;
  if (callback == nullptr && (!looper::AllowsNonCallbacks(looper) || ident < 0)) {
    Log(kLogError, "AInputQueue_attachLooper: NULL callback on a looper that does not allow non-callbacks");
    return;
  }
  Detach(queue);
  looper::HostSource source;
  source.ready = &QueueReady;
  source.context = queue;
  source.guest_callback = callback;
  source.ident = callback != nullptr ? looper::kPollCallback : ident;
  source.data = data;
  looper::AcquireLooper(looper);
  const looper::HostSourceId id = looper::AddHostSource(looper, source);
  if (id == 0) {
    looper::ReleaseLooper(looper);
    return;
  }
  std::lock_guard<std::mutex> lock(queue->mutex);
  queue->looper = looper;
  queue->source = id;
}

ELIXIR_ANDROID_FN(void, AInputQueue_detachLooper)(AInputQueue* queue) noexcept {
  if (queue != nullptr) elixir::android::Detach(queue);
}

ELIXIR_ANDROID_FN(std::int32_t, AInputQueue_hasEvents)(AInputQueue* queue) noexcept {
  if (queue == nullptr) return kBadValue;
  std::lock_guard<std::mutex> lock(queue->mutex);
  return queue->events.empty() ? 0 : 1;
}

ELIXIR_ANDROID_FN(std::int32_t, AInputQueue_getEvent)(AInputQueue* queue, AInputEvent** out_event) noexcept {
  if (queue == nullptr || out_event == nullptr) return kBadValue;
  return elixir::android::Guarded<std::int32_t>(kBadValue, [&]() -> std::int32_t {
    std::lock_guard<std::mutex> lock(queue->mutex);
    if (queue->events.empty()) return kNoEvent;
    AInputEvent* event = queue->events.front();
    queue->in_flight.insert(event);
    queue->events.pop_front();
    *out_event = event;
    return 0;
  });
}

ELIXIR_ANDROID_FN(std::int32_t, AInputQueue_preDispatchEvent)(AInputQueue* queue, AInputEvent* event) noexcept {
  return 0;
}

ELIXIR_ANDROID_FN(void, AInputQueue_finishEvent)(AInputQueue* queue, AInputEvent* event, int handled) noexcept {
  if (queue == nullptr || event == nullptr) return;
  {
    std::lock_guard<std::mutex> lock(queue->mutex);
    if (queue->in_flight.erase(event) == 0) return;
  }
  delete event;
}

ELIXIR_ANDROID_FN(AInputQueue*, AInputQueue_fromJava)(void* env, void* input_queue) noexcept { return nullptr; }

ELIXIR_ANDROID_FN(std::int32_t, AInputEvent_getType)(const AInputEvent* event) noexcept {
  return event != nullptr ? event->type : 0;
}
ELIXIR_ANDROID_FN(std::int32_t, AInputEvent_getDeviceId)(const AInputEvent* event) noexcept {
  if (event == nullptr) return 0;
  return event->type == kTypeKey ? event->key.device_id : event->motion.device_id;
}
ELIXIR_ANDROID_FN(std::int32_t, AInputEvent_getSource)(const AInputEvent* event) noexcept {
  if (event == nullptr) return 0;
  return event->type == kTypeKey ? event->key.source : event->motion.source;
}

ELIXIR_ANDROID_FN(void, AInputEvent_release)(const AInputEvent* event) noexcept {
  if (event != nullptr && !event->owned_by_queue) delete event;
}
ELIXIR_ANDROID_FN(void*, AInputEvent_toJava)(void* env, const AInputEvent* event) noexcept { return nullptr; }
ELIXIR_ANDROID_FN(const AInputEvent*, AKeyEvent_fromJava)(void* env, void* key_event) noexcept { return nullptr; }
ELIXIR_ANDROID_FN(const AInputEvent*, AMotionEvent_fromJava)(void* env, void* motion_event) noexcept {
  return nullptr;
}

#define ELIXIR_KEY_FIELD(T, Name, field)                                              \
  ELIXIR_ANDROID_FN(T, AKeyEvent_get##Name)(const AInputEvent* event) noexcept {      \
    return event != nullptr && event->type == kTypeKey ? event->key.field : T{};       \
  }
ELIXIR_KEY_FIELD(std::int32_t, Action, action)
ELIXIR_KEY_FIELD(std::int32_t, Flags, flags)
ELIXIR_KEY_FIELD(std::int32_t, KeyCode, key_code)
ELIXIR_KEY_FIELD(std::int32_t, ScanCode, scan_code)
ELIXIR_KEY_FIELD(std::int32_t, MetaState, meta_state)
ELIXIR_KEY_FIELD(std::int32_t, RepeatCount, repeat_count)
ELIXIR_KEY_FIELD(std::int64_t, DownTime, down_time)
ELIXIR_KEY_FIELD(std::int64_t, EventTime, event_time)
#undef ELIXIR_KEY_FIELD

#define ELIXIR_MOTION_FIELD(T, Name, field)                                            \
  ELIXIR_ANDROID_FN(T, AMotionEvent_get##Name)(const AInputEvent* event) noexcept {    \
    return event != nullptr && event->type == kTypeMotion ? event->motion.field : T{};  \
  }
ELIXIR_MOTION_FIELD(std::int32_t, Action, action)
ELIXIR_MOTION_FIELD(std::int32_t, ActionButton, action_button)
ELIXIR_MOTION_FIELD(std::int32_t, Flags, flags)
ELIXIR_MOTION_FIELD(std::int32_t, MetaState, meta_state)
ELIXIR_MOTION_FIELD(std::int32_t, ButtonState, button_state)
ELIXIR_MOTION_FIELD(std::int32_t, EdgeFlags, edge_flags)
ELIXIR_MOTION_FIELD(std::int32_t, Classification, classification)
ELIXIR_MOTION_FIELD(std::int64_t, DownTime, down_time)
ELIXIR_MOTION_FIELD(std::int64_t, EventTime, event_time)
ELIXIR_MOTION_FIELD(float, XOffset, x_offset)
ELIXIR_MOTION_FIELD(float, YOffset, y_offset)
ELIXIR_MOTION_FIELD(float, XPrecision, x_precision)
ELIXIR_MOTION_FIELD(float, YPrecision, y_precision)
#undef ELIXIR_MOTION_FIELD

ELIXIR_ANDROID_FN(std::uint64_t, AMotionEvent_getPointerCount)(const AInputEvent* event) noexcept {
  return event != nullptr && event->type == kTypeMotion ? event->motion.pointers.size() : 0;
}

ELIXIR_ANDROID_FN(std::int32_t, AMotionEvent_getPointerId)(const AInputEvent* event, std::uint64_t index) noexcept {
  const elixir::android::PointerData* pointer = elixir::android::Pointer(event, index);
  return pointer != nullptr ? pointer->id : -1;
}

ELIXIR_ANDROID_FN(std::int32_t, AMotionEvent_getToolType)(const AInputEvent* event, std::uint64_t index) noexcept {
  const elixir::android::PointerData* pointer = elixir::android::Pointer(event, index);
  return pointer != nullptr ? pointer->tool_type : 0;
}

#define ELIXIR_POINTER_FIELD(Name, field)                                                                    \
  ELIXIR_ANDROID_FN(float, AMotionEvent_get##Name)(const AInputEvent* event, std::uint64_t index) noexcept { \
    const elixir::android::PointerData* pointer = elixir::android::Pointer(event, index);                  \
    return pointer != nullptr ? pointer->field : 0.0f;                                                       \
  }                                                                                                          \
  ELIXIR_ANDROID_FN(float, AMotionEvent_getHistorical##Name)(const AInputEvent* event, std::uint64_t index,  \
                                                             std::uint64_t history) noexcept {               \
    return 0.0f;                                                                                             \
  }
ELIXIR_POINTER_FIELD(RawX, raw_x)
ELIXIR_POINTER_FIELD(RawY, raw_y)
ELIXIR_POINTER_FIELD(X, x)
ELIXIR_POINTER_FIELD(Y, y)
ELIXIR_POINTER_FIELD(Pressure, pressure)
ELIXIR_POINTER_FIELD(Size, size)
ELIXIR_POINTER_FIELD(TouchMajor, touch_major)
ELIXIR_POINTER_FIELD(TouchMinor, touch_minor)
ELIXIR_POINTER_FIELD(ToolMajor, tool_major)
ELIXIR_POINTER_FIELD(ToolMinor, tool_minor)
ELIXIR_POINTER_FIELD(Orientation, orientation)
#undef ELIXIR_POINTER_FIELD

ELIXIR_ANDROID_FN(float, AMotionEvent_getAxisValue)(const AInputEvent* event, std::int32_t axis,
                                                    std::uint64_t index) noexcept {
  return elixir::android::Axis(elixir::android::Pointer(event, index), axis);
}

ELIXIR_ANDROID_FN(std::uint64_t, AMotionEvent_getHistorySize)(const AInputEvent* event) noexcept { return 0; }

ELIXIR_ANDROID_FN(std::int64_t, AMotionEvent_getHistoricalEventTime)(const AInputEvent* event,
                                                                     std::uint64_t history) noexcept {
  return 0;
}

ELIXIR_ANDROID_FN(float, AMotionEvent_getHistoricalAxisValue)(const AInputEvent* event, std::int32_t axis,
                                                              std::uint64_t index, std::uint64_t history) noexcept {
  return 0.0f;
}

namespace elixir::android {

void RegisterInputExports(bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##name))
  ELIXIR_ADD(AInputQueue_attachLooper);
  ELIXIR_ADD(AInputQueue_detachLooper);
  ELIXIR_ADD(AInputQueue_hasEvents);
  ELIXIR_ADD(AInputQueue_getEvent);
  ELIXIR_ADD(AInputQueue_preDispatchEvent);
  ELIXIR_ADD(AInputQueue_finishEvent);
  ELIXIR_ADD(AInputQueue_fromJava);
  ELIXIR_ADD(AInputEvent_getType);
  ELIXIR_ADD(AInputEvent_getDeviceId);
  ELIXIR_ADD(AInputEvent_getSource);
  ELIXIR_ADD(AInputEvent_release);
  ELIXIR_ADD(AInputEvent_toJava);
  ELIXIR_ADD(AKeyEvent_fromJava);
  ELIXIR_ADD(AMotionEvent_fromJava);
  ELIXIR_ADD(AKeyEvent_getAction);
  ELIXIR_ADD(AKeyEvent_getFlags);
  ELIXIR_ADD(AKeyEvent_getKeyCode);
  ELIXIR_ADD(AKeyEvent_getScanCode);
  ELIXIR_ADD(AKeyEvent_getMetaState);
  ELIXIR_ADD(AKeyEvent_getRepeatCount);
  ELIXIR_ADD(AKeyEvent_getDownTime);
  ELIXIR_ADD(AKeyEvent_getEventTime);
  ELIXIR_ADD(AMotionEvent_getAction);
  ELIXIR_ADD(AMotionEvent_getActionButton);
  ELIXIR_ADD(AMotionEvent_getFlags);
  ELIXIR_ADD(AMotionEvent_getMetaState);
  ELIXIR_ADD(AMotionEvent_getButtonState);
  ELIXIR_ADD(AMotionEvent_getEdgeFlags);
  ELIXIR_ADD(AMotionEvent_getClassification);
  ELIXIR_ADD(AMotionEvent_getDownTime);
  ELIXIR_ADD(AMotionEvent_getEventTime);
  ELIXIR_ADD(AMotionEvent_getXOffset);
  ELIXIR_ADD(AMotionEvent_getYOffset);
  ELIXIR_ADD(AMotionEvent_getXPrecision);
  ELIXIR_ADD(AMotionEvent_getYPrecision);
  ELIXIR_ADD(AMotionEvent_getPointerCount);
  ELIXIR_ADD(AMotionEvent_getPointerId);
  ELIXIR_ADD(AMotionEvent_getToolType);
#define ELIXIR_ADD_POINTER(Name) \
  ELIXIR_ADD(AMotionEvent_get##Name); \
  ELIXIR_ADD(AMotionEvent_getHistorical##Name)
  ELIXIR_ADD_POINTER(RawX);
  ELIXIR_ADD_POINTER(RawY);
  ELIXIR_ADD_POINTER(X);
  ELIXIR_ADD_POINTER(Y);
  ELIXIR_ADD_POINTER(Pressure);
  ELIXIR_ADD_POINTER(Size);
  ELIXIR_ADD_POINTER(TouchMajor);
  ELIXIR_ADD_POINTER(TouchMinor);
  ELIXIR_ADD_POINTER(ToolMajor);
  ELIXIR_ADD_POINTER(ToolMinor);
  ELIXIR_ADD_POINTER(Orientation);
#undef ELIXIR_ADD_POINTER
  ELIXIR_ADD(AMotionEvent_getAxisValue);
  ELIXIR_ADD(AMotionEvent_getHistorySize);
  ELIXIR_ADD(AMotionEvent_getHistoricalEventTime);
  ELIXIR_ADD(AMotionEvent_getHistoricalAxisValue);
#undef ELIXIR_ADD
}

}
