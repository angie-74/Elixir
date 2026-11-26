// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "audio/opensl_recorder.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_set>

#include "audio/wasapi_audio.h"
#include "mocktail/audio/audio_sink.h"
#include "mocktail/audio/opensl_playback_runtime.h"
#include "mocktail/audio/sdl_audio_capture.h"

namespace mocktail::audio {
namespace {

namespace abi = opensl_abi;

constexpr abi::Result kPreconditionsViolated = 1;
constexpr abi::Result kMemoryFailure = 3;
constexpr abi::Result kResourceError = 4;
constexpr abi::Result kBufferInsufficient = 7;
constexpr abi::Result kPermissionDenied = 11;
constexpr abi::Uint32 kLocatorIoDevice = 3;
constexpr abi::Uint32 kIoDeviceAudioInput = 1;
constexpr abi::Uint32 kRecordStateStopped = 1;
constexpr abi::Uint32 kRecordStatePaused = 2;
constexpr abi::Uint32 kRecordStateRecording = 3;
constexpr abi::Uint32 kRecordingPresetGeneric = 1;
constexpr abi::Uint32 kMaximumQueuedBuffers = 255;
constexpr std::uint64_t kRecorderMagic = 0x454c585245434f52ull;

struct IoDeviceLocator {
  abi::Uint32 locator_type;
  abi::Uint32 device_type;
  abi::Uint32 device_id;
  void* device;
};

struct RecordTable;
using Record = const RecordTable* const*;
using RecordCallback = void (MOCKTAIL_OPENSL_API_ENTRY*)(Record caller, void* context, abi::Uint32 event);

struct RecordTable {
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* SetRecordState)(Record self, abi::Uint32 state);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* GetRecordState)(Record self, abi::Uint32* state);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* SetDurationLimit)(Record self, abi::Uint32 milliseconds);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* GetPosition)(Record self, abi::Uint32* milliseconds);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* RegisterCallback)(Record self, RecordCallback callback, void* context);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* SetCallbackEventsMask)(Record self, abi::Uint32 mask);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* GetCallbackEventsMask)(Record self, abi::Uint32* mask);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* SetMarkerPosition)(Record self, abi::Uint32 milliseconds);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* ClearMarkerPosition)(Record self);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* GetMarkerPosition)(Record self, abi::Uint32* milliseconds);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* SetPositionUpdatePeriod)(Record self, abi::Uint32 milliseconds);
  abi::Result (MOCKTAIL_OPENSL_API_ENTRY* GetPositionUpdatePeriod)(Record self, abi::Uint32* milliseconds);
};

struct Recorder;

struct ObjectSlot {
  const abi::ObjectTable* table = nullptr;
  Recorder* owner = nullptr;
};
struct RecordSlot {
  const RecordTable* table = nullptr;
  Recorder* owner = nullptr;
};
struct QueueSlot {
  const abi::AndroidSimpleBufferQueueTable* table = nullptr;
  Recorder* owner = nullptr;
};
struct ConfigurationSlot {
  const abi::AndroidConfigurationTable* table = nullptr;
  Recorder* owner = nullptr;
};

extern const abi::ObjectTable kObjectTable;
extern const RecordTable kRecordTable;
extern const abi::AndroidSimpleBufferQueueTable kQueueTable;
extern const abi::AndroidConfigurationTable kConfigurationTable;

std::mutex g_recorders_mutex;
std::unordered_set<Recorder*> g_recorders;

struct PendingBuffer {
  char* data = nullptr;
  abi::Uint32 size = 0;
  abi::Uint32 filled = 0;
};

struct Recorder {
  Recorder() {
    object = ObjectSlot{&kObjectTable, this};
    record = RecordSlot{&kRecordTable, this};
    queue = QueueSlot{&kQueueTable, this};
    configuration = ConfigurationSlot{&kConfigurationTable, this};
    std::lock_guard<std::mutex> lock(g_recorders_mutex);
    g_recorders.insert(this);
  }
  ~Recorder() {
    std::lock_guard<std::mutex> lock(g_recorders_mutex);
    g_recorders.erase(this);
  }

  abi::AndroidSimpleBufferQueue QueueInterface() {
    return reinterpret_cast<abi::AndroidSimpleBufferQueue>(&queue.table);
  }

  std::uint64_t magic = kRecorderMagic;
  ObjectSlot object;
  RecordSlot record;
  QueueSlot queue;
  ConfigurationSlot configuration;
  std::mutex mutex;
  PcmSpec spec;
  abi::Uint32 capacity = 0;
  abi::Uint32 object_state = abi::kObjectStateUnrealized;
  abi::Uint32 record_state = kRecordStateStopped;
  abi::Uint32 preset = kRecordingPresetGeneric;
  abi::Uint32 completed = 0;
  std::uint64_t captured_bytes = 0;
  std::deque<PendingBuffer> buffers;
  abi::AndroidSimpleBufferQueueCallback callback = nullptr;
  void* callback_context = nullptr;
  std::unique_ptr<AudioCapture> capture;
  bool reported_data = false;
};

template <typename Slot, typename Interface>
Recorder* Resolve(Interface self) {
  if (self == nullptr) return nullptr;
  Recorder* recorder = reinterpret_cast<const Slot*>(self)->owner;
  if (recorder == nullptr) return nullptr;
  std::lock_guard<std::mutex> lock(g_recorders_mutex);
  if (g_recorders.find(recorder) == g_recorders.end() || recorder->magic != kRecorderMagic) return nullptr;
  return recorder;
}

bool SameInterface(abi::InterfaceId left, abi::InterfaceId right) {
  return left != nullptr && right != nullptr &&
         (left == right || std::memcmp(left, right, sizeof(abi::InterfaceIdValue)) == 0);
}

void OnCaptured(void* context, std::size_t size_bytes) {
  auto* recorder = static_cast<Recorder*>(context);
  abi::AndroidSimpleBufferQueueCallback callback = nullptr;
  void* callback_context = nullptr;
  unsigned finished = 0;
  {
    std::lock_guard<std::mutex> lock(recorder->mutex);
    if (recorder->capture == nullptr || recorder->record_state != kRecordStateRecording) return;
    const char* source = static_cast<const char*>(recorder->capture->buffer_data());
    std::size_t remaining = source != nullptr ? size_bytes : 0;
    recorder->captured_bytes += remaining;
    while (remaining != 0 && !recorder->buffers.empty()) {
      PendingBuffer& front = recorder->buffers.front();
      const std::size_t room = front.size - front.filled;
      const std::size_t taken = remaining < room ? remaining : room;
      std::memcpy(front.data + front.filled, source, taken);
      front.filled += static_cast<abi::Uint32>(taken);
      source += taken;
      remaining -= taken;
      if (front.filled == front.size) {
        recorder->buffers.pop_front();
        ++recorder->completed;
        ++finished;
      }
    }
    callback = recorder->callback;
    callback_context = recorder->callback_context;
    if (!recorder->reported_data && finished != 0) {
      recorder->reported_data = true;
      std::fprintf(stderr, "  [audio] OpenSL recorder delivered its first buffer\n");
    }
  }
  if (callback == nullptr) return;
  for (unsigned index = 0; index < finished; ++index) callback(recorder->QueueInterface(), callback_context);
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ObjectRealize(abi::Object self, abi::Boolean) noexcept {
  Recorder* recorder = Resolve<ObjectSlot>(self);
  if (recorder == nullptr) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  recorder->object_state = abi::kObjectStateRealized;
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ObjectResume(abi::Object self, abi::Boolean) noexcept {
  return Resolve<ObjectSlot>(self) == nullptr ? abi::kResultParameterInvalid : kPreconditionsViolated;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ObjectGetState(abi::Object self, abi::Uint32* state) noexcept {
  Recorder* recorder = Resolve<ObjectSlot>(self);
  if (recorder == nullptr || state == nullptr) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  *state = recorder->object_state;
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ObjectGetInterface(abi::Object self, abi::InterfaceId iid,
                                                         void* interface_out) noexcept {
  Recorder* recorder = Resolve<ObjectSlot>(self);
  if (recorder == nullptr || iid == nullptr || interface_out == nullptr) return abi::kResultParameterInvalid;
  auto** out = static_cast<const void**>(interface_out);
  *out = nullptr;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  if (SameInterface(iid, SL_IID_ANDROIDCONFIGURATION)) {
    *out = &recorder->configuration.table;
    return abi::kResultSuccess;
  }
  const bool record = SameInterface(iid, SL_IID_RECORD);
  const bool queue = SameInterface(iid, SL_IID_ANDROIDSIMPLEBUFFERQUEUE);
  if (!record && !queue) return abi::kResultFeatureUnsupported;
  if (recorder->object_state != abi::kObjectStateRealized) return kPreconditionsViolated;
  *out = record ? static_cast<const void*>(&recorder->record.table) : static_cast<const void*>(&recorder->queue.table);
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ObjectRegisterCallback(abi::Object self, abi::ObjectCallback, void*) noexcept {
  return Resolve<ObjectSlot>(self) == nullptr ? abi::kResultParameterInvalid : abi::kResultSuccess;
}

void MOCKTAIL_OPENSL_API_ENTRY ObjectAbortAsyncOperation(abi::Object) noexcept {}

void MOCKTAIL_OPENSL_API_ENTRY ObjectDestroy(abi::Object self) noexcept {
  Recorder* recorder = Resolve<ObjectSlot>(self);
  if (recorder == nullptr) return;
  std::unique_ptr<AudioCapture> capture;
  {
    std::lock_guard<std::mutex> lock(recorder->mutex);
    recorder->record_state = kRecordStateStopped;
    recorder->callback = nullptr;
    recorder->buffers.clear();
    capture = std::move(recorder->capture);
  }
  if (capture != nullptr) {
    static_cast<void>(capture->Stop());
    capture->Shutdown();
    capture.reset();
  }
  delete recorder;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ObjectSetPriority(abi::Object self, abi::Int32, abi::Boolean) noexcept {
  return Resolve<ObjectSlot>(self) == nullptr ? abi::kResultParameterInvalid : abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ObjectGetPriority(abi::Object self, abi::Int32* priority,
                                                        abi::Boolean* preemptable) noexcept {
  if (Resolve<ObjectSlot>(self) == nullptr || priority == nullptr || preemptable == nullptr) {
    return abi::kResultParameterInvalid;
  }
  *priority = 0;
  *preemptable = abi::kBooleanFalse;
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ObjectSetLossOfControlInterfaces(abi::Object self, abi::Int16,
                                                                       abi::InterfaceId*, abi::Boolean) noexcept {
  return Resolve<ObjectSlot>(self) == nullptr ? abi::kResultParameterInvalid : abi::kResultFeatureUnsupported;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY RecordSetState(Record self, abi::Uint32 state) noexcept {
  Recorder* recorder = Resolve<RecordSlot>(self);
  if (recorder == nullptr ||
      (state != kRecordStateStopped && state != kRecordStatePaused && state != kRecordStateRecording)) {
    return abi::kResultParameterInvalid;
  }
  AudioCapture* capture = nullptr;
  {
    std::lock_guard<std::mutex> lock(recorder->mutex);
    if (state == recorder->record_state) return abi::kResultSuccess;
    if (state == kRecordStateRecording) {
      if (!MicrophoneCapturePermitted()) {
        std::fprintf(stderr, "  [audio] OpenSL recorder refused: the microphone is not permitted\n");
        return kPermissionDenied;
      }
      if (recorder->capture == nullptr) {
        WasapiAudioCaptureOptions options;
        options.output_spec = recorder->spec;
        options.frame_duration_ms = 10;
        options.data_callback = &OnCaptured;
        options.data_context = recorder;
        options.capture_thread_enters_guest = true;
        const Status status = CreateWasapiAudioCapture(options, &recorder->capture);
        if (!status.ok() || recorder->capture == nullptr) {
          recorder->capture.reset();
          std::fprintf(stderr, "  [audio] OpenSL recorder could not open the microphone: %s\n",
                       status.message().c_str());
          return kResourceError;
        }
      }
    }
    recorder->record_state = state;
    capture = recorder->capture.get();
  }
  if (capture == nullptr) return abi::kResultSuccess;
  if (state == kRecordStateRecording) {
    const Status status = capture->Start();
    if (!status.ok()) {
      std::lock_guard<std::mutex> lock(recorder->mutex);
      recorder->record_state = kRecordStateStopped;
      return kResourceError;
    }
    std::fprintf(stderr, "  [audio] OpenSL recorder started: %d Hz, %u channel(s), 16-bit\n",
                 recorder->spec.sample_rate_hz, static_cast<unsigned>(recorder->spec.channels));
    return abi::kResultSuccess;
  }
  static_cast<void>(capture->Stop());
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY RecordGetState(Record self, abi::Uint32* state) noexcept {
  Recorder* recorder = Resolve<RecordSlot>(self);
  if (recorder == nullptr || state == nullptr) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  *state = recorder->record_state;
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY RecordSetValue(Record self, abi::Uint32) noexcept {
  return Resolve<RecordSlot>(self) == nullptr ? abi::kResultParameterInvalid : abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY RecordGetPosition(Record self, abi::Uint32* milliseconds) noexcept {
  Recorder* recorder = Resolve<RecordSlot>(self);
  if (recorder == nullptr || milliseconds == nullptr) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  const std::uint64_t bytes_per_second =
      static_cast<std::uint64_t>(recorder->spec.sample_rate_hz) * recorder->spec.channels * 2;
  *milliseconds = bytes_per_second == 0
                      ? 0
                      : static_cast<abi::Uint32>(recorder->captured_bytes * 1000 / bytes_per_second);
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY RecordRegisterCallback(Record self, RecordCallback, void*) noexcept {
  return Resolve<RecordSlot>(self) == nullptr ? abi::kResultParameterInvalid : abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY RecordGetValue(Record self, abi::Uint32* value) noexcept {
  if (Resolve<RecordSlot>(self) == nullptr || value == nullptr) return abi::kResultParameterInvalid;
  *value = 0;
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY RecordClearMarker(Record self) noexcept {
  return Resolve<RecordSlot>(self) == nullptr ? abi::kResultParameterInvalid : abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY QueueEnqueue(abi::AndroidSimpleBufferQueue self, const void* buffer,
                                                   abi::Uint32 size) noexcept {
  Recorder* recorder = Resolve<QueueSlot>(self);
  if (recorder == nullptr || buffer == nullptr || size == 0) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  if (recorder->buffers.size() >= recorder->capacity) return kBufferInsufficient;
  recorder->buffers.push_back(PendingBuffer{static_cast<char*>(const_cast<void*>(buffer)), size, 0});
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY QueueClear(abi::AndroidSimpleBufferQueue self) noexcept {
  Recorder* recorder = Resolve<QueueSlot>(self);
  if (recorder == nullptr) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  recorder->buffers.clear();
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY QueueGetState(abi::AndroidSimpleBufferQueue self,
                                                    abi::AndroidSimpleBufferQueueState* state) noexcept {
  Recorder* recorder = Resolve<QueueSlot>(self);
  if (recorder == nullptr || state == nullptr) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  state->count = static_cast<abi::Uint32>(recorder->buffers.size());
  state->index = recorder->completed;
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY QueueRegisterCallback(abi::AndroidSimpleBufferQueue self,
                                                            abi::AndroidSimpleBufferQueueCallback callback,
                                                            void* context) noexcept {
  Recorder* recorder = Resolve<QueueSlot>(self);
  if (recorder == nullptr) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  recorder->callback = callback;
  recorder->callback_context = context;
  return abi::kResultSuccess;
}

bool KeyIs(const abi::Char* key, const char* expected) {
  return key != nullptr && std::strcmp(reinterpret_cast<const char*>(key), expected) == 0;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ConfigurationSet(abi::AndroidConfiguration self, const abi::Char* key,
                                                       const void* value, abi::Uint32 value_size) noexcept {
  Recorder* recorder = Resolve<ConfigurationSlot>(self);
  if (recorder == nullptr || key == nullptr || value == nullptr) return abi::kResultParameterInvalid;
  std::lock_guard<std::mutex> lock(recorder->mutex);
  if (recorder->object_state == abi::kObjectStateRealized) return kPreconditionsViolated;
  if (KeyIs(key, "androidRecordingPreset")) {
    if (value_size < sizeof(abi::Uint32)) return kBufferInsufficient;
    std::memcpy(&recorder->preset, value, sizeof(abi::Uint32));
  }
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ConfigurationGet(abi::AndroidConfiguration self, const abi::Char* key,
                                                       abi::Uint32* value_size, void* value) noexcept {
  Recorder* recorder = Resolve<ConfigurationSlot>(self);
  if (recorder == nullptr || key == nullptr || value_size == nullptr) return abi::kResultParameterInvalid;
  if (!KeyIs(key, "androidRecordingPreset")) return abi::kResultParameterInvalid;
  if (value == nullptr || *value_size < sizeof(abi::Uint32)) {
    *value_size = sizeof(abi::Uint32);
    return value == nullptr ? abi::kResultSuccess : kBufferInsufficient;
  }
  std::lock_guard<std::mutex> lock(recorder->mutex);
  std::memcpy(value, &recorder->preset, sizeof(abi::Uint32));
  *value_size = sizeof(abi::Uint32);
  return abi::kResultSuccess;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ConfigurationAcquireProxy(abi::AndroidConfiguration self, abi::Uint32,
                                                                abi::JavaProxyObject* proxy) noexcept {
  if (proxy != nullptr) *proxy = nullptr;
  return Resolve<ConfigurationSlot>(self) == nullptr ? abi::kResultParameterInvalid
                                                     : abi::kResultFeatureUnsupported;
}

abi::Result MOCKTAIL_OPENSL_API_ENTRY ConfigurationReleaseProxy(abi::AndroidConfiguration self, abi::Uint32) noexcept {
  return Resolve<ConfigurationSlot>(self) == nullptr ? abi::kResultParameterInvalid
                                                     : abi::kResultFeatureUnsupported;
}

const abi::ObjectTable kObjectTable = {
    &ObjectRealize,          &ObjectResume,     &ObjectGetState,   &ObjectGetInterface, &ObjectRegisterCallback,
    &ObjectAbortAsyncOperation, &ObjectDestroy, &ObjectSetPriority, &ObjectGetPriority,
    &ObjectSetLossOfControlInterfaces,
};

const RecordTable kRecordTable = {
    &RecordSetState, &RecordGetState,    &RecordSetValue, &RecordGetPosition, &RecordRegisterCallback, &RecordSetValue,
    &RecordGetValue, &RecordSetValue,    &RecordClearMarker, &RecordGetValue, &RecordSetValue,         &RecordGetValue,
};

const abi::AndroidSimpleBufferQueueTable kQueueTable = {
    &QueueEnqueue,
    &QueueClear,
    &QueueGetState,
    &QueueRegisterCallback,
};

const abi::AndroidConfigurationTable kConfigurationTable = {
    &ConfigurationSet,
    &ConfigurationGet,
    &ConfigurationAcquireProxy,
    &ConfigurationReleaseProxy,
};

}

abi::Result CreateOpenSlRecorder(const abi::DataSource* source, const abi::DataSink* sink, abi::Object* recorder) {
  if (recorder == nullptr) return abi::kResultParameterInvalid;
  *recorder = nullptr;
  if (source == nullptr || sink == nullptr || source->locator == nullptr || sink->locator == nullptr ||
      sink->format == nullptr) {
    return abi::kResultParameterInvalid;
  }
  const auto* device = static_cast<const IoDeviceLocator*>(source->locator);
  const auto* queue = static_cast<const abi::DataLocatorAndroidSimpleBufferQueue*>(sink->locator);
  const auto* format = static_cast<const abi::DataFormatPcm*>(sink->format);
  if (device->locator_type != kLocatorIoDevice || device->device_type != kIoDeviceAudioInput ||
      queue->locator_type != abi::kDataLocatorAndroidSimpleBufferQueue || format->format_type != abi::kDataFormatPcm) {
    return abi::kResultContentUnsupported;
  }
  const abi::Uint32 rate = format->samples_per_second_millihertz / 1000;
  if (queue->num_buffers == 0 || queue->num_buffers > kMaximumQueuedBuffers ||
      (format->num_channels != 1 && format->num_channels != 2) || format->bits_per_sample != 16 ||
      format->container_size != 16 || rate < 8000 || rate > 192000) {
    return abi::kResultContentUnsupported;
  }
  auto* created = new (std::nothrow) Recorder();
  if (created == nullptr) return kMemoryFailure;
  created->spec.sample_rate_hz = static_cast<int>(rate);
  created->spec.channels = static_cast<std::uint8_t>(format->num_channels);
  created->spec.format = PcmSampleFormat::kSigned16LittleEndian;
  created->capacity = queue->num_buffers;
  *recorder = reinterpret_cast<abi::Object>(&created->object.table);
  return abi::kResultSuccess;
}

}
