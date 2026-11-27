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

struct RenderScale {
  float size = 1.0f;
  float width = 1.0f;

  bool smaller() const { return RendersSmaller(size) || RendersSmaller(width); }
  float y() const { return RendersSmaller(size) ? size : 1.0f; }
  float stretch() const { return RendersSmaller(width) ? width : 1.0f; }
  float x() const { return y() * stretch(); }
  bool operator==(const RenderScale& other) const { return size == other.size && width == other.width; }
  bool operator!=(const RenderScale& other) const { return !(*this == other); }
};

inline RenderScale Normalized(const RenderScale& scale) { return RenderScale{scale.y(), scale.stretch()}; }

inline std::int64_t ScaledWidth(std::int64_t extent, const RenderScale& scale) {
  return ScaledExtent(extent, scale.x());
}

inline std::int64_t ScaledHeight(std::int64_t extent, const RenderScale& scale) {
  return ScaledExtent(extent, scale.y());
}

}

#endif
