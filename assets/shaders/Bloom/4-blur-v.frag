#version 450
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
