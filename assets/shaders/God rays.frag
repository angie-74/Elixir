#version 450
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
