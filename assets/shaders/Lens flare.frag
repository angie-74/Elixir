#version 450
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
