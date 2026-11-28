// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// kUpscaleFragmentShader is adapted from Snapdragon Game Super Resolution 1
// (sgsr1_shader_mobile.frag): Copyright (c) 2025, Qualcomm Innovation Center, Inc.
// All rights reserved. SPDX-License-Identifier: BSD-3-Clause. Modified by Elixir.
#include "shaders/builtin_shaders.h"

namespace elixir::shaders {

const char kTriangleVertexShader[] = R"glsl(#version 450
layout(location = 0) out vec2 uv;
void main() {
  uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

const char kUpscaleFragmentShader[] = R"glsl(#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
  float sharpness;
};
layout(set = 0, binding = 1) uniform sampler2D source;

float fastLanczos2(float x) {
  float wA = x - 4.0;
  float wB = x * wA - wA;
  wA *= wA;
  return wB * wA;
}

vec2 weightY(float dx, float dy, float c, float std) {
  float x = ((dx * dx) + (dy * dy)) * 0.55 + clamp(abs(c) * std, 0.0, 1.0);
  float w = fastLanczos2(x);
  return vec2(w, w * c);
}

void main() {
  const float edgeThreshold = 8.0 / 255.0;
  float edgeSharpness = 1.0 + clamp(sharpness, 0.0, 1.0);
  vec4 pixel = vec4(textureLod(source, uv, 0.0).rgb, 1.0);
  vec2 imgCoord = uv * source_size.xy + vec2(-0.5, 0.5);
  vec2 imgCoordPixel = floor(imgCoord);
  vec2 coord = imgCoordPixel * source_size.zw;
  vec2 pl = imgCoord - imgCoordPixel;
  vec4 left = textureGather(source, coord, 1);
  float edgeVote = abs(left.z - left.y) + abs(pixel.g - left.y) + abs(pixel.g - left.z);
  if (edgeVote > edgeThreshold) {
    coord.x += source_size.z;
    vec4 right = textureGather(source, coord + vec2(source_size.z, 0.0), 1);
    vec4 upDown;
    upDown.xy = textureGather(source, coord + vec2(0.0, -source_size.w), 1).wz;
    upDown.zw = textureGather(source, coord + vec2(0.0, source_size.w), 1).yx;
    float mean = (left.y + left.z + right.x + right.w) * 0.25;
    left -= vec4(mean);
    right -= vec4(mean);
    upDown -= vec4(mean);
    float centre = pixel.g - mean;
    float sum = abs(left.x) + abs(left.y) + abs(left.z) + abs(left.w) + abs(right.x) + abs(right.y) +
                abs(right.z) + abs(right.w) + abs(upDown.x) + abs(upDown.y) + abs(upDown.z) + abs(upDown.w);
    float std = 2.181818 / max(sum, 1.0e-5);
    vec2 aWY = weightY(pl.x, pl.y + 1.0, upDown.x, std);
    aWY += weightY(pl.x - 1.0, pl.y + 1.0, upDown.y, std);
    aWY += weightY(pl.x - 1.0, pl.y - 2.0, upDown.z, std);
    aWY += weightY(pl.x, pl.y - 2.0, upDown.w, std);
    aWY += weightY(pl.x + 1.0, pl.y - 1.0, left.x, std);
    aWY += weightY(pl.x, pl.y - 1.0, left.y, std);
    aWY += weightY(pl.x, pl.y, left.z, std);
    aWY += weightY(pl.x + 1.0, pl.y, left.w, std);
    aWY += weightY(pl.x - 1.0, pl.y - 1.0, right.x, std);
    aWY += weightY(pl.x - 2.0, pl.y - 1.0, right.y, std);
    aWY += weightY(pl.x - 2.0, pl.y, right.z, std);
    aWY += weightY(pl.x - 1.0, pl.y, right.w, std);
    float finalY = aWY.y / aWY.x;
    float maxY = max(max(left.y, left.z), max(right.x, right.w));
    float minY = min(min(left.y, left.z), min(right.x, right.w));
    finalY = clamp(edgeSharpness * finalY, minY, maxY);
    float deltaY = clamp(finalY - centre, -23.0 / 255.0, 23.0 / 255.0);
    pixel.rgb = clamp(pixel.rgb + vec3(deltaY), 0.0, 1.0);
  }
  color = pixel;
}
)glsl";

}
