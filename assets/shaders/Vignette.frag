#version 450
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
