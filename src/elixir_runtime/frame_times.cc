// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/frame_times.h"

#include <algorithm>
#include <cstdio>

namespace elixir::runtime {

void FrameTimes::Add(std::int64_t microseconds) noexcept {
  if (microseconds < 0) microseconds = 0;
  std::size_t bucket;
  if (microseconds < kFineLimitUs) {
    bucket = static_cast<std::size_t>(microseconds / kFineStepUs);
  } else if (microseconds < kCoarseLimitUs) {
    bucket = kFineBuckets + static_cast<std::size_t>((microseconds - kFineLimitUs) / kCoarseStepUs);
  } else {
    bucket = kBuckets - 1;
  }
  counts_[bucket].fetch_add(1, std::memory_order_relaxed);
}

double FrameTimes::LowerEdgeMs(std::size_t bucket) noexcept {
  if (bucket < kFineBuckets) return static_cast<double>(bucket * kFineStepUs) / 1000.0;
  if (bucket < kBuckets - 1) {
    return static_cast<double>(kFineLimitUs + (bucket - kFineBuckets) * kCoarseStepUs) / 1000.0;
  }
  return static_cast<double>(kCoarseLimitUs) / 1000.0;
}

FrameTimeSummary FrameTimes::Take() noexcept {
  std::array<std::uint32_t, kBuckets> counts;
  FrameTimeSummary summary;
  for (std::size_t bucket = 0; bucket < kBuckets; ++bucket) {
    counts[bucket] = counts_[bucket].exchange(0, std::memory_order_relaxed);
    summary.frames += counts[bucket];
  }
  if (summary.frames == 0) return summary;
  const std::uint64_t middle = (summary.frames + 1) / 2;
  const std::uint64_t first_of_percent = std::min(summary.frames, summary.frames - summary.frames / 100 + 1);
  const std::uint64_t first_of_permille = std::min(summary.frames, summary.frames - summary.frames / 1000 + 1);
  std::uint64_t seen = 0;
  bool median = false;
  bool percent = false;
  bool permille = false;
  for (std::size_t bucket = 0; bucket < kBuckets; ++bucket) {
    if (counts[bucket] == 0) continue;
    seen += counts[bucket];
    const double edge = LowerEdgeMs(bucket);
    if (!median && seen >= middle) {
      summary.median_ms = edge;
      median = true;
    }
    if (!percent && seen >= first_of_percent) {
      summary.slowest_percent_ms = edge;
      percent = true;
    }
    if (!permille && seen >= first_of_permille) {
      summary.slowest_permille_ms = edge;
      permille = true;
    }
  }
  return summary;
}

std::string DescribeFrameTimes(const FrameTimeSummary& summary) {
  if (summary.frames < 100) return {};
  char text[192];
  std::snprintf(text, sizeof(text),
                "frame times: median %.2f ms; 1%% of the frames took %.2f ms or more (%.0f FPS), 0.1%% took %.2f ms "
                "or more (%.0f FPS)",
                summary.median_ms, summary.slowest_percent_ms, 1000.0 / std::max(summary.slowest_percent_ms, 0.05),
                summary.slowest_permille_ms, 1000.0 / std::max(summary.slowest_permille_ms, 0.05));
  return text;
}

}
