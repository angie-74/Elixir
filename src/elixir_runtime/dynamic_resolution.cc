// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/dynamic_resolution.h"

#include <algorithm>
#include <cmath>

namespace elixir::runtime {

namespace {

constexpr int kSettleSeconds = 2;
constexpr int kSecondsBelowBeforeLowering = 2;
constexpr int kSecondsAtTargetBeforeRaising = 5;
constexpr int kSecondsToJudge = 3;
constexpr int kLongestWait = 300;
constexpr int kWaitWhenNotThePicture = 60;

double OnGrid(double scale) { return std::round(scale / kDynamicResolutionStep) * kDynamicResolutionStep; }

}

DynamicResolution::DynamicResolution(double target_fps, double highest_scale)
    : target_(std::max(1.0, target_fps)),
      highest_(std::min(1.0, std::max(0.25, highest_scale))),
      lowest_(std::min(highest_, kDynamicResolutionFloor)),
      scale_(highest_) {}

bool DynamicResolution::Update(double frames_per_second) {
  if (!(frames_per_second > 1.0)) return false;
  if (settling_ > 0) {
    --settling_;
    return false;
  }
  const bool below = frames_per_second < target_ * 0.92;
  const bool at_target = frames_per_second >= target_ * 0.97;
  if (raised_) {
    if (below) {
      scale_ = scale_before_raise_;
      raised_ = false;
      wait_before_raising_ = next_wait_;
      next_wait_ = std::min(next_wait_ * 2, kLongestWait);
      settling_ = kSettleSeconds;
      return true;
    }
    if (++raised_for_ >= kSecondsToJudge) {
      raised_ = false;
      next_wait_ = 15;
    }
    return false;
  }
  if (lowered_) {
    if (++lowered_for_ < kSecondsToJudge) return false;
    lowered_ = false;
    if (frames_per_second < rate_before_lowering_ * 1.03) {
      scale_ = scale_before_lowering_;
      not_the_picture_ = kWaitWhenNotThePicture;
      settling_ = kSettleSeconds;
      return true;
    }
    return false;
  }
  if (not_the_picture_ > 0) {
    --not_the_picture_;
    return false;
  }
  if (below) {
    at_target_ = 0;
    if (++below_ < kSecondsBelowBeforeLowering || scale_ <= lowest_ + 1.0e-6) return false;
    below_ = 0;
    const double wanted = OnGrid(scale_ * std::sqrt(frames_per_second / target_) - kDynamicResolutionStep * 0.5);
    const double next =
        std::max(lowest_, std::min(scale_ - kDynamicResolutionStep, std::max(wanted, scale_ - 3 * kDynamicResolutionStep)));
    scale_before_lowering_ = scale_;
    rate_before_lowering_ = frames_per_second;
    scale_ = OnGrid(next);
    lowered_ = true;
    lowered_for_ = 0;
    settling_ = kSettleSeconds;
    return true;
  }
  below_ = 0;
  if (wait_before_raising_ > 0) {
    --wait_before_raising_;
    return false;
  }
  if (!at_target || scale_ >= highest_ - 1.0e-6) {
    at_target_ = 0;
    return false;
  }
  if (++at_target_ < kSecondsAtTargetBeforeRaising) return false;
  at_target_ = 0;
  scale_before_raise_ = scale_;
  scale_ = std::min(highest_, OnGrid(scale_ + kDynamicResolutionStep));
  raised_ = true;
  raised_for_ = 0;
  settling_ = kSettleSeconds;
  return true;
}

}
