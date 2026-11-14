#include "ms_transform_ops.hpp"
#include <cmath>
#include <algorithm>

namespace ms {

Error blit_transformed(const ImageBuffer& src, ImageBuffer& dst,
                       const Transform2D& xf, float opacity) {
  if (!src.valid() || !dst.valid())
    return Error::make(ErrorCode::InvalidArgument, "blit_transformed");
  opacity = std::clamp(opacity, 0.f, 1.f);
  const int sw = src.width(), sh = src.height();
  const int dw = dst.width(), dh = dst.height();

  const float ax = xf.anchor.x * sw;
  const float ay = xf.anchor.y * sh;
  const float rad = xf.rotation * 3.14159265f / 180.f;
  const float cs = std::cos(rad), sn = std::sin(rad);
  const float sx = xf.scale.x == 0.f ? 1e-6f : xf.scale.x;
  const float sy = xf.scale.y == 0.f ? 1e-6f : xf.scale.y;
  // destination center + position
  const float cx = dw * 0.5f + xf.position.x;
  const float cy = dh * 0.5f + xf.position.y;

  auto sample = [&](float u, float v, float& r, float& g, float& b, float& a) {
    if (u < 0 || v < 0 || u >= sw - 1 || v >= sh - 1) {
      // edge clamp soft
      if (u < -1 || v < -1 || u > sw || v > sh) { r = g = b = a = 0; return; }
    }
    int x0 = std::clamp(static_cast<int>(std::floor(u)), 0, sw - 1);
    int y0 = std::clamp(static_cast<int>(std::floor(v)), 0, sh - 1);
    int x1 = std::min(x0 + 1, sw - 1);
    int y1 = std::min(y0 + 1, sh - 1);
    float tx = u - std::floor(u);
    float ty = v - std::floor(v);
    // bilinear sample
    const uint8_t* p00 = src.data() + (y0 * sw + x0) * 4;
    const uint8_t* p10 = src.data() + (y0 * sw + x1) * 4;
    const uint8_t* p01 = src.data() + (y1 * sw + x0) * 4;
    const uint8_t* p11 = src.data() + (y1 * sw + x1) * 4;
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    r = lerp(lerp(p00[0], p10[0], tx), lerp(p01[0], p11[0], tx), ty) / 255.f;
    g = lerp(lerp(p00[1], p10[1], tx), lerp(p01[1], p11[1], tx), ty) / 255.f;
    b = lerp(lerp(p00[2], p10[2], tx), lerp(p01[2], p11[2], tx), ty) / 255.f;
    a = lerp(lerp(p00[3], p10[3], tx), lerp(p01[3], p11[3], tx), ty) / 255.f;
  };

  // Inverse map: for each dst pixel, find src
  // dst = R*S*(src - anchor) + center
  // src = S^-1 * R^-1 * (dst - center) + anchor
  for (int y = 0; y < dh; ++y) {
    for (int x = 0; x < dw; ++x) {
      float dx = x + 0.5f - cx;
      float dy = y + 0.5f - cy;
      // inverse rotate
      float rx =  cs * dx + sn * dy;
      float ry = -sn * dx + cs * dy;
      // inverse scale
      float u = rx / sx + ax;
      float v = ry / sy + ay;
      float sr, sg, sb, sa;
      sample(u, v, sr, sg, sb, sa);
      sa *= opacity;
      if (sa < 0.001f) continue;
      uint8_t* d = dst.data() + (y * dw + x) * 4;
      float dr = d[0]/255.f, dg = d[1]/255.f, db = d[2]/255.f, da = d[3]/255.f;
      float ia = 1.f - sa;
      float or_ = sr * sa + dr * ia;
      float og = sg * sa + dg * ia;
      float ob = sb * sa + db * ia;
      float oa = sa + da * ia;
      d[0] = static_cast<uint8_t>(std::clamp(or_, 0.f, 1.f) * 255.f + 0.5f);
      d[1] = static_cast<uint8_t>(std::clamp(og, 0.f, 1.f) * 255.f + 0.5f);
      d[2] = static_cast<uint8_t>(std::clamp(ob, 0.f, 1.f) * 255.f + 0.5f);
      d[3] = static_cast<uint8_t>(std::clamp(oa, 0.f, 1.f) * 255.f + 0.5f);
    }
  }
  return Error::success();
}

} // namespace ms
