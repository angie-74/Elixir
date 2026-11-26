// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_AUDIO_PCM_CONVERTER_H_
#define ELIXIR_AUDIO_PCM_CONVERTER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "mocktail/audio/audio_sink.h"
#include "mocktail/status.h"

namespace mocktail::audio {

enum class PcmSampleType : std::uint8_t {
  kU8,
  kS16,
  kS24Packed,
  kS32,
  kF32,
};

std::size_t PcmSampleTypeBytes(PcmSampleType type);
const char* PcmSampleTypeName(PcmSampleType type);
PcmSampleType PcmSampleTypeFromFormat(PcmSampleFormat format);

inline constexpr std::uint32_t kSpeakerFrontLeft = 0x1;
inline constexpr std::uint32_t kSpeakerFrontRight = 0x2;
inline constexpr std::uint32_t kSpeakerFrontCenter = 0x4;
inline constexpr std::uint32_t kSpeakerLowFrequency = 0x8;
inline constexpr std::uint32_t kSpeakerBackLeft = 0x10;
inline constexpr std::uint32_t kSpeakerBackRight = 0x20;
inline constexpr std::uint32_t kSpeakerBackCenter = 0x100;
inline constexpr std::uint32_t kSpeakerSideLeft = 0x200;
inline constexpr std::uint32_t kSpeakerSideRight = 0x400;

std::uint32_t DefaultChannelMask(int channels);

struct PcmLayout {
  int sample_rate_hz = 48000;
  int channels = 2;
  std::uint32_t channel_mask = 0;
  PcmSampleType type = PcmSampleType::kF32;
};

PcmLayout LayoutFromPcmSpec(const PcmSpec& spec);
std::size_t BytesPerFrame(const PcmLayout& layout);

class ChannelMatrix final {
 public:
  void Build(int input_channels, std::uint32_t input_mask, int output_channels,
             std::uint32_t output_mask);
  bool identity() const { return identity_; }
  int input_channels() const { return input_channels_; }
  int output_channels() const { return output_channels_; }
  float coefficient(int output, int input) const {
    return coefficients_[static_cast<std::size_t>(output) * input_channels_ +
                         input];
  }
  void Apply(const float* input, float* output, std::size_t frames) const;

 private:
  int input_channels_ = 0;
  int output_channels_ = 0;
  bool identity_ = true;
  std::vector<float> coefficients_;
};

class PcmStreamConverter final {
 public:
  PcmStreamConverter();

  Status Configure(const PcmLayout& input, const PcmLayout& output,
                   bool preserve_input = false);
  bool configured() const { return configured_; }
  const PcmLayout& input() const { return input_; }
  const PcmLayout& output() const { return output_; }
  bool resampling() const { return input_.sample_rate_hz != output_.sample_rate_hz; }

  void Push(const void* data, std::size_t frames);
  std::size_t InputFramesWanted(std::size_t output_frames) const;
  std::size_t Pull(void* output, std::size_t max_frames, float gain, bool drain);
  std::size_t buffered_input_frames() const;
  std::size_t lookahead_frames() const;
  void Reset();

 private:
  void Compact();
  float FilterTap(double distance) const;

  PcmLayout input_;
  PcmLayout output_;
  bool configured_ = false;
  ChannelMatrix matrix_;
  std::vector<float> buffer_;
  std::size_t buffered_frames_ = 0;
  std::size_t read_frame_ = 0;
  std::uint64_t position_ = 0;
  std::uint64_t step_ = 0;
  int half_taps_ = 0;
  double cutoff_ = 1.0;
  std::vector<float> scratch_in_;
  std::vector<float> scratch_out_;
  std::vector<float> weights_;
};

void ConvertSamplesToFloat(PcmSampleType type, const void* input, float* output,
                           std::size_t samples);
void ConvertFloatToSamples(PcmSampleType type, const float* input, void* output,
                           std::size_t samples, float gain);

}

#endif
