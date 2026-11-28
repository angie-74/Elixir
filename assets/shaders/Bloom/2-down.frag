#version 450
#pragma scale 0.25
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec2 o = source_size.zw * 0.5;
  vec3 sum = texture(source, uv + vec2(-o.x, -o.y)).rgb + texture(source, uv + vec2(o.x, -o.y)).rgb +
             texture(source, uv + vec2(-o.x, o.y)).rgb + texture(source, uv + vec2(o.x, o.y)).rgb;
  color = vec4(sum * 0.25, 1.0);
}
