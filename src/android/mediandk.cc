// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail stubs/libmediandk_stub.cc (Apache-2.0, Copyright 2026
// Mocktail Project Authors). Modified by Elixir.
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include "android/android_internal.h"
#include "android/media_decoder.h"
#include "bionic/runtime.h"

namespace {

using media_status_t = std::int32_t;
constexpr media_status_t kMediaOk = 0;
constexpr media_status_t kMediaErrorUnknown = -10000;
constexpr media_status_t kMediaErrorUnsupported = -10002;
constexpr media_status_t kMediaErrorInvalidObject = -10003;
constexpr media_status_t kInvalidParameter = -10004;
constexpr media_status_t kInvalidOperation = -10005;
constexpr std::int64_t kTryAgainLater = -1;
constexpr std::int64_t kFormatChanged = -2;
constexpr std::uint32_t kKeyFrame = 1, kCodecConfig = 2, kEndOfStream = 4;
constexpr std::int32_t kNv12 = 21;
constexpr std::int32_t kFlexibleYuv420 = 0x7f420888;
constexpr std::size_t kMaxAccessUnit = 64u << 20;

enum class ValueType { kInt32, kFloat, kString, kBuffer };

struct Value {
  ValueType type = ValueType::kInt32;
  std::int32_t i32 = 0;
  float f = 0;
  std::string bytes;
};

}

struct AMediaFormat {
  std::map<std::string, Value> values;
  std::string text;
};

extern "C" {
const char* elixir_android_AMEDIAFORMAT_KEY_MIME = "mime";
const char* elixir_android_AMEDIAFORMAT_KEY_WIDTH = "width";
const char* elixir_android_AMEDIAFORMAT_KEY_HEIGHT = "height";
const char* elixir_android_AMEDIAFORMAT_KEY_BIT_RATE = "bitrate";
const char* elixir_android_AMEDIAFORMAT_KEY_FRAME_RATE = "frame-rate";
const char* elixir_android_AMEDIAFORMAT_KEY_COLOR_FORMAT = "color-format";
const char* elixir_android_AMEDIAFORMAT_KEY_CHANNEL_COUNT = "channel-count";
const char* elixir_android_AMEDIAFORMAT_KEY_SAMPLE_RATE = "sample-rate";
const char* elixir_android_AMEDIAFORMAT_KEY_I_FRAME_INTERVAL = "i-frame-interval";
const char* elixir_android_AMEDIAFORMAT_KEY_STRIDE = "stride";
}

using elixir::android::Guarded;
using elixir::android::GuardedVoid;

ELIXIR_ANDROID_FN(AMediaFormat*, AMediaFormat_new)() noexcept { return new (std::nothrow) AMediaFormat(); }

ELIXIR_ANDROID_FN(media_status_t, AMediaFormat_delete)(AMediaFormat* format) noexcept {
  delete format;
  return kMediaOk;
}

ELIXIR_ANDROID_FN(bool, AMediaFormat_getInt32)(AMediaFormat* format, const char* name, std::int32_t* out) noexcept {
  if (out != nullptr) *out = 0;
  if (format == nullptr || name == nullptr || out == nullptr) return false;
  return Guarded(false, [&] {
    const auto it = format->values.find(name);
    if (it == format->values.end() || it->second.type != ValueType::kInt32) return false;
    *out = it->second.i32;
    return true;
  });
}

ELIXIR_ANDROID_FN(bool, AMediaFormat_getBuffer)(AMediaFormat* format, const char* name, void** data,
                                                std::uint64_t* size) noexcept {
  if (format == nullptr || name == nullptr || data == nullptr || size == nullptr) return false;
  return Guarded(false, [&] {
    auto it = format->values.find(name);
    if (it == format->values.end() || it->second.type != ValueType::kBuffer) return false;
    *data = it->second.bytes.data();
    *size = it->second.bytes.size();
    return true;
  });
}

ELIXIR_ANDROID_FN(void, AMediaFormat_setInt32)(AMediaFormat* format, const char* name, std::int32_t value) noexcept {
  if (format == nullptr || name == nullptr) return;
  GuardedVoid([&] {
    Value& slot = format->values[name];
    slot = Value{};
    slot.type = ValueType::kInt32;
    slot.i32 = value;
  });
}

ELIXIR_ANDROID_FN(void, AMediaFormat_setFloat)(AMediaFormat* format, const char* name, float value) noexcept {
  if (format == nullptr || name == nullptr) return;
  GuardedVoid([&] {
    Value& slot = format->values[name];
    slot = Value{};
    slot.type = ValueType::kFloat;
    slot.f = value;
  });
}

ELIXIR_ANDROID_FN(void, AMediaFormat_setString)(AMediaFormat* format, const char* name, const char* value) noexcept {
  if (format == nullptr || name == nullptr || value == nullptr) return;
  GuardedVoid([&] {
    Value& slot = format->values[name];
    slot = Value{};
    slot.type = ValueType::kString;
    slot.bytes = value;
  });
}

ELIXIR_ANDROID_FN(void, AMediaFormat_setBuffer)(AMediaFormat* format, const char* name, const void* data,
                                                std::uint64_t size) noexcept {
  if (format == nullptr || name == nullptr || (data == nullptr && size != 0)) return;
  GuardedVoid([&] {
    Value& slot = format->values[name];
    slot = Value{};
    slot.type = ValueType::kBuffer;
    if (size != 0) slot.bytes.assign(static_cast<const char*>(data), static_cast<std::size_t>(size));
  });
}

ELIXIR_ANDROID_FN(const char*, AMediaFormat_toString)(AMediaFormat* format) noexcept {
  if (format == nullptr) return "{}";
  return Guarded<const char*>("{}", [&] {
    std::string text = "{";
    bool first = true;
    for (const auto& [name, value] : format->values) {
      if (!first) text += ", ";
      first = false;
      char number[64];
      switch (value.type) {
        case ValueType::kInt32:
          std::snprintf(number, sizeof(number), "int32(%d)", value.i32);
          text += name + ": " + number;
          break;
        case ValueType::kFloat:
          std::snprintf(number, sizeof(number), "float(%g)", static_cast<double>(value.f));
          text += name + ": " + number;
          break;
        case ValueType::kString:
          text += name + ": string(" + value.bytes + ")";
          break;
        case ValueType::kBuffer:
          std::snprintf(number, sizeof(number), "data(%zu bytes)", value.bytes.size());
          text += name + ": " + number;
          break;
      }
    }
    text += "}";
    format->text = std::move(text);
    return format->text.c_str();
  });
}

namespace {
namespace media = elixir::android::media;

struct BufferInfo {
  std::int32_t offset;
  std::int32_t size;
  std::int64_t time_us;
  std::uint32_t flags;
};
static_assert(sizeof(BufferInfo) == 24 && offsetof(BufferInfo, time_us) == 8);

enum class CodecState { kCreated, kConfigured, kStarted, kFailed };
enum class OutputSlot { kEmpty, kReady, kLeased };

struct CodecSession {
  explicit CodecSession(std::unique_ptr<media::AvcDecoder> backend) : decoder(std::move(backend)) {}

  std::unique_ptr<media::AvcDecoder> decoder;
  std::mutex mutex;
  std::condition_variable changed;
  CodecState state = CodecState::kCreated;
  unsigned generation = 0;
  std::vector<std::uint8_t> input;
  bool input_leased = false;
  std::vector<std::uint8_t> pending;
  std::int64_t pending_time = 0;
  std::vector<std::uint8_t> configured_csd, queued_csd;
  bool use_configured_csd = true;
  bool input_ended = false, draining = false, eos_delivered = false;
  std::int64_t eos_time = 0;
  bool may_have_output = false;
  OutputSlot output_slot = OutputSlot::kEmpty;
  media::VideoFrame frame;
  media::VideoFormat output_format{};
  bool format_changed = false;
  int width = 0, height = 0;

  void ResetBuffers() {
    input_leased = false;
    pending.clear();
    queued_csd.clear();
    use_configured_csd = true;
    input_ended = false;
    draining = false;
    eos_delivered = false;
    may_have_output = false;
    output_slot = OutputSlot::kEmpty;
    frame = {};
    output_format = {};
    format_changed = false;
    ++generation;
    changed.notify_all();
  }

  void Fail() {
    state = CodecState::kFailed;
    changed.notify_all();
  }

  void Pump() noexcept {
    try {
      PumpAvailable();
    } catch (...) {
      Fail();
    }
  }

  void PumpAvailable() {
    if (state != CodecState::kStarted) return;
    bool progress;
    do {
      progress = false;
      if (!pending.empty()) {
        const auto result = decoder->Submit(pending.data(), pending.size(), pending_time);
        if (result == media::DecodeResult::kError) return Fail();
        if (result == media::DecodeResult::kOk) {
          pending.clear();
          progress = true;
        }
        may_have_output = true;
      }
      if (input_ended && pending.empty() && !draining) {
        if (!decoder->Drain()) return Fail();
        draining = true;
        may_have_output = true;
      }
      if (may_have_output && output_slot == OutputSlot::kEmpty && !eos_delivered) {
        const auto result = decoder->Receive(&frame);
        if (result == media::DecodeResult::kError) return Fail();
        if (result == media::DecodeResult::kOk) {
          const auto& f = frame.format;
          if (frame.bytes.empty() || frame.bytes.size() > INT32_MAX) return Fail();
          format_changed = f.width != output_format.width || f.height != output_format.height ||
                           f.stride != output_format.stride || f.slice_height != output_format.slice_height;
          output_format = f;
          output_slot = OutputSlot::kReady;
          progress = true;
        } else {
          may_have_output = false;
          if (draining) {
            frame = {};
            frame.time_us = eos_time;
            output_slot = OutputSlot::kReady;
            eos_delivered = true;
            progress = true;
          }
        }
      }
      if (progress) changed.notify_all();
    } while (progress && !pending.empty());
  }

  template <typename Predicate>
  bool Wait(std::unique_lock<std::mutex>& lock, std::int64_t timeout_us, Predicate ready) {
    const unsigned before = generation;
    const auto done = [&] { return generation != before || state != CodecState::kStarted || ready(); };
    if (!done() && timeout_us != 0) {
      using namespace std::chrono;
      const auto now = steady_clock::now();
      const auto max_delay = duration_cast<microseconds>(steady_clock::time_point::max() - now).count();
      if (timeout_us < 0 || timeout_us >= max_delay) changed.wait(lock, done);
      else changed.wait_until(lock, now + microseconds(timeout_us), done);
    }
    return generation == before && state == CodecState::kStarted && ready();
  }
};

const Value* FindValue(const AMediaFormat* f, const char* key, ValueType type) {
  const auto it = f->values.find(key);
  return it != f->values.end() && it->second.type == type ? &it->second : nullptr;
}

int FormatInt(const AMediaFormat* f, const char* key, int fallback = 0) {
  const auto* v = FindValue(f, key, ValueType::kInt32);
  return v ? v->i32 : fallback;
}

bool AnnexB(const std::uint8_t* data, std::size_t size) {
  std::size_t n = 0;
  while (n < size && data[n] == 0) ++n;
  return n >= 2 && n + 1 < size && data[n] == 1;
}

}

struct AMediaCodec {
  std::shared_ptr<CodecSession> session;
};

namespace {
AMediaCodec* CreateAvcCodec() noexcept {
  return Guarded<AMediaCodec*>(nullptr, []() -> AMediaCodec* {
    auto backend = media::AvcDecoder::Create();
    if (!backend) return nullptr;
    return new AMediaCodec{std::make_shared<CodecSession>(std::move(backend))};
  });
}

template <typename R, typename F>
R WithCodec(AMediaCodec* codec, R failure, F fn) noexcept {
  if (!codec) return failure;
  return Guarded(failure, [&]() -> R {
    auto session = codec->session;
    std::unique_lock<std::mutex> lock(session->mutex);
    return fn(*session, lock);
  });
}
}

ELIXIR_ANDROID_FN(AMediaCodec*, AMediaCodec_createDecoderByType)(const char* mime) noexcept {
  return mime && _stricmp(mime, "video/avc") == 0 ? CreateAvcCodec() : nullptr;
}
ELIXIR_ANDROID_FN(AMediaCodec*, AMediaCodec_createCodecByName)(const char* name) noexcept {
  return name && std::strcmp(name, media::kAvcDecoderName) == 0 ? CreateAvcCodec() : nullptr;
}
ELIXIR_ANDROID_FN(AMediaCodec*, AMediaCodec_createEncoderByType)(const char*) noexcept { return nullptr; }

ELIXIR_ANDROID_FN(media_status_t, AMediaCodec_getName)(AMediaCodec* codec, char** out_name) noexcept {
  if (!out_name) return kInvalidParameter;
  *out_name = nullptr;
  if (!codec) return kMediaErrorInvalidObject;
  auto* name = new (std::nothrow) char[sizeof(media::kAvcDecoderName)];
  if (!name) return kMediaErrorUnknown;
  std::memcpy(name, media::kAvcDecoderName, sizeof(media::kAvcDecoderName));
  *out_name = name;
  return kMediaOk;
}
ELIXIR_ANDROID_FN(void, AMediaCodec_releaseName)(AMediaCodec*, char* name) noexcept { delete[] name; }

ELIXIR_ANDROID_FN(media_status_t, AMediaCodec_delete)(AMediaCodec* codec) noexcept {
  if (!codec) return kMediaOk;
  {
    std::lock_guard<std::mutex> lock(codec->session->mutex);
    codec->session->state = CodecState::kCreated;
    codec->session->ResetBuffers();
  }
  delete codec;
  return kMediaOk;
}

ELIXIR_ANDROID_FN(media_status_t, AMediaCodec_configure)(AMediaCodec* codec, const AMediaFormat* format,
                                                         void* surface, void* crypto, std::uint32_t flags) noexcept {
  if (!format) return kInvalidParameter;
  if (surface || crypto || flags != 0) return kMediaErrorUnsupported;
  return WithCodec(codec, kMediaErrorInvalidObject, [&](CodecSession& s, auto&) {
    if (s.state != CodecState::kCreated) return kInvalidOperation;
    const auto* mime = FindValue(format, "mime", ValueType::kString);
    if (!mime || _stricmp(mime->bytes.c_str(), "video/avc") != 0) return kInvalidParameter;
    const int width = FormatInt(format, "width"), height = FormatInt(format, "height");
    const int hint = FormatInt(format, "max-input-size");
    const int color = FormatInt(format, "color-format", kNv12);
    if (width <= 0 || height <= 0 || hint < 0) return kInvalidParameter;
    if (color != kNv12 && color != kFlexibleYuv420) return kMediaErrorUnsupported;
    const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
    const std::uint64_t capacity = std::max<std::uint64_t>({65536, pixels * 3 / 2, static_cast<unsigned>(hint)});
    if (capacity > kMaxAccessUnit) return kMediaErrorUnsupported;

    std::vector<std::uint8_t> csd;
    for (const char* key : {"csd-0", "csd-1"}) {
      const auto* v = FindValue(format, key, ValueType::kBuffer);
      if (!v || v->bytes.empty()) continue;
      const auto* data = reinterpret_cast<const std::uint8_t*>(v->bytes.data());
      if (v->bytes.size() > kMaxAccessUnit - csd.size() || !AnnexB(data, v->bytes.size())) return kInvalidParameter;
      csd.insert(csd.end(), data, data + v->bytes.size());
    }
    std::vector<std::uint8_t> input(static_cast<std::size_t>(capacity));
    if (!s.decoder->Configure(width, height)) return kMediaErrorUnsupported;
    s.input = std::move(input);
    s.configured_csd = std::move(csd);
    s.width = width;
    s.height = height;
    s.state = CodecState::kConfigured;
    return kMediaOk;
  });
}

ELIXIR_ANDROID_FN(media_status_t, AMediaCodec_start)(AMediaCodec* codec) noexcept {
  return WithCodec(codec, kMediaErrorInvalidObject, [](CodecSession& s, auto&) {
    if (s.state != CodecState::kConfigured) return kInvalidOperation;
    if (!s.decoder->Start()) {
      s.Fail();
      return kMediaErrorUnknown;
    }
    s.ResetBuffers();
    s.state = CodecState::kStarted;
    return kMediaOk;
  });
}
ELIXIR_ANDROID_FN(media_status_t, AMediaCodec_stop)(AMediaCodec* codec) noexcept {
  if (!codec) return kMediaOk;
  return WithCodec(codec, kMediaErrorUnknown, [](CodecSession& s, auto&) {
    if (s.state == CodecState::kCreated) return kInvalidOperation;
    if (!s.decoder->Stop()) {
      s.Fail();
      return kMediaErrorUnknown;
    }
    s.ResetBuffers();
    s.input.clear();
    s.configured_csd.clear();
    s.state = CodecState::kCreated;
    return kMediaOk;
  });
}
ELIXIR_ANDROID_FN(media_status_t, AMediaCodec_flush)(AMediaCodec* codec) noexcept {
  if (!codec) return kMediaOk;
  return WithCodec(codec, kMediaErrorUnknown, [](CodecSession& s, auto&) {
    if (s.state != CodecState::kStarted) return kInvalidOperation;
    if (!s.decoder->Flush()) {
      s.Fail();
      return kMediaErrorUnknown;
    }
    s.ResetBuffers();
    return kMediaOk;
  });
}

ELIXIR_ANDROID_FN(std::int64_t, AMediaCodec_dequeueInputBuffer)(AMediaCodec* codec, std::int64_t timeout_us) noexcept {
  return WithCodec(codec, kTryAgainLater, [&](CodecSession& s, auto& lock) -> std::int64_t {
    if (s.state != CodecState::kStarted) return kInvalidOperation;
    const auto ready = [&] { return s.input_ended || (!s.input_leased && s.pending.empty()); };
    if (!s.Wait(lock, timeout_us, ready) || s.input_ended) {
      return s.state == CodecState::kFailed ? kMediaErrorUnknown : kTryAgainLater;
    }
    s.input_leased = true;
    return 0;
  });
}
ELIXIR_ANDROID_FN(std::uint8_t*, AMediaCodec_getInputBuffer)(AMediaCodec* codec, std::uint64_t index,
                                                             std::uint64_t* out_size) noexcept {
  if (out_size) *out_size = 0;
  return WithCodec<std::uint8_t*>(codec, nullptr, [&](CodecSession& s, auto&) -> std::uint8_t* {
    if (index != 0 || s.state != CodecState::kStarted || !s.input_leased) return nullptr;
    if (out_size) *out_size = s.input.size();
    return s.input.data();
  });
}
ELIXIR_ANDROID_FN(media_status_t, AMediaCodec_queueInputBuffer)(AMediaCodec* codec, std::uint64_t index,
                                                                std::int64_t offset, std::uint64_t size,
                                                                std::uint64_t time_us, std::uint32_t flags) noexcept {
  return WithCodec(codec, kMediaErrorInvalidObject, [&](CodecSession& s, auto&) {
    if (s.state != CodecState::kStarted) return kInvalidOperation;
    if (index != 0 || !s.input_leased || offset < 0 || static_cast<std::uint64_t>(offset) > s.input.size() ||
        size > s.input.size() - static_cast<std::size_t>(offset) || time_us > INT64_MAX / 10) return kInvalidParameter;
    if (flags & ~(kKeyFrame | kCodecConfig | kEndOfStream)) return kMediaErrorUnsupported;
    const auto* data = s.input.data() + offset;
    if (size && !AnnexB(data, size)) return kInvalidParameter;
    if (flags & kCodecConfig) {
      if (size > kMaxAccessUnit - s.queued_csd.size()) return kInvalidParameter;
      s.queued_csd.insert(s.queued_csd.end(), data, data + size);
      s.use_configured_csd = false;
    } else if (size) {
      const auto& prefix = s.use_configured_csd ? s.configured_csd : s.queued_csd;
      if (size > kMaxAccessUnit - prefix.size()) return kInvalidParameter;
      s.pending.reserve(prefix.size() + size);
      s.pending.assign(prefix.begin(), prefix.end());
      s.pending.insert(s.pending.end(), data, data + size);
      s.pending_time = static_cast<std::int64_t>(time_us);
      s.queued_csd.clear();
      s.use_configured_csd = false;
    }
    s.input_leased = false;
    if (flags & kEndOfStream) {
      s.input_ended = true;
      s.eos_time = static_cast<std::int64_t>(time_us);
    }
    s.Pump();
    s.changed.notify_all();
    return s.state == CodecState::kFailed ? kMediaErrorUnknown : kMediaOk;
  });
}

ELIXIR_ANDROID_FN(std::int64_t, AMediaCodec_dequeueOutputBuffer)(AMediaCodec* codec, BufferInfo* info,
                                                                 std::int64_t timeout_us) noexcept {
  if (!info) return kInvalidParameter;
  return WithCodec(codec, kTryAgainLater, [&](CodecSession& s, auto& lock) -> std::int64_t {
    if (s.state != CodecState::kStarted) return kInvalidOperation;
    const auto ready = [&] {
      return s.output_slot == OutputSlot::kReady || (s.eos_delivered && s.output_slot == OutputSlot::kEmpty);
    };
    if (!s.Wait(lock, timeout_us, ready) || s.output_slot != OutputSlot::kReady) {
      return s.state == CodecState::kFailed ? kMediaErrorUnknown : kTryAgainLater;
    }
    if (s.format_changed) {
      s.format_changed = false;
      return kFormatChanged;
    }
    *info = {0, static_cast<std::int32_t>(s.frame.bytes.size()), s.frame.time_us,
             s.eos_delivered ? kEndOfStream : 0};
    s.output_slot = OutputSlot::kLeased;
    return 0;
  });
}
ELIXIR_ANDROID_FN(std::uint8_t*, AMediaCodec_getOutputBuffer)(AMediaCodec* codec, std::uint64_t index,
                                                              std::uint64_t* out_size) noexcept {
  if (out_size) *out_size = 0;
  return WithCodec<std::uint8_t*>(codec, nullptr, [&](CodecSession& s, auto&) -> std::uint8_t* {
    if (index != 0 || s.state != CodecState::kStarted || s.output_slot != OutputSlot::kLeased) return nullptr;
    if (out_size) *out_size = s.frame.bytes.size();
    return s.frame.bytes.data();
  });
}
ELIXIR_ANDROID_FN(media_status_t, AMediaCodec_releaseOutputBuffer)(AMediaCodec* codec, std::uint64_t index,
                                                                   bool ) noexcept {
  if (!codec) return kMediaOk;
  return WithCodec(codec, kMediaErrorUnknown, [&](CodecSession& s, auto&) {
    if (s.state != CodecState::kStarted) return kInvalidOperation;
    if (index != 0 || s.output_slot != OutputSlot::kLeased) return kInvalidParameter;
    s.output_slot = OutputSlot::kEmpty;
    s.Pump();
    s.changed.notify_all();
    return s.state == CodecState::kFailed ? kMediaErrorUnknown : kMediaOk;
  });
}
ELIXIR_ANDROID_FN(AMediaFormat*, AMediaCodec_getOutputFormat)(AMediaCodec* codec) noexcept {
  return WithCodec<AMediaFormat*>(codec, nullptr, [](CodecSession& s, auto&) -> AMediaFormat* {
    if (s.state != CodecState::kConfigured && s.state != CodecState::kStarted) return nullptr;
    auto f = std::make_unique<AMediaFormat>();
    f->values["mime"] = {ValueType::kString, 0, 0.0f, "video/raw"};
    const bool decoded = s.output_format.width > 0;
    f->values["width"].i32 = decoded ? s.output_format.width : s.width;
    f->values["height"].i32 = decoded ? s.output_format.height : s.height;
    f->values["color-format"].i32 = kNv12;
    if (decoded) {
      f->values["stride"].i32 = s.output_format.stride;
      f->values["slice-height"].i32 = s.output_format.slice_height;
    }
    return f.release();
  });
}

struct AImageReader;
struct AImage;
struct AHardwareBuffer;
struct ANativeWindow;

ELIXIR_ANDROID_FN(media_status_t, AImageReader_newWithUsage)(std::int32_t width, std::int32_t height,
                                                             std::int32_t format, std::uint64_t usage,
                                                             std::int32_t max_images, AImageReader** reader) noexcept {
  if (reader == nullptr) return kMediaErrorUnsupported;
  *reader = nullptr;
  return kMediaErrorUnsupported;
}
ELIXIR_ANDROID_FN(void, AImageReader_delete)(AImageReader* reader) noexcept {}
ELIXIR_ANDROID_FN(media_status_t, AImageReader_getWindow)(AImageReader* reader, ANativeWindow** window) noexcept {
  if (window != nullptr) *window = nullptr;
  return kMediaErrorInvalidObject;
}
ELIXIR_ANDROID_FN(media_status_t, AImageReader_acquireNextImage)(AImageReader* reader, AImage** image) noexcept {
  if (image != nullptr) *image = nullptr;
  return kMediaErrorInvalidObject;
}
ELIXIR_ANDROID_FN(void, AImage_delete)(AImage* image) noexcept {}
ELIXIR_ANDROID_FN(media_status_t, AImage_getTimestamp)(const AImage* image, std::int64_t* timestamp_ns) noexcept {
  return kMediaErrorInvalidObject;
}
ELIXIR_ANDROID_FN(media_status_t, AImage_getHardwareBuffer)(const AImage* image, AHardwareBuffer** buffer) noexcept {
  if (buffer != nullptr) *buffer = nullptr;
  return kMediaErrorInvalidObject;
}

namespace elixir::android {

void RegisterMediaExports(bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibMediandk, #name, reinterpret_cast<void*>(&elixir_android_##name))
  ELIXIR_ADD(AMEDIAFORMAT_KEY_MIME);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_WIDTH);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_HEIGHT);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_BIT_RATE);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_FRAME_RATE);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_COLOR_FORMAT);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_CHANNEL_COUNT);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_SAMPLE_RATE);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_I_FRAME_INTERVAL);
  ELIXIR_ADD(AMEDIAFORMAT_KEY_STRIDE);
  ELIXIR_ADD(AMediaFormat_new);
  ELIXIR_ADD(AMediaFormat_delete);
  ELIXIR_ADD(AMediaFormat_getInt32);
  ELIXIR_ADD(AMediaFormat_getBuffer);
  ELIXIR_ADD(AMediaFormat_setInt32);
  ELIXIR_ADD(AMediaFormat_setFloat);
  ELIXIR_ADD(AMediaFormat_setString);
  ELIXIR_ADD(AMediaFormat_setBuffer);
  ELIXIR_ADD(AMediaFormat_toString);
  ELIXIR_ADD(AMediaCodec_createDecoderByType);
  ELIXIR_ADD(AMediaCodec_createEncoderByType);
  ELIXIR_ADD(AMediaCodec_getName);
  ELIXIR_ADD(AMediaCodec_releaseName);
  ELIXIR_ADD(AMediaCodec_delete);
  ELIXIR_ADD(AMediaCodec_configure);
  ELIXIR_ADD(AMediaCodec_start);
  ELIXIR_ADD(AMediaCodec_stop);
  ELIXIR_ADD(AMediaCodec_flush);
  ELIXIR_ADD(AMediaCodec_getInputBuffer);
  ELIXIR_ADD(AMediaCodec_getOutputBuffer);
  ELIXIR_ADD(AMediaCodec_dequeueInputBuffer);
  ELIXIR_ADD(AMediaCodec_dequeueOutputBuffer);
  ELIXIR_ADD(AMediaCodec_queueInputBuffer);
  ELIXIR_ADD(AMediaCodec_releaseOutputBuffer);
  ELIXIR_ADD(AMediaCodec_getOutputFormat);
  ELIXIR_ADD(AMediaCodec_createCodecByName);
  ELIXIR_ADD(AImageReader_newWithUsage);
  ELIXIR_ADD(AImageReader_delete);
  ELIXIR_ADD(AImageReader_getWindow);
  ELIXIR_ADD(AImageReader_acquireNextImage);
  ELIXIR_ADD(AImage_delete);
  ELIXIR_ADD(AImage_getTimestamp);
  ELIXIR_ADD(AImage_getHardwareBuffer);
#undef ELIXIR_ADD
}

}
