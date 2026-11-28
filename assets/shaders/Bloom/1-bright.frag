#version 450
#pragma scale 0.5
#pragma parameter threshold "Threshold" 0.70 0.00 1.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
  float threshold;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec2 o = source_size.zw * 0.5;
  vec3 sum = texture(source, uv + vec2(-o.x, -o.y)).rgb + texture(source, uv + vec2(o.x, -o.y)).rgb +
             texture(source, uv + vec2(-o.x, o.y)).rgb + texture(source, uv + vec2(o.x, o.y)).rgb;
  vec3 pixel = sum * 0.25;
  vec3 light = pixel * pixel;
  float level = max(light.r, max(light.g, light.b));
  float cut = threshold * threshold;
  float keep = clamp((level - cut) / max(1.0 - cut, 0.001), 0.0, 1.0);
  color = vec4(light * keep, 1.0);
}
