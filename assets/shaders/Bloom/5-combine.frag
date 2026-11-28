#version 450
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
