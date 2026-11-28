// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/shaders.h"

namespace elixir::launcher {

const DefaultShaderFile kDefaultShaderFiles[] = {
    {L"Bloom\\1-bright.frag", R"elixir(#version 450
#pragma scale 0.5
#pragma parameter threshold "Threshold" 0.70 0.00 1.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
  float threshold;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec2 o = source_size.zw * 0.5;
  vec3 sum = texture(source, uv + vec2(-o.x, -o.y)).rgb + texture(source, uv + vec2(o.x, -o.y)).rgb +
             texture(source, uv + vec2(-o.x, o.y)).rgb + texture(source, uv + vec2(o.x, o.y)).rgb;
  vec3 pixel = sum * 0.25;
  vec3 light = pixel * pixel;
  float level = max(light.r, max(light.g, light.b));
  float cut = threshold * threshold;
  float keep = clamp((level - cut) / max(1.0 - cut, 0.001), 0.0, 1.0);
  color = vec4(light * keep, 1.0);
}
)elixir"},
    {L"Bloom\\2-down.frag", R"elixir(#version 450
#pragma scale 0.25
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec2 o = source_size.zw * 0.5;
  vec3 sum = texture(source, uv + vec2(-o.x, -o.y)).rgb + texture(source, uv + vec2(o.x, -o.y)).rgb +
             texture(source, uv + vec2(-o.x, o.y)).rgb + texture(source, uv + vec2(o.x, o.y)).rgb;
  color = vec4(sum * 0.25, 1.0);
}
)elixir"},
    {L"Bloom\\3-blur-h.frag", R"elixir(#version 450
#pragma scale 0.25
#pragma parameter radius "Radius" 1.00 0.25 3.00 0.05
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
  float radius;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec2 stride = vec2(source_size.z, 0.0) * radius;
  vec3 sum = texture(source, uv).rgb * 0.2270270270;
  sum += (texture(source, uv + stride * 1.3846153846).rgb + texture(source, uv - stride * 1.3846153846).rgb) * 0.3162162162;
  sum += (texture(source, uv + stride * 3.2307692308).rgb + texture(source, uv - stride * 3.2307692308).rgb) * 0.0702702703;
  color = vec4(sum, 1.0);
}
)elixir"},
    {L"Bloom\\4-blur-v.frag", R"elixir(#version 450
#pragma scale 0.25
#pragma parameter radius "Radius" 1.00 0.25 3.00 0.05
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
  float radius;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec2 stride = vec2(0.0, source_size.w) * radius;
  vec3 sum = texture(source, uv).rgb * 0.2270270270;
  sum += (texture(source, uv + stride * 1.3846153846).rgb + texture(source, uv - stride * 1.3846153846).rgb) * 0.3162162162;
  sum += (texture(source, uv + stride * 3.2307692308).rgb + texture(source, uv - stride * 3.2307692308).rgb) * 0.0702702703;
  color = vec4(sum, 1.0);
}
)elixir"},
    {L"Bloom\\5-combine.frag", R"elixir(#version 450
#pragma parameter intensity "Intensity" 0.50 0.00 2.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  float intensity;
};
layout(set = 0, binding = 1) uniform sampler2D source;
layout(set = 0, binding = 2) uniform sampler2D original;
void main() {
  vec3 pixel = texture(original, uv).rgb;
  vec3 light = pixel * pixel + texture(source, uv).rgb * intensity;
  color = vec4(sqrt(clamp(light, 0.0, 1.0)), 1.0);
}
)elixir"},
    {L"Chromatic aberration.frag", R"elixir(#version 450
#pragma parameter amount "Amount" 0.30 0.00 2.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  float amount;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec2 from_centre = uv - 0.5;
  vec2 shift = from_centre * dot(from_centre, from_centre) * amount * 0.04;
  float red = texture(source, uv + shift).r;
  float green = texture(source, uv).g;
  float blue = texture(source, uv - shift).b;
  color = vec4(red, green, blue, 1.0);
}
)elixir"},
    {L"Color correction.frag", R"elixir(#version 450
#pragma parameter brightness "Brightness" 0.00 -0.50 0.50 0.01
#pragma parameter contrast "Contrast" 1.00 0.50 1.50 0.01
#pragma parameter saturation "Saturation" 1.00 0.00 2.00 0.01
#pragma parameter vibrance "Vibrance" 0.00 -1.00 1.00 0.01
#pragma parameter gamma "Gamma" 1.00 0.50 2.00 0.01
#pragma parameter temperature "Color temperature" 0.00 -1.00 1.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  float brightness;
  float contrast;
  float saturation;
  float vibrance;
  float gamma;
  float temperature;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec3 pixel = texture(source, uv).rgb;
  pixel *= vec3(1.0 + 0.12 * temperature, 1.0, 1.0 - 0.12 * temperature);
  pixel = (pixel - 0.5) * contrast + 0.5 + brightness;
  float luma = dot(pixel, vec3(0.2126, 0.7152, 0.0722));
  float most = max(pixel.r, max(pixel.g, pixel.b));
  float least = min(pixel.r, min(pixel.g, pixel.b));
  float boost = saturation * (1.0 + vibrance * (1.0 - clamp(most - least, 0.0, 1.0)));
  pixel = mix(vec3(luma), pixel, boost);
  pixel = pow(clamp(pixel, 0.0, 1.0), vec3(1.0 / gamma));
  color = vec4(pixel, 1.0);
}
)elixir"},
    {L"Debanding.frag", R"elixir(#version 450
#pragma parameter threshold "Threshold" 0.020 0.000 0.100 0.005
#pragma parameter range "Range" 16.0 4.0 64.0 1.0
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
  float time;
  float threshold;
  float range;
};
layout(set = 0, binding = 1) uniform sampler2D source;
float hash(vec2 p) {
  vec3 q = fract(vec3(p.xyx) * 0.1031);
  q += dot(q, q.yzx + 33.33);
  return fract((q.x + q.y) * q.z);
}
void main() {
  vec3 pixel = texture(source, uv).rgb;
  vec2 at = uv * source_size.xy;
  float angle = hash(at + fract(time) * 311.0) * 6.2831853;
  vec2 offset = vec2(cos(angle), sin(angle)) * hash(at + 17.0) * range * source_size.zw;
  vec2 across = vec2(-offset.y, offset.x);
  vec3 average = (texture(source, uv + offset).rgb + texture(source, uv - offset).rgb +
                  texture(source, uv + across).rgb + texture(source, uv - across).rgb) * 0.25;
  vec3 take = step(abs(average - pixel), vec3(threshold));
  color = vec4(mix(pixel, average, take), 1.0);
}
)elixir"},
    {L"Dithering.frag", R"elixir(#version 450
#pragma parameter amount "Amount" 1.0 0.0 4.0 0.1
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 output_size;
  float amount;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec3 pixel = texture(source, uv).rgb;
  vec2 at = floor(uv * output_size.xy);
  float noise = fract(52.9829189 * fract(dot(at, vec2(0.06711056, 0.00583715))));
  color = vec4(clamp(pixel + (noise - 0.5) * amount / 255.0, 0.0, 1.0), 1.0);
}
)elixir"},
    {L"FXAA.frag", R"elixir(#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
};
layout(set = 0, binding = 1) uniform sampler2D source;
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main() {
  vec2 px = source_size.zw;
  vec3 middle = texture(source, uv).rgb;
  float lm = luma(middle);
  float lnw = luma(texture(source, uv + vec2(-1.0, -1.0) * px).rgb);
  float lne = luma(texture(source, uv + vec2(1.0, -1.0) * px).rgb);
  float lsw = luma(texture(source, uv + vec2(-1.0, 1.0) * px).rgb);
  float lse = luma(texture(source, uv + vec2(1.0, 1.0) * px).rgb);
  float lowest = min(lm, min(min(lnw, lne), min(lsw, lse)));
  float highest = max(lm, max(max(lnw, lne), max(lsw, lse)));
  if (highest - lowest < max(0.0312, highest * 0.125)) {
    color = vec4(middle, 1.0);
    return;
  }
  vec2 along = vec2(-((lnw + lne) - (lsw + lse)), (lnw + lsw) - (lne + lse));
  float reduce = max((lnw + lne + lsw + lse) * 0.03125, 0.0078125);
  along = clamp(along / (min(abs(along.x), abs(along.y)) + reduce), vec2(-8.0), vec2(8.0)) * px;
  vec3 tight = 0.5 * (texture(source, uv + along * (1.0 / 3.0 - 0.5)).rgb +
                     texture(source, uv + along * (2.0 / 3.0 - 0.5)).rgb);
  vec3 wide = tight * 0.5 + 0.25 * (texture(source, uv - along * 0.5).rgb + texture(source, uv + along * 0.5).rgb);
  float lf = luma(wide);
  color = vec4(lf < lowest || lf > highest ? tight : wide, 1.0);
}
)elixir"},
    {L"Film grain.frag", R"elixir(#version 450
#pragma parameter amount "Amount" 0.06 0.00 0.30 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 output_size;
  float time;
  float amount;
};
layout(set = 0, binding = 1) uniform sampler2D source;
float hash(vec2 p) {
  vec3 q = fract(vec3(p.xyx) * 0.1031);
  q += dot(q, q.yzx + 33.33);
  return fract((q.x + q.y) * q.z);
}
void main() {
  vec3 pixel = texture(source, uv).rgb;
  float noise = hash(uv * output_size.xy + fract(time) * 917.0) - 0.5;
  float luma = dot(pixel, vec3(0.2126, 0.7152, 0.0722));
  color = vec4(clamp(pixel + noise * amount * (1.0 - 0.6 * luma), 0.0, 1.0), 1.0);
}
)elixir"},
    {L"God rays.frag", R"elixir(#version 450
#pragma parameter intensity "Intensity" 0.35 0.00 1.00 0.01
#pragma parameter threshold "Threshold" 0.80 0.50 1.00 0.01
#pragma parameter light_x "Light position X" 0.50 0.00 1.00 0.01
#pragma parameter light_y "Light position Y" 0.20 0.00 1.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  float intensity;
  float threshold;
  float light_x;
  float light_y;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec3 pixel = texture(source, uv).rgb;
  vec2 stride = (vec2(light_x, light_y) - uv) * (0.9 / 16.0);
  vec2 at = uv;
  float fade = 1.0;
  vec3 rays = vec3(0.0);
  for (int i = 0; i < 16; ++i) {
    at += stride;
    vec3 c = texture(source, at).rgb;
    rays += c * smoothstep(threshold, 1.0, max(c.r, max(c.g, c.b))) * fade;
    fade *= 0.92;
  }
  color = vec4(clamp(pixel + rays * intensity / 6.0, 0.0, 1.0), 1.0);
}
)elixir"},
    {L"Lens flare.frag", R"elixir(#version 450
#pragma parameter intensity "Intensity" 0.30 0.00 1.00 0.01
#pragma parameter threshold "Threshold" 0.85 0.50 1.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  float intensity;
  float threshold;
};
layout(set = 0, binding = 1) uniform sampler2D source;
vec3 bright(vec2 at) {
  vec3 c = texture(source, clamp(at, 0.0, 1.0)).rgb;
  return c * smoothstep(threshold, 1.0, max(c.r, max(c.g, c.b)));
}
void main() {
  vec3 pixel = texture(source, uv).rgb;
  vec2 mirrored = vec2(1.0) - uv;
  vec2 toward = (vec2(0.5) - mirrored) * 0.35;
  vec3 ghosts = vec3(0.0);
  for (int i = 0; i < 4; ++i) {
    vec2 at = mirrored + toward * float(i);
    ghosts += bright(at) * (1.0 - smoothstep(0.0, 0.75, length(vec2(0.5) - at) * 1.4142));
  }
  color = vec4(clamp(pixel + ghosts * intensity * 0.5, 0.0, 1.0), 1.0);
}
)elixir"},
    {L"Sharpening.frag", R"elixir(#version 450
#pragma parameter strength "Strength" 0.50 0.00 1.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
  float strength;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec3 centre = texture(source, uv).rgb;
  vec3 north = texture(source, uv + vec2(0.0, -source_size.w)).rgb;
  vec3 south = texture(source, uv + vec2(0.0, source_size.w)).rgb;
  vec3 west = texture(source, uv + vec2(-source_size.z, 0.0)).rgb;
  vec3 east = texture(source, uv + vec2(source_size.z, 0.0)).rgb;
  vec3 low = min(centre, min(min(north, south), min(west, east)));
  vec3 high = max(centre, max(max(north, south), max(west, east)));
  vec3 room = clamp(min(low, 1.0 - high) / max(high, vec3(0.0001)), 0.0, 1.0);
  vec3 weight = -sqrt(room) * mix(0.125, 0.2, strength);
  vec3 sharp = (centre + (north + south + west + east) * weight) / (1.0 + 4.0 * weight);
  color = vec4(clamp(sharp, 0.0, 1.0), 1.0);
}
)elixir"},
    {L"Tonemapping.frag", R"elixir(#version 450
#pragma parameter exposure "Exposure" 0.00 -2.00 2.00 0.05
#pragma parameter amount "Tonemapping" 1.00 0.00 1.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  float exposure;
  float amount;
};
layout(set = 0, binding = 1) uniform sampler2D source;
vec3 filmic(vec3 x) {
  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main() {
  vec3 pixel = texture(source, uv).rgb;
  vec3 light = pixel * pixel * exp2(exposure);
  vec3 mapped = mix(clamp(light, 0.0, 1.0), filmic(light * 1.6), amount);
  color = vec4(sqrt(mapped), 1.0);
}
)elixir"},
    {L"Vignette.frag", R"elixir(#version 450
#pragma parameter strength "Strength" 0.35 0.00 1.00 0.01
#pragma parameter radius "Radius" 0.75 0.20 1.50 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 output_size;
  float strength;
  float radius;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec3 pixel = texture(source, uv).rgb;
  vec2 from_centre = (uv - 0.5) * vec2(output_size.x * output_size.w, 1.0);
  float edge = smoothstep(radius * 0.5, radius, length(from_centre));
  color = vec4(pixel * (1.0 - strength * edge), 1.0);
}
)elixir"},
};

const std::size_t kDefaultShaderFileCount = 16;

const wchar_t* const kDefaultShaderOrder[] = {
    L"FXAA.frag",
    L"Sharpening.frag",
    L"Bloom",
    L"Lens flare.frag",
    L"God rays.frag",
    L"Tonemapping.frag",
    L"Color correction.frag",
    L"Chromatic aberration.frag",
    L"Vignette.frag",
    L"Film grain.frag",
    L"Debanding.frag",
    L"Dithering.frag",
};

const std::size_t kDefaultShaderOrderCount = 12;

}
