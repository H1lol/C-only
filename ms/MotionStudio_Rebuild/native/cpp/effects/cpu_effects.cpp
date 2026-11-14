#include "ms_cpu_effects.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ms {
namespace {

inline void sample_rgba8(const ImageBuffer& img, int x, int y,
                         float& r, float& g, float& b, float& a) {
  x = std::clamp(x, 0, img.width() - 1);
  y = std::clamp(y, 0, img.height() - 1);
  const uint8_t* p = img.data() + (static_cast<size_t>(y) * img.width() + x) * 4;
  r = p[0] / 255.f;
  g = p[1] / 255.f;
  b = p[2] / 255.f;
  a = p[3] / 255.f;
}

inline void store_rgba8(ImageBuffer& img, int x, int y,
                        float r, float g, float b, float a) {
  if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) return;
  uint8_t* p = img.data() + (static_cast<size_t>(y) * img.width() + x) * 4;
  p[0] = static_cast<uint8_t>(std::clamp(r, 0.f, 1.f) * 255.f + 0.5f);
  p[1] = static_cast<uint8_t>(std::clamp(g, 0.f, 1.f) * 255.f + 0.5f);
  p[2] = static_cast<uint8_t>(std::clamp(b, 0.f, 1.f) * 255.f + 0.5f);
  p[3] = static_cast<uint8_t>(std::clamp(a, 0.f, 1.f) * 255.f + 0.5f);
}

} // namespace

Error cpu_solid(ImageBuffer& out, int w, int h, float r, float g, float b, float a) {
  if (w <= 0 || h <= 0) return Error::make(ErrorCode::InvalidArgument, "solid size");
  out.allocate(w, h, PixelFormat::RGBA8);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      store_rgba8(out, x, y, r, g, b, a);
  return Error::success();
}

Error cpu_opacity(const ImageBuffer& in, ImageBuffer& out, float opacity) {
  if (!in.data() || in.width() <= 0) return Error::make(ErrorCode::InvalidArgument, "empty input");
  opacity = std::clamp(opacity, 0.f, 1.f);
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  const int n = in.width() * in.height();
  const uint8_t* s = in.data();
  uint8_t* d = out.data();
  for (int i = 0; i < n; ++i) {
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
    d[3] = static_cast<uint8_t>(std::clamp((s[3] / 255.f) * opacity, 0.f, 1.f) * 255.f + 0.5f);
    s += 4; d += 4;
  }
  return Error::success();
}

Error cpu_brightness_contrast(const ImageBuffer& in, ImageBuffer& out,
                              float brightness, float contrast) {
  if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty input");
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  // brightness -1..1, contrast 0..2 (1 = neutral)
  const float b = std::clamp(brightness, -1.f, 1.f);
  const float c = std::max(contrast, 0.f);
  const int n = in.width() * in.height();
  const uint8_t* s = in.data();
  uint8_t* d = out.data();
  for (int i = 0; i < n; ++i) {
    auto adj = [&](uint8_t v) {
      float x = v / 255.f;
      x = (x - 0.5f) * c + 0.5f + b;
      return static_cast<uint8_t>(std::clamp(x, 0.f, 1.f) * 255.f + 0.5f);
    };
    d[0] = adj(s[0]); d[1] = adj(s[1]); d[2] = adj(s[2]); d[3] = s[3];
    s += 4; d += 4;
  }
  return Error::success();
}

Error cpu_gaussian_blur(const ImageBuffer& in, ImageBuffer& out, float radius) {
  if (!in.data() || in.width() <= 0) return Error::make(ErrorCode::InvalidArgument, "empty input");
  const int w = in.width();
  const int h = in.height();
  out.allocate(w, h, PixelFormat::RGBA8);

  radius = std::clamp(radius, 0.f, 64.f);
  if (radius < 0.5f) {
    std::memcpy(out.data(), in.data(), static_cast<size_t>(w) * h * 4);
    return Error::success();
  }

  const int R = std::max(1, static_cast<int>(std::ceil(radius)));
  // Separable box approximation (fast, good enough for preview)
  std::vector<uint8_t> tmp(static_cast<size_t>(w) * h * 4);

  auto blur_axis = [&](const uint8_t* src, uint8_t* dst, bool horizontal) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        float r = 0, g = 0, b = 0, a = 0;
        int count = 0;
        for (int k = -R; k <= R; ++k) {
          int sx = horizontal ? x + k : x;
          int sy = horizontal ? y : y + k;
          if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
          const uint8_t* p = src + (static_cast<size_t>(sy) * w + sx) * 4;
          r += p[0]; g += p[1]; b += p[2]; a += p[3];
          ++count;
        }
        uint8_t* d = dst + (static_cast<size_t>(y) * w + x) * 4;
        if (count > 0) {
          d[0] = static_cast<uint8_t>(r / count + 0.5f);
          d[1] = static_cast<uint8_t>(g / count + 0.5f);
          d[2] = static_cast<uint8_t>(b / count + 0.5f);
          d[3] = static_cast<uint8_t>(a / count + 0.5f);
        }
      }
    }
  };

  blur_axis(in.data(), tmp.data(), true);
  blur_axis(tmp.data(), out.data(), false);
  return Error::success();
}


Error cpu_grayscale(const ImageBuffer& in, ImageBuffer& out) {
  if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty");
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  const int n = in.width() * in.height();
  const uint8_t* s = in.data(); uint8_t* d = out.data();
  for (int i = 0; i < n; ++i) {
    uint8_t y = static_cast<uint8_t>(0.2126f*s[0] + 0.7152f*s[1] + 0.0722f*s[2] + 0.5f);
    d[0]=d[1]=d[2]=y; d[3]=s[3]; s+=4; d+=4;
  }
  return Error::success();
}

Error cpu_sepia(const ImageBuffer& in, ImageBuffer& out, float amount) {
  if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty");
  amount = std::clamp(amount, 0.f, 1.f);
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  const int n = in.width() * in.height();
  const uint8_t* s = in.data(); uint8_t* d = out.data();
  for (int i = 0; i < n; ++i) {
    float r=s[0]/255.f, g=s[1]/255.f, b=s[2]/255.f;
    float sr = std::min(1.f, 0.393f*r + 0.769f*g + 0.189f*b);
    float sg = std::min(1.f, 0.349f*r + 0.686f*g + 0.168f*b);
    float sb = std::min(1.f, 0.272f*r + 0.534f*g + 0.131f*b);
    d[0]=static_cast<uint8_t>((r+(sr-r)*amount)*255);
    d[1]=static_cast<uint8_t>((g+(sg-g)*amount)*255);
    d[2]=static_cast<uint8_t>((b+(sb-b)*amount)*255);
    d[3]=s[3]; s+=4; d+=4;
  }
  return Error::success();
}

Error cpu_vignette(const ImageBuffer& in, ImageBuffer& out, float strength, float radius) {
  if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty");
  strength = std::clamp(strength, 0.f, 1.f);
  radius = std::clamp(radius, 0.1f, 1.5f);
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  const int w=in.width(), h=in.height();
  float cx=w*0.5f, cy=h*0.5f, maxd=std::sqrt(cx*cx+cy*cy)*radius;
  for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
    const uint8_t* s = in.data()+(y*w+x)*4;
    uint8_t* d = out.data()+(y*w+x)*4;
    float dx=x-cx, dy=y-cy;
    float d0=std::sqrt(dx*dx+dy*dy)/maxd;
    float f=1.f - strength*std::clamp(d0,0.f,1.f)*std::clamp(d0,0.f,1.f);
    d[0]=static_cast<uint8_t>(s[0]*f); d[1]=static_cast<uint8_t>(s[1]*f);
    d[2]=static_cast<uint8_t>(s[2]*f); d[3]=s[3];
  }
  return Error::success();
}

Error cpu_mirror(const ImageBuffer& in, ImageBuffer& out, int horizontal) {
  if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty");
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  const int w=in.width(), h=in.height();
  for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
    int sx = horizontal ? (w-1-x) : x;
    int sy = horizontal ? y : (h-1-y);
    const uint8_t* s = in.data()+(sy*w+sx)*4;
    uint8_t* d = out.data()+(y*w+x)*4;
    d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d[3]=s[3];
  }
  return Error::success();
}

Error cpu_effect_apply(const std::string& effect_id,
                       const ImageBuffer& in,
                       ImageBuffer& out,
                       const std::unordered_map<std::string, double>& params) {
  auto get = [&](const char* k, double def) {
    auto it = params.find(k);
    return it == params.end() ? def : it->second;
  };

  if (effect_id == "org.motionstudio.gaussian_blur" || effect_id == "gaussian_blur" || effect_id == "blur") {
    return cpu_gaussian_blur(in, out, static_cast<float>(get("radius", 4.0)));
  }
  if (effect_id == "brightness_contrast" || effect_id == "org.motionstudio.brightness_contrast") {
    return cpu_brightness_contrast(in, out,
                                   static_cast<float>(get("brightness", 0.0)),
                                   static_cast<float>(get("contrast", 1.0)));
  }
  if (effect_id == "grayscale" || effect_id == "org.motionstudio.grayscale") {
    return cpu_grayscale(in, out);
  }
  if (effect_id == "sepia" || effect_id == "org.motionstudio.sepia") {
    return cpu_sepia(in, out, static_cast<float>(get("amount", 1.0)));
  }
  if (effect_id == "vignette" || effect_id == "org.motionstudio.vignette") {
    return cpu_vignette(in, out, static_cast<float>(get("strength", 0.6)),
                        static_cast<float>(get("radius", 1.0)));
  }
  if (effect_id == "mirror" || effect_id == "org.motionstudio.mirror") {
    return cpu_mirror(in, out, static_cast<int>(get("horizontal", 1.0)));
  }
  if (effect_id == "opacity") {
    return cpu_opacity(in, out, static_cast<float>(get("opacity", 1.0)));
  }
  if (effect_id == "invert" || effect_id == "org.motionstudio.invert") {
    if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty input");
    out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
    const int n = in.width() * in.height();
    const uint8_t* s = in.data();
    uint8_t* d = out.data();
    for (int i = 0; i < n; ++i) {
      d[0] = 255 - s[0]; d[1] = 255 - s[1]; d[2] = 255 - s[2]; d[3] = s[3];
      s += 4; d += 4;
    }
    return Error::success();
  }

  if (effect_id == "chromakey" || effect_id == "org.motionstudio.chromakey") {
    float tol = static_cast<float>(get("tolerance", 0.2));
    if (!in.data()) return Error::make(ErrorCode::InvalidArgument, "empty");
    out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
    const int n = in.width() * in.height();
    const uint8_t* s = in.data(); uint8_t* d = out.data();
    // key near green
    for (int i = 0; i < n; ++i) {
      float g = s[1] / 255.f, r = s[0] / 255.f, b = s[2] / 255.f;
      float dist = std::abs(g - 0.8f) + std::abs(r - 0.1f) + std::abs(b - 0.1f);
      d[0]=s[0]; d[1]=s[1]; d[2]=s[2];
      d[3] = dist < tol * 3.f ? 0 : s[3];
      s += 4; d += 4;
    }
    return Error::success();
  }
  // Unknown: copy through
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  if (in.data() && out.data()) {
    std::memcpy(out.data(), in.data(), static_cast<size_t>(in.width()) * in.height() * 4);
  }
  return Error::make(ErrorCode::EffectNotFound, "unknown effect, passthrough", effect_id);
}

} // namespace ms
