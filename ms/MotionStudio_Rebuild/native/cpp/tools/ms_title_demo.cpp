// Renders sample titles to PPM files so the rasteriser can be eyeballed
// without a device. Usage: ms_title_demo <out-dir>
#include "ms_text.hpp"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const int w = 960, h = 540;

  struct Sample {
    const char* family;
    const char* text;
    ms::TextStyle style;
  };

  struct Preset {
    const char* family;
    const char* text;
    float size;
    float outline;
    float shadow;
    const char* hex;
    const char* outline_hex;
    bool upper;
    ms::TextAnchor anchor;
  };

  // A stacked title card: big display line, small caption underneath.
  const Preset presets[] = {
      {"anton", "MOTION STUDIO", 96.f, 0.f, 6.f, "#FFFFFF", "#000000", true, ms::TextAnchor::Middle},
      {"bebas", "Cut. Grade. Deliver.", 88.f, 4.f, 4.f, "#FFC857", "#0E0E10", false, ms::TextAnchor::Top},
      {"playfair", "A Film by You", 84.f, 0.f, 8.f, "#FFFFFF", "#000000", false, ms::TextAnchor::Bottom},
      {"lobster", "Lobster", 110.f, 2.f, 5.f, "#FF6B57", "#0E0E10", false, ms::TextAnchor::Middle},
  };

  int index = 0;
  for (const auto& p : presets) {
    ms::TextStyle style;
    style.font = p.family;
    style.size = p.size;
    style.align = ms::TextAlign::Center;
    style.anchor = p.anchor;
    style.outline = p.outline;
    style.shadow = p.shadow;
    style.uppercase = p.upper;
    ms::set_text_color(style, p.hex);
    ms::TextStyle outline_style = style;
    ms::set_text_color(outline_style, p.outline_hex);
    style.outline_r = outline_style.r;
    style.outline_g = outline_style.g;
    style.outline_b = outline_style.b;
    style.box_width = p.upper ? 760.f : 0.f;

    ms::ImageBuffer img;
    const ms::Error e = ms::render_text(img, w, h, style, p.text);
    if (!e.ok()) {
      std::printf("render failed for %s: %s\n", p.family, e.message.c_str());
      return 1;
    }

    // Composite over a mid grey so the glyph edges are visible in the PPM.
    const std::string path = dir + "/title_" + p.family + ".ppm";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return 1;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; ++y) {
      const uint8_t* row = img.data() + static_cast<size_t>(y) * img.stride();
      for (int x = 0; x < w; ++x) {
        const float a = row[x * 4 + 3] / 255.f;
        for (int c = 0; c < 3; ++c) {
          const float base = (c == 0 ? 0.16f : c == 1 ? 0.18f : 0.22f);
          const float v = row[x * 4 + c] / 255.f * a + base * (1.f - a);
          const auto px = static_cast<unsigned char>(v * 255.f + 0.5f);
          std::fputc(px, f);
        }
      }
    }
    std::fclose(f);
    std::printf("wrote %s\n", path.c_str());
    ++index;
  }
  return index == 4 ? 0 : 1;
}
