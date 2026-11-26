// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "android/media_decoder.h"

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wmcodecdsp.h>
#include <wrl/client.h>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace elixir::android::media {
namespace {

using Microsoft::WRL::ComPtr;

constexpr UINT32 kMaxDimension = 4096;

struct OutputLayout {
  UINT32 width = 0;
  UINT32 height = 0;
  UINT32 stride = 0;
  UINT32 left = 0;
  UINT32 top = 0;
  UINT32 visible_width = 0;
  UINT32 visible_height = 0;

  DWORD StorageBytes(UINT32 pitch) const {
    return pitch * (height + (height + 1) / 2);
  }
};

bool ReadLayout(IMFMediaType* type, OutputLayout* out) {
  OutputLayout layout;
  if (FAILED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &layout.width, &layout.height)) ||
      !layout.width || layout.width > kMaxDimension || !layout.height || layout.height > kMaxDimension) return false;
  const UINT32 min_pitch = (layout.width + 1u) & ~1u;
  layout.stride = MFGetAttributeUINT32(type, MF_MT_DEFAULT_STRIDE, min_pitch);
  if (layout.stride < min_pitch || layout.stride > std::numeric_limits<int>::max() / (2 * layout.height)) return false;
  layout.visible_width = layout.width;
  layout.visible_height = layout.height;
  MFVideoArea area{};
  if (SUCCEEDED(type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, reinterpret_cast<UINT8*>(&area), sizeof(area), nullptr)) ||
      SUCCEEDED(type->GetBlob(MF_MT_GEOMETRIC_APERTURE, reinterpret_cast<UINT8*>(&area), sizeof(area), nullptr))) {
    if (area.OffsetX.value < 0 || area.OffsetY.value < 0 || area.OffsetX.fract || area.OffsetY.fract ||
        area.Area.cx <= 0 || area.Area.cy <= 0) return false;
    layout.left = area.OffsetX.value;
    layout.top = area.OffsetY.value;
    layout.visible_width = area.Area.cx;
    layout.visible_height = area.Area.cy;
    if ((layout.left & 1) || (layout.top & 1) || layout.left + layout.visible_width > layout.width ||
        layout.top + layout.visible_height > layout.height) return false;
  }
  *out = layout;
  return true;
}

bool CopyPicture(const OutputLayout& layout, const BYTE* data, DWORD size, UINT32 pitch, VideoFrame* frame) {
  if (size < layout.StorageBytes(pitch)) return false;
  const UINT32 output_pitch = (layout.visible_width + 1u) & ~1u;
  const UINT32 chroma_rows = (layout.visible_height + 1) / 2;
  frame->bytes.assign(output_pitch * (layout.visible_height + chroma_rows), 0);
  BYTE* dest = frame->bytes.data();
  const BYTE* luma = data + layout.top * pitch + layout.left;
  for (UINT32 y = 0; y < layout.visible_height; ++y)
    std::memcpy(dest + y * output_pitch, luma + y * pitch, layout.visible_width);
  dest += layout.visible_height * output_pitch;
  const BYTE* chroma = data + (layout.height + layout.top / 2) * pitch + layout.left;
  for (UINT32 y = 0; y < chroma_rows; ++y) std::memcpy(dest + y * output_pitch, chroma + y * pitch, output_pitch);
  frame->format = {static_cast<int>(layout.visible_width), static_cast<int>(layout.visible_height),
                   static_cast<int>(output_pitch), static_cast<int>(layout.visible_height)};
  return true;
}

class LockedBuffer {
 public:
  explicit LockedBuffer(IMFMediaBuffer* buffer) : buffer_(buffer) {}
  ~LockedBuffer() { if (locked_) buffer_->Unlock(); }
  bool Lock() {
    locked_ = SUCCEEDED(buffer_->Lock(&data, nullptr, &size));
    return locked_;
  }
  BYTE* data = nullptr;
  DWORD size = 0;
 private:
  IMFMediaBuffer* buffer_;
  bool locked_ = false;
};

bool ReadFrame(IMFSample* sample, const OutputLayout& layout, VideoFrame* frame) {
  LONGLONG pts = 0;
  ComPtr<IMFMediaBuffer> buffer;
  if (FAILED(sample->GetSampleTime(&pts)) || FAILED(sample->ConvertToContiguousBuffer(&buffer))) return false;
  frame->time_us = pts / 10;
  ComPtr<IMF2DBuffer> image;
  if (SUCCEEDED(buffer.As(&image))) {
    const UINT32 pitch = (layout.width + 1u) & ~1u;
    DWORD length = 0;
    if (FAILED(image->GetContiguousLength(&length)) || length != layout.StorageBytes(pitch)) return false;
    std::vector<BYTE> packed(length);
    return SUCCEEDED(image->ContiguousCopyTo(packed.data(), length)) &&
           CopyPicture(layout, packed.data(), length, pitch, frame);
  }
  LockedBuffer bytes(buffer.Get());
  return bytes.Lock() && CopyPicture(layout, bytes.data, bytes.size, layout.stride, frame);
}

bool MakeOutputSample(IMFTransform* transform, const OutputLayout& layout, IMFSample** sample) {
  MFT_OUTPUT_STREAM_INFO stream{};
  if (FAILED(transform->GetOutputStreamInfo(0, &stream))) return false;
  if (stream.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) return true;
  ComPtr<IMFMediaBuffer> buffer;
  ComPtr<IMFSample> result;
  const DWORD capacity = std::max(stream.cbSize, layout.StorageBytes(layout.stride));
  DWORD mask = stream.cbAlignment;
  if (mask > 0 && (mask & (mask - 1)) == 0) --mask;
  mask = std::max<DWORD>(MF_64_BYTE_ALIGNMENT, mask);
  if (FAILED(MFCreateSample(&result)) || FAILED(MFCreateAlignedMemoryBuffer(capacity, mask, &buffer)) ||
      FAILED(result->AddBuffer(buffer.Get()))) return false;
  *sample = result.Detach();
  return true;
}

class Transform {
 public:
  bool Open() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return false;
    com_ = true;
    platform_ = LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!platform_ || FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) return false;
    started_ = true;
    return SUCCEEDED(CoCreateInstance(CLSID_CMSH264DecoderMFT, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&transform_)));
  }

  ~Transform() {
    empty_sample_.Reset();
    transform_.Reset();
    if (started_) MFShutdown();
    if (platform_) FreeLibrary(platform_);
    if (com_) CoUninitialize();
  }

  bool Configure(int width, int height) {
    if (width <= 0 || height <= 0 || width > static_cast<int>(kMaxDimension) ||
        height > static_cast<int>(kMaxDimension)) return false;
    if (FAILED(transform_->SetOutputType(0, nullptr, 0)) || FAILED(transform_->SetInputType(0, nullptr, 0))) return false;
    ComPtr<IMFMediaType> input;
    return SUCCEEDED(MFCreateMediaType(&input)) &&
           SUCCEEDED(input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
           SUCCEEDED(input->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264)) &&
           SUCCEEDED(MFSetAttributeSize(input.Get(), MF_MT_FRAME_SIZE, width, height)) &&
           SUCCEEDED(input->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive)) &&
           SUCCEEDED(transform_->SetInputType(0, input.Get(), 0)) && SelectOutput();
  }

  bool Start() {
    return Message(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING) && Message(MFT_MESSAGE_NOTIFY_START_OF_STREAM);
  }
  bool Flush() {
    return Message(MFT_MESSAGE_COMMAND_FLUSH) && Message(MFT_MESSAGE_NOTIFY_START_OF_STREAM);
  }
  bool Stop() {
    return Message(MFT_MESSAGE_COMMAND_FLUSH) && Message(MFT_MESSAGE_NOTIFY_END_STREAMING);
  }
  bool Drain() {
    return Message(MFT_MESSAGE_NOTIFY_END_OF_STREAM) && Message(MFT_MESSAGE_COMMAND_DRAIN);
  }

  DecodeResult Submit(const std::uint8_t* data, std::size_t size, std::int64_t time_us) {
    if (!data || !size || size > MAXDWORD || time_us > std::numeric_limits<LONGLONG>::max() / 10 ||
        time_us < std::numeric_limits<LONGLONG>::min() / 10) return DecodeResult::kError;
    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(MFCreateSample(&sample)) || FAILED(MFCreateMemoryBuffer(static_cast<DWORD>(size), &buffer)))
      return DecodeResult::kError;
    {
      LockedBuffer bytes(buffer.Get());
      if (!bytes.Lock()) return DecodeResult::kError;
      std::memcpy(bytes.data, data, size);
    }
    if (FAILED(buffer->SetCurrentLength(static_cast<DWORD>(size))) || FAILED(sample->AddBuffer(buffer.Get())) ||
        FAILED(sample->SetSampleTime(time_us * 10))) return DecodeResult::kError;
    const HRESULT hr = transform_->ProcessInput(0, sample.Get(), 0);
    if (hr == MF_E_NOTACCEPTING) return DecodeResult::kAgain;
    return SUCCEEDED(hr) ? DecodeResult::kOk : DecodeResult::kError;
  }

  DecodeResult Receive(VideoFrame* frame) {
    for (int pass = 0; pass < 2; ++pass) {
      MFT_OUTPUT_DATA_BUFFER output{};
      DWORD status = 0;
      if (empty_sample_) output.pSample = empty_sample_.Detach();
      else if (!MakeOutputSample(transform_.Get(), layout_, &output.pSample)) return DecodeResult::kError;
      const HRESULT hr = transform_->ProcessOutput(0, 1, &output, &status);
      ComPtr<IMFSample> sample;
      sample.Attach(output.pSample);
      if (output.pEvents) output.pEvents->Release();
      if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        empty_sample_ = std::move(sample);
        return DecodeResult::kAgain;
      }
      if (hr == MF_E_TRANSFORM_STREAM_CHANGE && SelectOutput()) continue;
      if (FAILED(hr) || !sample || !ReadFrame(sample.Get(), layout_, frame)) return DecodeResult::kError;
      return DecodeResult::kOk;
    }
    return DecodeResult::kError;
  }

 private:
  bool Message(MFT_MESSAGE_TYPE message) { return SUCCEEDED(transform_->ProcessMessage(message, 0)); }

  bool SelectOutput() {
    ComPtr<IMFMediaType> type;
    for (DWORD index = 0; SUCCEEDED(transform_->GetOutputAvailableType(0, index, type.ReleaseAndGetAddressOf())); ++index) {
      GUID subtype{};
      OutputLayout candidate;
      if (SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE, &subtype)) && subtype == MFVideoFormat_NV12 &&
          ReadLayout(type.Get(), &candidate) && SUCCEEDED(transform_->SetOutputType(0, type.Get(), 0))) {
        empty_sample_.Reset();
        layout_ = candidate;
        return true;
      }
    }
    return false;
  }

  bool com_ = false;
  bool started_ = false;
  HMODULE platform_ = nullptr;
  ComPtr<IMFTransform> transform_;
  ComPtr<IMFSample> empty_sample_;
  OutputLayout layout_;
};

}

class AvcDecoder::Impl {
 public:
  Impl() : thread_([this] { Loop(); }) {}
  ~Impl() {
    {
      std::lock_guard lock(mutex_);
      closing_ = true;
    }
    ready_.notify_one();
    thread_.join();
  }

  template <typename Fn>
  auto Run(Fn fn) {
    using Result = decltype(fn(std::declval<Transform&>()));
    std::lock_guard serialize(calls_);
    std::unique_lock lock(mutex_);
    Result result{};
    std::exception_ptr error;
    work_ = [&](Transform& transform) {
      try { result = fn(transform); } catch (...) { error = std::current_exception(); }
    };
    finished_ = false;
    ready_.notify_one();
    completed_.wait(lock, [&] { return finished_; });
    if (error) std::rethrow_exception(error);
    return result;
  }

 private:
  void Loop() {
    Transform transform;
    for (;;) {
      std::function<void(Transform&)> next;
      {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [&] { return closing_ || work_; });
        if (closing_) return;
        next = std::move(work_);
        work_ = nullptr;
      }
      next(transform);
      {
        std::lock_guard lock(mutex_);
        finished_ = true;
      }
      completed_.notify_one();
    }
  }

  std::mutex calls_, mutex_;
  std::condition_variable ready_, completed_;
  std::function<void(Transform&)> work_;
  bool closing_ = false;
  bool finished_ = false;
  std::thread thread_;
};

AvcDecoder::AvcDecoder(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
AvcDecoder::~AvcDecoder() = default;
std::unique_ptr<AvcDecoder> AvcDecoder::Create() {
  auto impl = std::make_unique<Impl>();
  if (!impl->Run([](Transform& t) { return t.Open(); })) return nullptr;
  return std::unique_ptr<AvcDecoder>(new AvcDecoder(std::move(impl)));
}
bool AvcDecoder::Configure(int width, int height) {
  return impl_->Run([=](Transform& t) { return t.Configure(width, height); });
}
bool AvcDecoder::Start() { return impl_->Run([](Transform& t) { return t.Start(); }); }
bool AvcDecoder::Drain() { return impl_->Run([](Transform& t) { return t.Drain(); }); }
bool AvcDecoder::Flush() { return impl_->Run([](Transform& t) { return t.Flush(); }); }
bool AvcDecoder::Stop() { return impl_->Run([](Transform& t) { return t.Stop(); }); }
DecodeResult AvcDecoder::Submit(const std::uint8_t* data, std::size_t size, std::int64_t time_us) {
  return impl_->Run([=](Transform& t) { return t.Submit(data, size, time_us); });
}
DecodeResult AvcDecoder::Receive(VideoFrame* frame) {
  if (!frame) return DecodeResult::kError;
  return impl_->Run([=](Transform& t) { return t.Receive(frame); });
}

}
