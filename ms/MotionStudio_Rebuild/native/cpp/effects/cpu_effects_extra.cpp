#include "ms_cpu_effects.hpp"
#include <algorithm>
#include <cmath>

namespace ms {

Error cpu_invert(const ImageBuffer& in, ImageBuffer& out) {
  if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty");
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  const uint8_t* s = in.data();
  uint8_t* d = out.data();
  int n = in.width() * in.height();
  for (int i = 0; i < n; ++i) {
    d[0] = 255 - s[0]; d[1] = 255 - s[1]; d[2] = 255 - s[2]; d[3] = s[3];
    s += 4; d += 4;
  }
  return Error::success();
}

Error cpu_threshold(const ImageBuffer& in, ImageBuffer& out, float level) {
  if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty");
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  level = std::clamp(level, 0.f, 1.f);
  const uint8_t* s = in.data();
  uint8_t* d = out.data();
  int n = in.width() * in.height();
  for (int i = 0; i < n; ++i) {
    float y = (0.2126f * s[0] + 0.7152f * s[1] + 0.0722f * s[2]) / 255.f;
    uint8_t v = y >= level ? 255 : 0;
    d[0] = d[1] = d[2] = v; d[3] = s[3];
    s += 4; d += 4;
  }
  return Error::success();
}

Error cpu_chromakey_simple(const ImageBuffer& in, ImageBuffer& out,
                           float kr, float kg, float kb, float tol) {
  if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty");
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  tol = std::max(tol, 0.01f);
  const uint8_t* s = in.data();
  uint8_t* d = out.data();
  int n = in.width() * in.height();
  for (int i = 0; i < n; ++i) {
    float r = s[0]/255.f, g = s[1]/255.f, b = s[2]/255.f;
    float dist = std::sqrt((r-kr)*(r-kr)+(g-kg)*(g-kg)+(b-kb)*(b-kb));
    float a = std::clamp(dist / tol, 0.f, 1.f);
    d[0]=s[0]; d[1]=s[1]; d[2]=s[2];
    d[3] = static_cast<uint8_t>(a * (s[3]/255.f) * 255.f + 0.5f);
    s += 4; d += 4;
  }
  return Error::success();
}

} // namespace ms
