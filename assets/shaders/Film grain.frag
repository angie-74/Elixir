#version 450
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
