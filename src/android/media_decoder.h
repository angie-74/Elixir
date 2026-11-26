// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace elixir::android::media {

inline constexpr char kAvcDecoderName[] = "c2.elixir.avc.decoder";

struct VideoFormat {
  int width = 0;
  int height = 0;
  int stride = 0;
  int slice_height = 0;
};

struct VideoFrame {
  VideoFormat format;
  std::vector<std::uint8_t> bytes;
  std::int64_t time_us = 0;
};

enum class DecodeResult { kOk, kAgain, kError };

class AvcDecoder {
 public:
  static std::unique_ptr<AvcDecoder> Create();
  ~AvcDecoder();

  bool Configure(int width, int height);
  bool Start();
  DecodeResult Submit(const std::uint8_t* data, std::size_t size, std::int64_t time_us);
  bool Drain();
  DecodeResult Receive(VideoFrame* frame);
  bool Flush();
  bool Stop();

 private:
  class Impl;
  explicit AvcDecoder(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}
