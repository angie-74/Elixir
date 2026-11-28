#version 450
#pragma parameter brightness "Brightness" 0.00 -0.50 0.50 0.01
#pragma parameter contrast "Contrast" 1.00 0.50 1.50 0.01
#pragma parameter saturation "Saturation" 1.00 0.00 2.00 0.01
#pragma parameter vibrance "Vibrance" 0.00 -1.00 1.00 0.01
#pragma parameter gamma "Gamma" 1.00 0.50 2.00 0.01
#pragma parameter temperature "Color temperature" 0.00 -1.00 1.00 0.01
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform Frame {
  float brightness;
  float contrast;
  float saturation;
  float vibrance;
  float gamma;
  float temperature;
};
layout(set = 0, binding = 1) uniform sampler2D source;
void main() {
  vec3 pixel = texture(source, uv).rgb;
  pixel *= vec3(1.0 + 0.12 * temperature, 1.0, 1.0 - 0.12 * temperature);
  pixel = (pixel - 0.5) * contrast + 0.5 + brightness;
  float luma = dot(pixel, vec3(0.2126, 0.7152, 0.0722));
  float most = max(pixel.r, max(pixel.g, pixel.b));
  float least = min(pixel.r, min(pixel.g, pixel.b));
  float boost = saturation * (1.0 + vibrance * (1.0 - clamp(most - least, 0.0, 1.0)));
  pixel = mix(vec3(luma), pixel, boost);
  pixel = pow(clamp(pixel, 0.0, 1.0), vec3(1.0 / gamma));
  color = vec4(pixel, 1.0);
}
