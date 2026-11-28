// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_FRAME_TIMES_H_
#define ELIXIR_RUNTIME_FRAME_TIMES_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace elixir::runtime {

struct FrameTimeSummary {
  std::uint64_t frames = 0;
  double median_ms = 0;
  double slowest_percent_ms = 0;
  double slowest_permille_ms = 0;
};

class FrameTimes {
 public:
  void Add(std::int64_t microseconds) noexcept;
  FrameTimeSummary Take() noexcept;

 private:
  static constexpr std::int64_t kFineStepUs = 50;
  static constexpr std::int64_t kFineLimitUs = 20000;
  static constexpr std::int64_t kCoarseStepUs = 1000;
  static constexpr std::int64_t kCoarseLimitUs = 200000;
  static constexpr std::size_t kFineBuckets = kFineLimitUs / kFineStepUs;
  static constexpr std::size_t kCoarseBuckets = (kCoarseLimitUs - kFineLimitUs) / kCoarseStepUs;
  static constexpr std::size_t kBuckets = kFineBuckets + kCoarseBuckets + 1;

  static double LowerEdgeMs(std::size_t bucket) noexcept;

  std::array<std::atomic<std::uint32_t>, kBuckets> counts_{};
};

std::string DescribeFrameTimes(const FrameTimeSummary& summary);

}

#endif
