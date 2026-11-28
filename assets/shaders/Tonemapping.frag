#version 450
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
