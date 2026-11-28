#version 450
#pragma parameter amount "Amount" 0.30 0.00 2.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  float amount;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec2 from_centre = uv - 0.5;
  vec2 shift = from_centre * dot(from_centre, from_centre) * amount * 0.04;
  float red = texture(source, uv + shift).r;
  float green = texture(source, uv).g;
  float blue = texture(source, uv - shift).b;
  color = vec4(red, green, blue, 1.0);
}
