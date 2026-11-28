#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  vec4 source_size;
};
layout(set = 0, binding = 1) uniform sampler2D source;
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main() {
  vec2 px = source_size.zw;
  vec3 middle = texture(source, uv).rgb;
  float lm = luma(middle);
  float lnw = luma(texture(source, uv + vec2(-1.0, -1.0) * px).rgb);
  float lne = luma(texture(source, uv + vec2(1.0, -1.0) * px).rgb);
  float lsw = luma(texture(source, uv + vec2(-1.0, 1.0) * px).rgb);
  float lse = luma(texture(source, uv + vec2(1.0, 1.0) * px).rgb);
  float lowest = min(lm, min(min(lnw, lne), min(lsw, lse)));
  float highest = max(lm, max(max(lnw, lne), max(lsw, lse)));
  if (highest - lowest < max(0.0312, highest * 0.125)) {
    color = vec4(middle, 1.0);
    return;
  }
  vec2 along = vec2(-((lnw + lne) - (lsw + lse)), (lnw + lsw) - (lne + lse));
  float reduce = max((lnw + lne + lsw + lse) * 0.03125, 0.0078125);
  along = clamp(along / (min(abs(along.x), abs(along.y)) + reduce), vec2(-8.0), vec2(8.0)) * px;
  vec3 tight = 0.5 * (texture(source, uv + along * (1.0 / 3.0 - 0.5)).rgb +
                     texture(source, uv + along * (2.0 / 3.0 - 0.5)).rgb);
  vec3 wide = tight * 0.5 + 0.25 * (texture(source, uv - along * 0.5).rgb + texture(source, uv + along * 0.5).rgb);
  float lf = luma(wide);
  color = vec4(lf < lowest || lf > highest ? tight : wide, 1.0);
}
