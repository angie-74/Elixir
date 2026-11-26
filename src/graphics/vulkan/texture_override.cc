// Copyright 2026 Mocktail Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from Mocktail's src/graphics/texture_override.cc.
// Modified by Elixir.
#include "graphics/vulkan/texture_override.h"

#include <windows.h>

#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace elixir::vulkan {

using Microsoft::WRL::ComPtr;

std::uint64_t HashBytes(const std::uint8_t* data, std::size_t size) {
  std::uint64_t hash = 0xcbf29ce484222325ULL;
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= data[index];
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

std::string HashName(std::uint64_t hash) {
  char text[17];
  std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
  return text;
}

namespace {

std::wstring Widen(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
  return wide;
}

class ComScope {
 public:
  ComScope() {
    const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    initialized_ = SUCCEEDED(result);
    usable_ = initialized_ || result == RPC_E_CHANGED_MODE;
  }
  ~ComScope() {
    if (initialized_) CoUninitialize();
  }
  ComScope(const ComScope&) = delete;
  ComScope& operator=(const ComScope&) = delete;
  bool usable() const { return usable_; }

 private:
  bool initialized_ = false;
  bool usable_ = false;
};

ComPtr<IWICImagingFactory> CreateFactory() {
  ComPtr<IWICImagingFactory> factory;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(factory.GetAddressOf())))) {
    return nullptr;
  }
  return factory;
}

}

bool ReadPngRgba(const std::string& path, RgbaImage* image) {
  if (image == nullptr) {
    return false;
  }
  ComScope com;
  if (!com.usable()) return false;
  const ComPtr<IWICImagingFactory> factory = CreateFactory();
  if (factory == nullptr) return false;
  const std::wstring wide = Widen(path);
  ComPtr<IWICBitmapDecoder> decoder;
  if (wide.empty() || FAILED(factory->CreateDecoderFromFilename(wide.c_str(), nullptr, GENERIC_READ,
                                                                WICDecodeMetadataCacheOnDemand,
                                                                decoder.GetAddressOf()))) {
    return false;
  }
  GUID container{};
  if (FAILED(decoder->GetContainerFormat(&container)) || container != GUID_ContainerFormatPng) return false;
  ComPtr<IWICBitmapFrameDecode> frame;
  if (FAILED(decoder->GetFrame(0, frame.GetAddressOf()))) return false;
  ComPtr<IWICFormatConverter> converter;
  if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf())) ||
      FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr,
                                   0.0, WICBitmapPaletteTypeCustom))) {
    return false;
  }
  UINT width = 0;
  UINT height = 0;
  if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0 ||
      static_cast<std::uint64_t>(width) * height * 4 > 0x7fffffffULL) {
    return false;
  }
  RgbaImage result;
  result.width = width;
  result.height = height;
  result.pixels.resize(static_cast<std::size_t>(width) * height * 4);
  if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(result.pixels.size()),
                                   result.pixels.data()))) {
    return false;
  }
  *image = std::move(result);
  return true;
}

bool WritePngRgba(const std::string& path, const RgbaImage& image) {
  if (image.width == 0 || image.height == 0 ||
      image.pixels.size() != static_cast<std::size_t>(image.width) * image.height * 4 ||
      image.pixels.size() > 0x7fffffffULL) {
    return false;
  }
  ComScope com;
  if (!com.usable()) return false;
  const ComPtr<IWICImagingFactory> factory = CreateFactory();
  if (factory == nullptr) return false;
  const std::wstring wide = Widen(path);
  ComPtr<IWICStream> stream;
  ComPtr<IWICBitmapEncoder> encoder;
  ComPtr<IWICBitmapFrameEncode> frame;
  ComPtr<IPropertyBag2> options;
  if (wide.empty() || FAILED(factory->CreateStream(stream.GetAddressOf())) ||
      FAILED(stream->InitializeFromFilename(wide.c_str(), GENERIC_WRITE)) ||
      FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf())) ||
      FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
      FAILED(encoder->CreateNewFrame(frame.GetAddressOf(), options.GetAddressOf())) ||
      FAILED(frame->Initialize(options.Get())) || FAILED(frame->SetSize(image.width, image.height))) {
    return false;
  }
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  if (FAILED(frame->SetPixelFormat(&format)) || format != GUID_WICPixelFormat32bppBGRA) return false;
  std::vector<std::uint8_t> bgra(image.pixels.size());
  for (std::size_t i = 0; i < bgra.size(); i += 4) {
    bgra[i] = image.pixels[i + 2];
    bgra[i + 1] = image.pixels[i + 1];
    bgra[i + 2] = image.pixels[i];
    bgra[i + 3] = image.pixels[i + 3];
  }
  return SUCCEEDED(frame->WritePixels(image.height, image.width * 4, static_cast<UINT>(bgra.size()),
                                      bgra.data())) &&
         SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}

void ResampleRgba(const RgbaImage& source, std::uint32_t width, std::uint32_t height,
                  std::uint8_t* destination) {
  if (source.pixels.size() < static_cast<std::size_t>(source.width) * source.height * 4) {
    return;
  }
  ResampleRgba(source.pixels.data(), source.width, source.height, width, height, destination);
}

void ResampleRgba(const std::uint8_t* source, std::uint32_t source_width, std::uint32_t source_height,
                  std::uint32_t width, std::uint32_t height, std::uint8_t* destination) {
  if (source == nullptr || destination == nullptr || width == 0 || height == 0 || source_width == 0 ||
      source_height == 0) {
    return;
  }
  if (width == source_width && height == source_height) {
    if (source != destination) {
      std::memcpy(destination, source, static_cast<std::size_t>(width) * height * 4);
    }
    return;
  }
  if (width % source_width == 0 && height % source_height == 0) {
    const std::uint32_t scale_x = width / source_width;
    const std::uint32_t scale_y = height / source_height;
    const std::size_t row_bytes = static_cast<std::size_t>(width) * 4;
    std::vector<std::uint8_t> row(row_bytes);
    for (std::uint32_t y = 0; y < source_height; ++y) {
      const std::uint8_t* source_row = source + static_cast<std::size_t>(y) * source_width * 4;
      for (std::uint32_t x = 0; x < source_width; ++x) {
        for (std::uint32_t repeat = 0; repeat < scale_x; ++repeat) {
          std::memcpy(row.data() + (static_cast<std::size_t>(x) * scale_x + repeat) * 4,
                      source_row + static_cast<std::size_t>(x) * 4, 4);
        }
      }
      std::uint8_t* target_row = destination + static_cast<std::size_t>(y) * scale_y * row_bytes;
      for (std::uint32_t repeat = 0; repeat < scale_y; ++repeat) {
        std::memcpy(target_row + repeat * row_bytes, row.data(), row_bytes);
      }
    }
    return;
  }
  for (std::uint32_t y = 0; y < height; ++y) {
    const std::uint64_t y0 = static_cast<std::uint64_t>(y) * source_height / height;
    std::uint64_t y1 = (static_cast<std::uint64_t>(y) + 1) * source_height / height;
    if (y1 <= y0) {
      y1 = y0 + 1;
    }
    for (std::uint32_t x = 0; x < width; ++x) {
      const std::uint64_t x0 = static_cast<std::uint64_t>(x) * source_width / width;
      std::uint64_t x1 = (static_cast<std::uint64_t>(x) + 1) * source_width / width;
      if (x1 <= x0) {
        x1 = x0 + 1;
      }
      std::uint64_t sum[4] = {0, 0, 0, 0};
      for (std::uint64_t sy = y0; sy < y1; ++sy) {
        for (std::uint64_t sx = x0; sx < x1; ++sx) {
          const std::uint8_t* pixel = source + (sy * source_width + sx) * 4;
          for (int channel = 0; channel < 4; ++channel) {
            sum[channel] += pixel[channel];
          }
        }
      }
      const std::uint64_t count = (y1 - y0) * (x1 - x0);
      std::uint8_t* out = destination + (static_cast<std::size_t>(y) * width + x) * 4;
      for (int channel = 0; channel < 4; ++channel) {
        out[channel] = static_cast<std::uint8_t>((sum[channel] + count / 2) / count);
      }
    }
  }
}

namespace {

std::string EnvironmentString(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr ? value : "";
}

bool HasPng(const std::string& dir) {
  std::error_code error;
  for (std::filesystem::directory_iterator it(std::filesystem::u8path(dir), error), end; !error && it != end;
       it.increment(error)) {
    if (it->path().extension() == ".png") {
      return true;
    }
  }
  return false;
}

}

TextureOverrides::TextureOverrides(std::string override_dir, std::string dump_dir)
    : override_dir_(std::move(override_dir)), dump_dir_(std::move(dump_dir)) {
  if (!override_dir_.empty() && !HasPng(override_dir_)) {
    override_dir_.clear();
  }
}

TextureOverrides TextureOverrides::FromEnvironment() {
  return TextureOverrides(EnvironmentString("ELIXIR_TEXTURE_OVERRIDE_DIR"),
                          EnvironmentString("ELIXIR_TEXTURE_DUMP_DIR"));
}

std::shared_ptr<const RgbaImage> TextureOverrides::Lookup(std::uint64_t hash) {
  if (override_dir_.empty()) {
    return nullptr;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  const auto cached = cache_.find(hash);
  if (cached != cache_.end()) {
    return cached->second;
  }
  std::shared_ptr<const RgbaImage> loaded;
  RgbaImage image;
  const std::string path = override_dir_ + "/" + HashName(hash) + ".png";
  if (ReadPngRgba(path, &image) && image.width > 0 && image.height > 0) {
    loaded = std::make_shared<const RgbaImage>(std::move(image));
    std::fprintf(stderr, "[vulkan] texture override loaded: %s (%ux%u)\n", path.c_str(), loaded->width,
                 loaded->height);
  }
  cache_[hash] = loaded;
  return loaded;
}

void TextureOverrides::Dump(std::uint64_t hash, std::uint32_t width, std::uint32_t height,
                            const std::uint8_t* rgba) {
  if (dump_dir_.empty() || rgba == nullptr || width == 0 || height == 0) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!dumped_.insert(hash).second) {
      return;
    }
  }
  std::error_code error;
  std::filesystem::create_directories(std::filesystem::u8path(dump_dir_), error);
  RgbaImage image;
  image.width = width;
  image.height = height;
  image.pixels.assign(rgba, rgba + static_cast<std::size_t>(width) * height * 4);
  const std::string path =
      dump_dir_ + "/" + HashName(hash) + "_" + std::to_string(width) + "x" + std::to_string(height) + ".png";
  if (!WritePngRgba(path, image)) {
    std::fprintf(stderr, "[vulkan] texture dump failed: %s\n", path.c_str());
  }
}

}
