// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_GRAPHICS_RENDER_SCALE_H_
#define ELIXIR_GRAPHICS_RENDER_SCALE_H_

#include <cmath>
#include <cstdint>

namespace elixir {

inline bool RendersSmaller(float scale) { return std::isfinite(scale) && scale > 0.0f && scale < 1.0f; }

inline std::int64_t ScaledExtent(std::int64_t extent, float scale) {
  if (extent <= 0 || !RendersSmaller(scale)) return extent;
  const std::int64_t scaled = std::llround(static_cast<double>(extent) * static_cast<double>(scale));
  return scaled < 1 ? 1 : scaled;
}

}

#endif
