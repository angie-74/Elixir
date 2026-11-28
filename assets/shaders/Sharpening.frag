#version 450
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
