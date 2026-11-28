#version 450
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
