#version 450
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
