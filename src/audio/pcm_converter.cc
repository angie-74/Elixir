// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "audio/pcm_converter.h"

#include <emmintrin.h>
#include <xmmintrin.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace mocktail::audio {
namespace {

constexpr int kZeroCrossings = 8;
constexpr int kTableStepsPerCrossing = 512;
constexpr double kKaiserBeta = 8.0;
constexpr double kPi = 3.14159265358979323846;
constexpr std::size_t kBlockFrames = 256;

int PopCount(std::uint32_t value) {
  int count = 0;
  while (value != 0) {
    value &= value - 1;
    ++count;
  }
  return count;
}

double BesselI0(double x) {
  double sum = 1.0;
  double term = 1.0;
  const double half = x / 2.0;
  for (int k = 1; k < 64; ++k) {
    term *= (half / k) * (half / k);
    sum += term;
    if (term < sum * 1e-17) break;
  }
  return sum;
}

const std::vector<float>& FilterTable() {
  static const std::vector<float> table = [] {
    const int size = kZeroCrossings * kTableStepsPerCrossing + 2;
    std::vector<float> values(static_cast<std::size_t>(size), 0.0F);
    const double i0_beta = BesselI0(kKaiserBeta);
    for (int index = 0; index < size; ++index) {
      const double u = static_cast<double>(index) / kTableStepsPerCrossing;
      if (u >= kZeroCrossings) {
        values[static_cast<std::size_t>(index)] = 0.0F;
        continue;
      }
      const double sinc = u == 0.0 ? 1.0 : std::sin(kPi * u) / (kPi * u);
      const double ratio = u / kZeroCrossings;
      const double window =
          BesselI0(kKaiserBeta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / i0_beta;
      values[static_cast<std::size_t>(index)] = static_cast<float>(sinc * window);
    }
    return values;
  }();
  return table;
}

inline std::int32_t RoundToInt(float value) {
  return _mm_cvt_ss2si(_mm_set_ss(value));
}

std::uint32_t EffectiveMask(int channels, std::uint32_t mask) {
  if (mask != 0 && PopCount(mask) == channels) return mask;
  return DefaultChannelMask(channels);
}

std::vector<std::uint32_t> Speakers(int channels, std::uint32_t mask) {
  std::vector<std::uint32_t> speakers;
  const std::uint32_t effective = EffectiveMask(channels, mask);
  for (std::uint32_t bit = 1; bit != 0 && speakers.size() < static_cast<std::size_t>(channels);
       bit <<= 1) {
    if ((effective & bit) != 0) speakers.push_back(bit);
  }
  while (speakers.size() < static_cast<std::size_t>(channels)) speakers.push_back(0);
  return speakers;
}

}

std::size_t PcmSampleTypeBytes(PcmSampleType type) {
  switch (type) {
    case PcmSampleType::kU8: return 1;
    case PcmSampleType::kS16: return 2;
    case PcmSampleType::kS24Packed: return 3;
    case PcmSampleType::kS32:
    case PcmSampleType::kF32: return 4;
  }
  return 0;
}

const char* PcmSampleTypeName(PcmSampleType type) {
  switch (type) {
    case PcmSampleType::kU8: return "u8";
    case PcmSampleType::kS16: return "s16le";
    case PcmSampleType::kS24Packed: return "s24le";
    case PcmSampleType::kS32: return "s32le";
    case PcmSampleType::kF32: return "f32le";
  }
  return "unknown";
}

PcmSampleType PcmSampleTypeFromFormat(PcmSampleFormat format) {
  switch (format) {
    case PcmSampleFormat::kUnsigned8: return PcmSampleType::kU8;
    case PcmSampleFormat::kSigned16LittleEndian: return PcmSampleType::kS16;
    case PcmSampleFormat::kSigned32LittleEndian: return PcmSampleType::kS32;
    case PcmSampleFormat::kFloat32LittleEndian: return PcmSampleType::kF32;
  }
  return PcmSampleType::kS16;
}

std::uint32_t DefaultChannelMask(int channels) {
  switch (channels) {
    case 1: return kSpeakerFrontCenter;
    case 2: return kSpeakerFrontLeft | kSpeakerFrontRight;
    case 3: return kSpeakerFrontLeft | kSpeakerFrontRight | kSpeakerFrontCenter;
    case 4: return kSpeakerFrontLeft | kSpeakerFrontRight | kSpeakerBackLeft | kSpeakerBackRight;
    case 5:
      return kSpeakerFrontLeft | kSpeakerFrontRight | kSpeakerFrontCenter | kSpeakerBackLeft |
             kSpeakerBackRight;
    case 6:
      return kSpeakerFrontLeft | kSpeakerFrontRight | kSpeakerFrontCenter | kSpeakerLowFrequency |
             kSpeakerBackLeft | kSpeakerBackRight;
    case 7:
      return kSpeakerFrontLeft | kSpeakerFrontRight | kSpeakerFrontCenter | kSpeakerLowFrequency |
             kSpeakerBackCenter | kSpeakerSideLeft | kSpeakerSideRight;
    case 8:
      return kSpeakerFrontLeft | kSpeakerFrontRight | kSpeakerFrontCenter | kSpeakerLowFrequency |
             kSpeakerBackLeft | kSpeakerBackRight | kSpeakerSideLeft | kSpeakerSideRight;
    default: return 0;
  }
}

PcmLayout LayoutFromPcmSpec(const PcmSpec& spec) {
  PcmLayout layout;
  layout.sample_rate_hz = spec.sample_rate_hz;
  layout.channels = spec.channels;
  layout.channel_mask = 0;
  layout.type = PcmSampleTypeFromFormat(spec.format);
  return layout;
}

std::size_t BytesPerFrame(const PcmLayout& layout) {
  return layout.channels > 0 ? PcmSampleTypeBytes(layout.type) * static_cast<std::size_t>(layout.channels)
                             : 0;
}

void ConvertSamplesToFloat(PcmSampleType type, const void* input, float* output,
                           std::size_t samples) {
  const auto* bytes = static_cast<const std::uint8_t*>(input);
  switch (type) {
    case PcmSampleType::kU8:
      for (std::size_t i = 0; i < samples; ++i) {
        output[i] = (static_cast<float>(bytes[i]) - 128.0F) * (1.0F / 128.0F);
      }
      break;
    case PcmSampleType::kS16:
      for (std::size_t i = 0; i < samples; ++i) {
        std::int16_t value;
        std::memcpy(&value, bytes + i * 2, 2);
        output[i] = static_cast<float>(value) * (1.0F / 32768.0F);
      }
      break;
    case PcmSampleType::kS24Packed:
      for (std::size_t i = 0; i < samples; ++i) {
        const std::uint8_t* s = bytes + i * 3;
        std::int32_t value = static_cast<std::int32_t>(static_cast<std::uint32_t>(s[0]) |
                                                       (static_cast<std::uint32_t>(s[1]) << 8) |
                                                       (static_cast<std::uint32_t>(s[2]) << 16));
        if ((value & 0x800000) != 0) value -= 0x1000000;
        output[i] = static_cast<float>(value) * (1.0F / 8388608.0F);
      }
      break;
    case PcmSampleType::kS32:
      for (std::size_t i = 0; i < samples; ++i) {
        std::int32_t value;
        std::memcpy(&value, bytes + i * 4, 4);
        output[i] = static_cast<float>(static_cast<double>(value) * (1.0 / 2147483648.0));
      }
      break;
    case PcmSampleType::kF32:
      std::memcpy(output, input, samples * sizeof(float));
      break;
  }
}

void ConvertFloatToSamples(PcmSampleType type, const float* input, void* output,
                           std::size_t samples, float gain) {
  auto* bytes = static_cast<std::uint8_t*>(output);
  switch (type) {
    case PcmSampleType::kU8:
      for (std::size_t i = 0; i < samples; ++i) {
        const float scaled = std::clamp(input[i] * gain * 128.0F, -128.0F, 127.0F);
        bytes[i] = static_cast<std::uint8_t>(RoundToInt(scaled) + 128);
      }
      break;
    case PcmSampleType::kS16:
      for (std::size_t i = 0; i < samples; ++i) {
        const float scaled = std::clamp(input[i] * gain * 32768.0F, -32768.0F, 32767.0F);
        const auto value = static_cast<std::int16_t>(RoundToInt(scaled));
        std::memcpy(bytes + i * 2, &value, 2);
      }
      break;
    case PcmSampleType::kS24Packed:
      for (std::size_t i = 0; i < samples; ++i) {
        const float scaled = std::clamp(input[i] * gain * 8388608.0F, -8388608.0F, 8388607.0F);
        const auto value = static_cast<std::uint32_t>(RoundToInt(scaled));
        bytes[i * 3] = static_cast<std::uint8_t>(value);
        bytes[i * 3 + 1] = static_cast<std::uint8_t>(value >> 8);
        bytes[i * 3 + 2] = static_cast<std::uint8_t>(value >> 16);
      }
      break;
    case PcmSampleType::kS32:
      for (std::size_t i = 0; i < samples; ++i) {
        const double scaled = std::clamp(static_cast<double>(input[i]) * gain * 2147483648.0,
                                         -2147483648.0, 2147483647.0);
        const auto value = static_cast<std::int32_t>(_mm_cvtsd_si32(_mm_set_sd(scaled)));
        std::memcpy(bytes + i * 4, &value, 4);
      }
      break;
    case PcmSampleType::kF32:
      if (gain == 1.0F) {
        std::memcpy(output, input, samples * sizeof(float));
      } else {
        auto* floats = static_cast<float*>(output);
        for (std::size_t i = 0; i < samples; ++i) floats[i] = input[i] * gain;
      }
      break;
  }
}

void ChannelMatrix::Build(int input_channels, std::uint32_t input_mask, int output_channels,
                          std::uint32_t output_mask) {
  input_channels_ = input_channels;
  output_channels_ = output_channels;
  coefficients_.assign(static_cast<std::size_t>(input_channels) * output_channels, 0.0F);
  const std::vector<std::uint32_t> in = Speakers(input_channels, input_mask);
  const std::vector<std::uint32_t> out = Speakers(output_channels, output_mask);
  identity_ = input_channels == output_channels && in == out;
  if (identity_) {
    for (int c = 0; c < input_channels; ++c) {
      coefficients_[static_cast<std::size_t>(c) * input_channels + c] = 1.0F;
    }
    return;
  }
  const auto find = [&out](std::uint32_t speaker) -> int {
    if (speaker == 0) return -1;
    for (std::size_t i = 0; i < out.size(); ++i) {
      if (out[i] == speaker) return static_cast<int>(i);
    }
    return -1;
  };
  const auto add = [this](int output, int input, float value) {
    if (output >= 0) coefficients_[static_cast<std::size_t>(output) * input_channels_ + input] += value;
  };
  constexpr float kMinus3dB = 0.70710678F;

  if (output_channels == 1) {
    int counted = 0;
    for (int i = 0; i < input_channels; ++i) counted += in[static_cast<std::size_t>(i)] != kSpeakerLowFrequency;
    if (counted == 0) counted = input_channels;
    for (int i = 0; i < input_channels; ++i) {
      if (in[static_cast<std::size_t>(i)] != kSpeakerLowFrequency || counted == input_channels) {
        add(0, i, 1.0F / static_cast<float>(counted));
      }
    }
    return;
  }

  const int fl = find(kSpeakerFrontLeft);
  const int fr = find(kSpeakerFrontRight);
  const int fc = find(kSpeakerFrontCenter);
  if (input_channels == 1) {
    if (fl >= 0 && fr >= 0) {
      add(fl, 0, 1.0F);
      add(fr, 0, 1.0F);
    } else if (fc >= 0) {
      add(fc, 0, 1.0F);
    } else {
      add(0, 0, 1.0F);
    }
    return;
  }

  bool folded = false;
  const auto pair = [&](int input, int left, int right, float value) {
    if (left >= 0 && right >= 0) {
      add(left, input, value);
      add(right, input, value);
      return true;
    }
    return false;
  };
  for (int i = 0; i < input_channels; ++i) {
    const std::uint32_t speaker = in[static_cast<std::size_t>(i)];
    const int direct = find(speaker);
    if (direct >= 0) {
      add(direct, i, 1.0F);
      continue;
    }
    folded = true;
    switch (speaker) {
      case kSpeakerFrontLeft:
      case kSpeakerFrontRight:
        add(fc, i, kMinus3dB);
        break;
      case kSpeakerFrontCenter:
        if (!pair(i, fl, fr, kMinus3dB)) add(fl >= 0 ? fl : fr, i, kMinus3dB);
        break;
      case kSpeakerLowFrequency:
        break;
      case kSpeakerBackLeft:
      case kSpeakerBackRight: {
        const bool left = speaker == kSpeakerBackLeft;
        const int side = find(left ? kSpeakerSideLeft : kSpeakerSideRight);
        if (side >= 0) {
          add(side, i, 1.0F);
        } else {
          add(left ? fl : fr, i, kMinus3dB);
        }
        break;
      }
      case kSpeakerSideLeft:
      case kSpeakerSideRight: {
        const bool left = speaker == kSpeakerSideLeft;
        const int back = find(left ? kSpeakerBackLeft : kSpeakerBackRight);
        if (back >= 0) {
          add(back, i, 1.0F);
        } else {
          add(left ? fl : fr, i, kMinus3dB);
        }
        break;
      }
      case kSpeakerBackCenter:
        if (!pair(i, find(kSpeakerBackLeft), find(kSpeakerBackRight), kMinus3dB) &&
            !pair(i, find(kSpeakerSideLeft), find(kSpeakerSideRight), kMinus3dB)) {
          pair(i, fl, fr, kMinus3dB);
        }
        break;
      case 0x40:
        add(fl, i, 1.0F);
        break;
      case 0x80:
        add(fr, i, 1.0F);
        break;
      default:
        break;
    }
  }
  if (folded) {
    for (int o = 0; o < output_channels; ++o) {
      float sum = 0.0F;
      for (int i = 0; i < input_channels; ++i) sum += coefficient(o, i);
      if (sum > 1.0F) {
        for (int i = 0; i < input_channels; ++i) {
          coefficients_[static_cast<std::size_t>(o) * input_channels + i] /= sum;
        }
      }
    }
  }
}

void ChannelMatrix::Apply(const float* input, float* output, std::size_t frames) const {
  if (identity_) {
    std::memcpy(output, input, frames * static_cast<std::size_t>(input_channels_) * sizeof(float));
    return;
  }
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const float* in = input + frame * static_cast<std::size_t>(input_channels_);
    float* out = output + frame * static_cast<std::size_t>(output_channels_);
    for (int o = 0; o < output_channels_; ++o) {
      const float* row = coefficients_.data() + static_cast<std::size_t>(o) * input_channels_;
      float sum = 0.0F;
      for (int i = 0; i < input_channels_; ++i) sum += row[i] * in[i];
      out[o] = sum;
    }
  }
}

PcmStreamConverter::PcmStreamConverter() { (void)FilterTable(); }

Status PcmStreamConverter::Configure(const PcmLayout& input, const PcmLayout& output,
                                     bool preserve_input) {
  for (const PcmLayout* layout : {&input, &output}) {
    if (layout->sample_rate_hz < 8000 || layout->sample_rate_hz > 384000) {
      return Status::Error(StatusCode::kInvalidArgument,
                           "PCM sample rate must be between 8000 and 384000 Hz");
    }
    if (layout->channels < 1 || layout->channels > 8) {
      return Status::Error(StatusCode::kInvalidArgument, "PCM channel count must be between 1 and 8");
    }
    if (PcmSampleTypeBytes(layout->type) == 0) {
      return Status::Error(StatusCode::kInvalidArgument, "PCM sample type is unsupported");
    }
  }
  const bool same_input = configured_ && input_.channels == input.channels &&
                          input_.sample_rate_hz == input.sample_rate_hz &&
                          input_.type == input.type &&
                          EffectiveMask(input_.channels, input_.channel_mask) ==
                              EffectiveMask(input.channels, input.channel_mask);
  std::vector<float> pending;
  if (preserve_input && same_input) {
    const std::size_t channels = static_cast<std::size_t>(input_.channels);
    std::size_t first = resampling() ? static_cast<std::size_t>(position_ >> 32) : read_frame_;
    first = std::min(first, buffered_frames_);
    pending.assign(buffer_.begin() + static_cast<std::ptrdiff_t>(first * channels),
                   buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_frames_ * channels));
  }
  input_ = input;
  output_ = output;
  matrix_.Build(input.channels, input.channel_mask, output.channels, output.channel_mask);
  configured_ = true;
  if (resampling()) {
    cutoff_ = std::min(1.0, static_cast<double>(output.sample_rate_hz) / input.sample_rate_hz);
    half_taps_ = static_cast<int>(std::ceil(kZeroCrossings / cutoff_));
    step_ = (static_cast<std::uint64_t>(input.sample_rate_hz) << 32) /
            static_cast<std::uint64_t>(output.sample_rate_hz);
  } else {
    cutoff_ = 1.0;
    half_taps_ = 0;
    step_ = 1ULL << 32;
  }
  Reset();
  if (!pending.empty()) {
    buffer_.insert(buffer_.end(), pending.begin(), pending.end());
    buffered_frames_ += pending.size() / static_cast<std::size_t>(input_.channels);
  }
  return Status::Ok();
}

void PcmStreamConverter::Reset() {
  buffer_.clear();
  read_frame_ = 0;
  buffered_frames_ = 0;
  if (configured_ && resampling()) {
    const std::size_t history = static_cast<std::size_t>(half_taps_ - 1);
    buffer_.assign(history * static_cast<std::size_t>(input_.channels), 0.0F);
    buffered_frames_ = history;
    position_ = static_cast<std::uint64_t>(history) << 32;
  } else {
    position_ = 0;
  }
}

std::size_t PcmStreamConverter::lookahead_frames() const {
  return resampling() ? static_cast<std::size_t>(half_taps_) : 0;
}

void PcmStreamConverter::Push(const void* data, std::size_t frames) {
  if (!configured_ || data == nullptr || frames == 0) return;
  const std::size_t channels = static_cast<std::size_t>(input_.channels);
  const std::size_t old_size = buffer_.size();
  buffer_.resize(old_size + frames * channels);
  ConvertSamplesToFloat(input_.type, data, buffer_.data() + old_size, frames * channels);
  buffered_frames_ += frames;
}

std::size_t PcmStreamConverter::buffered_input_frames() const {
  if (!configured_) return 0;
  if (!resampling()) return buffered_frames_ - read_frame_;
  const std::size_t center = static_cast<std::size_t>(position_ >> 32);
  return buffered_frames_ > center ? buffered_frames_ - center : 0;
}

std::size_t PcmStreamConverter::InputFramesWanted(std::size_t output_frames) const {
  if (!configured_ || output_frames == 0) return 0;
  if (!resampling()) {
    const std::size_t available = buffered_frames_ - read_frame_;
    return output_frames > available ? output_frames - available : 0;
  }
  const std::uint64_t last_position = position_ + step_ * (output_frames - 1);
  const std::size_t needed_index = static_cast<std::size_t>(last_position >> 32) +
                                   static_cast<std::size_t>(half_taps_);
  return needed_index + 1 > buffered_frames_ ? needed_index + 1 - buffered_frames_ : 0;
}

float PcmStreamConverter::FilterTap(double distance) const {
  const double u = std::fabs(distance) * cutoff_;
  if (u >= kZeroCrossings) return 0.0F;
  const std::vector<float>& table = FilterTable();
  const double scaled = u * kTableStepsPerCrossing;
  const std::size_t index = static_cast<std::size_t>(scaled);
  const float frac = static_cast<float>(scaled - static_cast<double>(index));
  return table[index] + (table[index + 1] - table[index]) * frac;
}

std::size_t PcmStreamConverter::Pull(void* output, std::size_t max_frames, float gain, bool drain) {
  if (!configured_ || output == nullptr || max_frames == 0) return 0;
  const std::size_t in_channels = static_cast<std::size_t>(input_.channels);
  const std::size_t out_channels = static_cast<std::size_t>(output_.channels);
  const std::size_t out_frame_bytes = BytesPerFrame(output_);
  auto* out_bytes = static_cast<std::uint8_t*>(output);
  std::size_t produced = 0;

  if (!resampling()) {
    const std::size_t frames = std::min(max_frames, buffered_frames_ - read_frame_);
    scratch_out_.resize(kBlockFrames * out_channels);
    for (std::size_t done = 0; done < frames;) {
      const std::size_t block = std::min(kBlockFrames, frames - done);
      matrix_.Apply(buffer_.data() + (read_frame_ + done) * in_channels, scratch_out_.data(), block);
      ConvertFloatToSamples(output_.type, scratch_out_.data(), out_bytes + (produced + done) * out_frame_bytes,
                            block * out_channels, gain);
      done += block;
    }
    read_frame_ += frames;
    produced = frames;
    Compact();
    return produced;
  }

  const int half = half_taps_;
  const std::size_t taps = static_cast<std::size_t>(2 * half);
  weights_.resize(taps);
  float* weights = weights_.data();
  scratch_in_.resize(kBlockFrames * in_channels);
  scratch_out_.resize(kBlockFrames * out_channels);
  while (produced < max_frames) {
    std::size_t block = 0;
    for (; block < kBlockFrames && produced + block < max_frames; ++block) {
      const std::size_t center = static_cast<std::size_t>(position_ >> 32);
      if (drain) {
        if (center >= buffered_frames_) break;
      } else if (center + static_cast<std::size_t>(half) >= buffered_frames_) {
        break;
      }
      const double frac = static_cast<double>(position_ & 0xffffffffULL) * (1.0 / 4294967296.0);
      float weight_sum = 0.0F;
      for (std::size_t t = 0; t < taps; ++t) {
        const double k = static_cast<double>(static_cast<int>(t) - (half - 1));
        weights[t] = FilterTap(k - frac);
        weight_sum += weights[t];
      }
      const float normalise = weight_sum != 0.0F ? 1.0F / weight_sum : 0.0F;
      float* acc = scratch_in_.data() + block * in_channels;
      for (std::size_t c = 0; c < in_channels; ++c) acc[c] = 0.0F;
      const std::size_t first = center + 1 - static_cast<std::size_t>(half);
      for (std::size_t t = 0; t < taps; ++t) {
        const std::size_t index = first + t;
        if (index >= buffered_frames_) break;
        const float w = weights[t];
        const float* frame = buffer_.data() + index * in_channels;
        for (std::size_t c = 0; c < in_channels; ++c) acc[c] += frame[c] * w;
      }
      for (std::size_t c = 0; c < in_channels; ++c) acc[c] *= normalise;
      position_ += step_;
    }
    if (block == 0) break;
    matrix_.Apply(scratch_in_.data(), scratch_out_.data(), block);
    ConvertFloatToSamples(output_.type, scratch_out_.data(), out_bytes + produced * out_frame_bytes,
                          block * out_channels, gain);
    produced += block;
  }
  Compact();
  return produced;
}

void PcmStreamConverter::Compact() {
  const std::size_t channels = static_cast<std::size_t>(input_.channels);
  std::size_t removable = 0;
  if (!resampling()) {
    removable = read_frame_;
  } else {
    const std::size_t center = static_cast<std::size_t>(position_ >> 32);
    const std::size_t history = static_cast<std::size_t>(half_taps_ - 1);
    removable = center > history ? center - history : 0;
    removable = std::min(removable, buffered_frames_);
  }
  if (removable == 0) return;
  if (removable < 1024 && removable != buffered_frames_ && buffer_.size() < 64 * 1024) return;
  buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(removable * channels));
  buffered_frames_ -= removable;
  if (!resampling()) {
    read_frame_ -= removable;
  } else {
    position_ -= static_cast<std::uint64_t>(removable) << 32;
  }
}

}
