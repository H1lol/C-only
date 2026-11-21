// Properties of the warp/screen effects that a "did pixels change?" check
// cannot see: alpha survives a lens split untouched, a warp never pulls the
// colour of a transparent neighbour into the frame, the zero settings are true
// identity, and repeated renders agree.
#include "ms_cpu_effects.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

using namespace ms;
using P = std::unordered_map<std::string, double>;

static ImageBuffer make_frame(int w, int h) {
  ImageBuffer img;
  img.allocate(w, h, PixelFormat::RGBA8);
  uint8_t* p = img.data();
  const float cx = w * 0.5f, cy = h * 0.5f, r = std::min(w, h) * 0.35f;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint8_t* o = p + (static_cast<size_t>(y) * w + x) * 4;
      const float d = std::hypot(x - cx, y - cy);
      const float a = d < r ? 1.f : (d < r + 2.f ? (r + 2.f - d) / 2.f : 0.f);
      o[0] = static_cast<uint8_t>(x * 255 / (w - 1));
      o[1] = static_cast<uint8_t>(y * 255 / (h - 1));
      o[2] = 200;
      o[3] = static_cast<uint8_t>(a * 255.f + 0.5f);
      if (a == 0.f) o[0] = o[1] = o[2] = 255;  // bright garbage under alpha 0
    }
  }
  return img;
}

static int failures = 0;

static void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

static bool identical(const ImageBuffer& a, const ImageBuffer& b) {
  if (a.width() != b.width() || a.height() != b.height() || !a.data() || !b.data()) return false;
  return std::memcmp(a.data(), b.data(),
                     static_cast<size_t>(a.width()) * a.height() * 4) == 0;
}

int main() {
  const int w = 96, h = 64;
  const ImageBuffer src = make_frame(w, h);

  // 1. The lens split redistributes colour but must not touch coverage.
  {
    ImageBuffer out;
    check(cpu_effect_apply("chromatic_aberration", src, out, P{{"amount", 0.8}}).ok(),
          "chromatic_aberration applies");
    bool alpha_same = out.width() == w && out.height() == h;
    for (int i = 0; alpha_same && i < w * h; ++i)
      alpha_same = out.data()[i * 4 + 3] == src.data()[i * 4 + 3];
    check(alpha_same, "chromatic_aberration leaves alpha untouched");
    check(!identical(src, out), "chromatic_aberration changes colour");
  }

  // 2. A warp must not drag a transparent neighbour's colour into the frame.
  {
    ImageBuffer out;
    check(cpu_effect_apply("wave_warp", src, out, P{{"amplitude", 0.08}}).ok(),
          "wave_warp applies");
    const int margin = 8;  // more than the warp can reach (0.08 * 64px)
    bool clean = out.width() == w && out.height() == h;
    for (int y = 0; clean && y < h; ++y)
      for (int x = 0; x < w; ++x) {
        // Only pixels whose whole neighbourhood is transparent: a warped pixel
        // may legitimately receive coverage from a neighbour.
        bool source_clear = true;
        for (int yy = std::max(0, y - margin); yy <= std::min(h - 1, y + margin) && source_clear; ++yy)
          for (int xx = std::max(0, x - margin); xx <= std::min(w - 1, x + margin); ++xx)
            if (src.data()[(static_cast<size_t>(yy) * w + xx) * 4 + 3] != 0) { source_clear = false; break; }
        if (!source_clear) continue;
        const uint8_t* d = out.data() + (static_cast<size_t>(y) * w + x) * 4;
        // 0-alpha pixels carry 255,255,255 in the source; anything but 0 here
        // means straight-alpha colour bled through the sampler.
        if (d[0] != 0 || d[1] != 0 || d[2] != 0) clean = false;
      }
    check(clean, "wave_warp keeps transparent pixels colour-free");
  }

  // 3. Zero settings are identity, not "almost identity".
  {
    ImageBuffer out;
    check(cpu_effect_apply("rgb_split", src, out, P{{"distance", 0.0}}).ok(), "rgb_split applies");
    check(identical(src, out), "rgb_split at distance 0 is identity");

    ImageBuffer ht;
    check(cpu_effect_apply("halftone", src, ht, P{{"mix", 0.0}}).ok(), "halftone applies");
    check(identical(src, ht), "halftone at mix 0 is identity");

    ImageBuffer sl;
    check(cpu_effect_apply("scanlines", src, sl, P{{"strength", 0.0}}).ok(), "scanlines applies");
    check(identical(src, sl), "scanlines at strength 0 is identity");

    ImageBuffer tw;
    check(cpu_effect_apply("twirl", src, tw, P{{"angle", 0.0}}).ok(), "twirl applies");
    check(identical(src, tw), "twirl at 0 degrees is identity");
  }

  // 4. Effect output is deterministic: the same frame and settings agree.
  {
    ImageBuffer a, b;
    cpu_effect_apply("kaleidoscope", src, a, P{{"segments", 5.0}, {"rotation", 12.0}});
    cpu_effect_apply("kaleidoscope", src, b, P{{"segments", 5.0}, {"rotation", 12.0}});
    check(identical(a, b), "kaleidoscope is deterministic");
    ImageBuffer c, d;
    cpu_effect_apply("vhs", src, c, P{{"amount", 0.7}, {"seed", 5.0}});
    cpu_effect_apply("vhs", src, d, P{{"amount", 0.7}, {"seed", 5.0}});
    check(identical(c, d), "vhs is deterministic");
  }

  if (failures == 0) std::printf("ms_effects_screen_test: all properties hold\n");
  return failures == 0 ? 0 : 1;
}
