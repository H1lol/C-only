// Effects that read the frame through a warped coordinate: lens dispersion, a
// kaleidoscope fold, swirls, waves, a halftone screen, CRT scanlines and a VHS
// tape. They share one bilinear sampler that works on premultiplied coverage,
// so a warp cannot pull the colour of a transparent neighbour into the edge of
// a glyph (the same defect that darkened blurred titles).
//
// Every routine here is an original implementation of a standard image
// operation. No shader, source file, parameter text or asset from any
// third-party editor was copied.
#include "ms_cpu_effects.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ms {
namespace {

constexpr float kPi = 3.14159265358979f;

inline float clampf(float v, float lo, float hi) { return std::min(std::max(v, lo), hi); }

/** Deterministic per-pixel noise, so a frame renders the same every time. */
inline float hash01(int x, int y, int seed) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
               static_cast<uint32_t>(seed) * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return static_cast<float>(h & 0xffffff) / 16777215.f;
}

/** A premultiplied RGBA8 frame. Averaging and warping happen in this space. */
struct Plane {
  int w = 0;
  int h = 0;
  std::vector<uint8_t> p;
};

bool plane_from(const ImageBuffer& in, Plane& plane) {
  if (!in.data() || in.width() <= 0 || in.height() <= 0) return false;
  plane.w = in.width();
  plane.h = in.height();
  premultiply_rgba(in, plane.p);
  return true;
}

inline const uint8_t* at(const Plane& s, int x, int y) {
  return s.p.data() + (static_cast<size_t>(y) * s.w + x) * 4;
}

/** Bilinear sample; coordinates clamp to the frame so edges do not smear black. */
inline void sample(const Plane& s, float x, float y, float out[4]) {
  const float cx = clampf(x, 0.f, static_cast<float>(s.w - 1));
  const float cy = clampf(y, 0.f, static_cast<float>(s.h - 1));
  const int x0 = static_cast<int>(cx);
  const int y0 = static_cast<int>(cy);
  const int x1 = std::min(x0 + 1, s.w - 1);
  const int y1 = std::min(y0 + 1, s.h - 1);
  const float fx = cx - static_cast<float>(x0);
  const float fy = cy - static_cast<float>(y0);
  const uint8_t* a = at(s, x0, y0);
  const uint8_t* b = at(s, x1, y0);
  const uint8_t* c = at(s, x0, y1);
  const uint8_t* d = at(s, x1, y1);
  for (int i = 0; i < 4; ++i) {
    const float top = a[i] + (b[i] - a[i]) * fx;
    const float bot = c[i] + (d[i] - c[i]) * fx;
    out[i] = top + (bot - top) * fy;
  }
}

inline void put(Plane& dst, int x, int y, const float v[4]) {
  uint8_t* q = dst.p.data() + (static_cast<size_t>(y) * dst.w + x) * 4;
  for (int i = 0; i < 4; ++i)
    q[i] = static_cast<uint8_t>(clampf(v[i], 0.f, 255.f) + 0.5f);
}

/** Prepares source and destination planes; clears the destination. */
bool begin(const ImageBuffer& in, Plane& src, Plane& dst) {
  if (!plane_from(in, src)) return false;
  dst.w = src.w;
  dst.h = src.h;
  dst.p.assign(src.p.size(), 0);
  return true;
}

/** Copies the premultiplied result back and returns it to straight alpha. */
Error finish(Plane& dst, ImageBuffer& out) {
  out.allocate(dst.w, dst.h, PixelFormat::RGBA8);
  if (!out.data()) return Error::make(ErrorCode::InvalidArgument, "alloc failed");
  std::memcpy(out.data(), dst.p.data(), dst.p.size());
  unpremultiply_rgba(out.data(), static_cast<size_t>(dst.w) * dst.h);
  return Error::success();
}

/** A zero setting is a true identity, not a lossy premultiplied round trip. */
Error passthrough(const ImageBuffer& in, ImageBuffer& out) {
  if (!in.data() || in.width() <= 0 || in.height() <= 0)
    return Error::make(ErrorCode::InvalidArgument, "empty input");
  out.allocate(in.width(), in.height(), PixelFormat::RGBA8);
  if (!out.data()) return Error::make(ErrorCode::InvalidArgument, "alloc failed");
  std::memcpy(out.data(), in.data(), static_cast<size_t>(in.width()) * in.height() * 4);
  return Error::success();
}

} // namespace

// ------------------------------------------------------------------ lens ----

Error cpu_chromatic_aberration(const ImageBuffer& in, ImageBuffer& out,
                               float amount, float cx, float cy) {
  if (amount <= 0.f) return passthrough(in, out);
  Plane src, dst;
  if (!begin(in, src, dst)) return Error::make(ErrorCode::InvalidArgument, "empty input");
  amount = clampf(amount, 0.f, 1.f);
  const float ox = clampf(cx, 0.f, 1.f) * static_cast<float>(src.w);
  const float oy = clampf(cy, 0.f, 1.f) * static_cast<float>(src.h);
  // Radially growing split: red is pushed out, blue pulled in.
  const float out_scale = 1.f + amount * 0.25f;
  const float in_scale = 1.f - amount * 0.25f;
  for (int y = 0; y < src.h; ++y) {
    for (int x = 0; x < src.w; ++x) {
      const float dx = static_cast<float>(x) - ox;
      const float dy = static_cast<float>(y) - oy;
      float r[4], g[4], b[4];
      sample(src, ox + dx * out_scale, oy + dy * out_scale, r);
      sample(src, static_cast<float>(x), static_cast<float>(y), g);
      sample(src, ox + dx * in_scale, oy + dy * in_scale, b);
      const float v[4] = {r[0], g[1], b[2], g[3]};
      put(dst, x, y, v);
    }
  }
  return finish(dst, out);
}

Error cpu_rgb_split(const ImageBuffer& in, ImageBuffer& out,
                    float distance, float angle_deg) {
  if (std::fabs(distance) < 0.5f) return passthrough(in, out);
  Plane src, dst;
  if (!begin(in, src, dst)) return Error::make(ErrorCode::InvalidArgument, "empty input");
  const float rad = angle_deg * kPi / 180.f;
  const float dx = std::cos(rad) * distance;
  const float dy = std::sin(rad) * distance;
  for (int y = 0; y < src.h; ++y) {
    for (int x = 0; x < src.w; ++x) {
      float r[4], g[4], b[4];
      sample(src, x + dx, y + dy, r);
      sample(src, static_cast<float>(x), static_cast<float>(y), g);
      sample(src, x - dx, y - dy, b);
      const float v[4] = {r[0], g[1], b[2], g[3]};
      put(dst, x, y, v);
    }
  }
  return finish(dst, out);
}

// ------------------------------------------------------------ distortion ----

Error cpu_kaleidoscope(const ImageBuffer& in, ImageBuffer& out,
                       int segments, float rotation_deg, float zoom) {
  Plane src, dst;
  if (!begin(in, src, dst)) return Error::make(ErrorCode::InvalidArgument, "empty input");
  segments = std::clamp(segments, 2, 24);
  zoom = clampf(zoom, 0.25f, 4.f);
  const float seg = 2.f * kPi / static_cast<float>(segments);
  const float rot = rotation_deg * kPi / 180.f;
  const float cx = src.w * 0.5f;
  const float cy = src.h * 0.5f;
  for (int y = 0; y < src.h; ++y) {
    for (int x = 0; x < src.w; ++x) {
      const float dx = static_cast<float>(x) - cx;
      const float dy = static_cast<float>(y) - cy;
      const float r = std::hypot(dx, dy);
      float a = std::atan2(dy, dx) + rot;
      a = std::fmod(a, seg);
      if (a < 0.f) a += seg;
      a = std::fabs(a - seg * 0.5f);  // mirror each wedge
      float v[4];
      sample(src, cx + r * std::cos(a) / zoom, cy + r * std::sin(a) / zoom, v);
      put(dst, x, y, v);
    }
  }
  return finish(dst, out);
}

Error cpu_twirl(const ImageBuffer& in, ImageBuffer& out,
                float angle_deg, float radius, float cx, float cy) {
  if (std::fabs(angle_deg) < 0.01f) return passthrough(in, out);
  Plane src, dst;
  if (!begin(in, src, dst)) return Error::make(ErrorCode::InvalidArgument, "empty input");
  const float ox = clampf(cx, 0.f, 1.f) * static_cast<float>(src.w);
  const float oy = clampf(cy, 0.f, 1.f) * static_cast<float>(src.h);
  const float reach = std::max(1.f, clampf(radius, 0.01f, 2.f) *
                                        0.5f * static_cast<float>(std::min(src.w, src.h)));
  const float angle = angle_deg * kPi / 180.f;
  for (int y = 0; y < src.h; ++y) {
    for (int x = 0; x < src.w; ++x) {
      const float dx = static_cast<float>(x) - ox;
      const float dy = static_cast<float>(y) - oy;
      const float d = std::hypot(dx, dy);
      float v[4];
      if (d < reach && d > 0.f) {
        // Strongest at the centre, easing to zero at the rim.
        const float t = 1.f - d / reach;
        const float a = angle * t * t;
        const float ca = std::cos(a), sa = std::sin(a);
        sample(src, ox + dx * ca - dy * sa, oy + dx * sa + dy * ca, v);
      } else {
        sample(src, static_cast<float>(x), static_cast<float>(y), v);
      }
      put(dst, x, y, v);
    }
  }
  return finish(dst, out);
}

Error cpu_wave_warp(const ImageBuffer& in, ImageBuffer& out,
                    float amplitude, float frequency, float phase_deg) {
  if (amplitude <= 0.f) return passthrough(in, out);
  Plane src, dst;
  if (!begin(in, src, dst)) return Error::make(ErrorCode::InvalidArgument, "empty input");
  const float amp = clampf(amplitude, 0.f, 0.3f) * static_cast<float>(std::min(src.w, src.h));
  const float freq = clampf(frequency, 0.1f, 64.f);
  const float phase = phase_deg * kPi / 180.f;
  const float w = static_cast<float>(src.w);
  const float h = static_cast<float>(src.h);
  for (int y = 0; y < src.h; ++y) {
    const float sy_off = amp * std::sin(2.f * kPi * freq * (static_cast<float>(y) / h) + phase);
    for (int x = 0; x < src.w; ++x) {
      const float sx_off = amp * std::sin(2.f * kPi * freq * (static_cast<float>(x) / w) + phase);
      float v[4];
      sample(src, static_cast<float>(x) + sx_off, static_cast<float>(y) + sy_off, v);
      put(dst, x, y, v);
    }
  }
  return finish(dst, out);
}

// --------------------------------------------------------------- screens ----

Error cpu_halftone(const ImageBuffer& in, ImageBuffer& out,
                   float cell, float angle_deg, float mix) {
  if (mix <= 0.f) return passthrough(in, out);
  Plane src, dst;
  if (!begin(in, src, dst)) return Error::make(ErrorCode::InvalidArgument, "empty input");
  cell = clampf(cell, 2.f, 64.f);
  mix = clampf(mix, 0.f, 1.f);
  const float rad = angle_deg * kPi / 180.f;
  const float ca = std::cos(rad), sa = std::sin(rad);
  const float cx = src.w * 0.5f;
  const float cy = src.h * 0.5f;
  for (int y = 0; y < src.h; ++y) {
    for (int x = 0; x < src.w; ++x) {
      // Rotated screen space.
      const float px = static_cast<float>(x) - cx;
      const float py = static_cast<float>(y) - cy;
      const float u = px * ca + py * sa;
      const float v = -px * sa + py * ca;
      const float ci = std::floor(u / cell);
      const float cj = std::floor(v / cell);
      const float cu = (ci + 0.5f) * cell;
      const float cv = (cj + 0.5f) * cell;
      // The pixel keeps its own colour; the screen only decides how much of it
      // survives, so thin glyph strokes stay readable instead of sampling empty
      // cell centres.
      float c[4];
      sample(src, static_cast<float>(x), static_cast<float>(y), c);
      const float luma = (0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]) / 255.f;
      // Half a cell is the largest dot that still leaves gaps at the corners,
      // so a solid white title screens into dots instead of staying solid.
      const float radius = std::sqrt(clampf(luma, 0.f, 1.f)) * cell * 0.5f;
      const float du = u - cu, dv = v - cv;
      const float dist = std::sqrt(du * du + dv * dv);
      const float covered = clampf((radius - dist) + 0.5f, 0.f, 1.f);
      const float keep = 1.f - mix + mix * covered;
      const float o[4] = {c[0] * keep, c[1] * keep, c[2] * keep, c[3] * keep};
      put(dst, x, y, o);
    }
  }
  return finish(dst, out);
}

Error cpu_scanlines(const ImageBuffer& in, ImageBuffer& out,
                    float spacing, float strength, float phase_deg) {
  if (strength <= 0.f) return passthrough(in, out);
  Plane src, dst;
  if (!begin(in, src, dst)) return Error::make(ErrorCode::InvalidArgument, "empty input");
  spacing = clampf(spacing, 2.f, 32.f);
  strength = clampf(strength, 0.f, 1.f);
  const float phase = phase_deg * kPi / 180.f;
  for (int y = 0; y < src.h; ++y) {
    const float row = (static_cast<float>(y) + 0.5f) / spacing;
    const float wave = 0.5f + 0.5f * std::cos(2.f * kPi * row + phase);
    const float gain = 1.f - strength * (1.f - wave);
    for (int x = 0; x < src.w; ++x) {
      const uint8_t* s = at(src, x, y);
      const float v[4] = {s[0] * gain, s[1] * gain, s[2] * gain, static_cast<float>(s[3])};
      put(dst, x, y, v);
    }
  }
  return finish(dst, out);
}

Error cpu_vhs(const ImageBuffer& in, ImageBuffer& out,
              float amount, float tracking, int seed) {
  if (amount <= 0.f) return passthrough(in, out);
  Plane src, dst;
  if (!begin(in, src, dst)) return Error::make(ErrorCode::InvalidArgument, "empty input");
  amount = clampf(amount, 0.f, 1.f);
  tracking = clampf(tracking, 0.f, 1.f);
  const float h = static_cast<float>(std::max(src.h, 1));
  // The tracking error sits in one band per seed, the way a tape loses lock.
  const float band_centre = 0.15f + 0.7f * hash01(seed, 0, 17);
  const float chroma = amount * 2.f;
  for (int y = 0; y < src.h; ++y) {
    const float t = static_cast<float>(y) / h;
    const float wobble = amount * 4.f * std::sin(t * 60.f + static_cast<float>(seed) * 0.7f);
    const float band = std::exp(-std::pow((t - band_centre) / 0.05f, 2.f));
    const float shift = wobble + tracking * band * 24.f * std::sin(t * 180.f + static_cast<float>(seed));
    const float scan = (y & 1) ? (1.f - 0.18f * amount) : 1.f;
    for (int x = 0; x < src.w; ++x) {
      float r[4], g[4], b[4];
      sample(src, static_cast<float>(x) + shift + chroma, static_cast<float>(y), r);
      sample(src, static_cast<float>(x) + shift, static_cast<float>(y), g);
      sample(src, static_cast<float>(x) + shift - chroma, static_cast<float>(y), b);
      const float noise = (hash01(x, y, seed) - 0.5f) * amount * 26.f;
      const float a = g[3];
      float v[4] = {clampf((r[0] + noise) * scan, 0.f, a),
                    clampf((g[1] + noise) * scan, 0.f, a),
                    clampf((b[2] + noise) * scan, 0.f, a), a};
      put(dst, x, y, v);
    }
  }
  return finish(dst, out);
}

// ------------------------------------------------------------- dispatch -----

bool cpu_effect_screen_try(const std::string& id,
                           const ImageBuffer& in,
                           ImageBuffer& out,
                           const std::unordered_map<std::string, double>& params) {
  auto get = [&](const char* k, double def) {
    auto it = params.find(k);
    return it == params.end() ? def : it->second;
  };
  auto is = [&](const char* bare) {
    return id == bare || id == std::string("org.motionstudio.") + bare;
  };

  if (is("chromatic_aberration") || is("chromatic_ab")) {
    cpu_chromatic_aberration(in, out, static_cast<float>(get("amount", 0.6)),
                             static_cast<float>(get("center_x", 0.5)),
                             static_cast<float>(get("center_y", 0.5)));
    return true;
  }
  if (is("rgb_split")) {
    cpu_rgb_split(in, out, static_cast<float>(get("distance", 6.0)),
                  static_cast<float>(get("angle", 0.0)));
    return true;
  }
  if (is("kaleidoscope")) {
    cpu_kaleidoscope(in, out, static_cast<int>(get("segments", 6.0)),
                     static_cast<float>(get("rotation", 0.0)),
                     static_cast<float>(get("zoom", 1.0)));
    return true;
  }
  if (is("twirl")) {
    cpu_twirl(in, out, static_cast<float>(get("angle", 180.0)),
              static_cast<float>(get("radius", 0.8)),
              static_cast<float>(get("center_x", 0.5)),
              static_cast<float>(get("center_y", 0.5)));
    return true;
  }
  if (is("wave_warp") || is("wave")) {
    cpu_wave_warp(in, out, static_cast<float>(get("amplitude", 0.02)),
                  static_cast<float>(get("frequency", 6.0)),
                  static_cast<float>(get("phase", 0.0)));
    return true;
  }
  if (is("halftone")) {
    cpu_halftone(in, out, static_cast<float>(get("cell", 6.0)),
                 static_cast<float>(get("angle", 45.0)),
                 static_cast<float>(get("mix", 1.0)));
    return true;
  }
  if (is("scanlines")) {
    cpu_scanlines(in, out, static_cast<float>(get("spacing", 4.0)),
                  static_cast<float>(get("strength", 0.45)),
                  static_cast<float>(get("phase", 0.0)));
    return true;
  }
  if (is("vhs")) {
    cpu_vhs(in, out, static_cast<float>(get("amount", 0.6)),
            static_cast<float>(get("tracking", 0.35)),
            static_cast<int>(get("seed", 3.0)));
    return true;
  }
  return false;
}

} // namespace ms
