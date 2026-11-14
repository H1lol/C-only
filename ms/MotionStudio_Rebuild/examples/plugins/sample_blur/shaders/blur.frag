// MotionStudio .msp sample — Gaussian-ish blur (single pass approximation)
precision mediump float;
uniform sampler2D uInput;
uniform vec2 uResolution;
uniform float uParam_radius;
uniform float uParam_quality;
out vec4 fragColor;

void main() {
  vec2 uv = gl_FragCoord.xy / uResolution;
  float r = max(uParam_radius, 0.0) * uParam_quality;
  vec2 texel = 1.0 / uResolution;
  vec4 c = texture(uInput, uv) * 0.227027;
  c += texture(uInput, uv + vec2(texel.x * r, 0.0)) * 0.316216;
  c += texture(uInput, uv - vec2(texel.x * r, 0.0)) * 0.316216;
  c += texture(uInput, uv + vec2(0.0, texel.y * r)) * 0.070270;
  c += texture(uInput, uv - vec2(0.0, texel.y * r)) * 0.070270;
  fragColor = c;
}
