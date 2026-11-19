#include "ms_finishing.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ms {

Error apply_finishing_grade(ImageBuffer& img, const GradeSettings& g) {
  if (!img.valid()) return Error::make(ErrorCode::InvalidArgument, "grade img");
  ColorPipeline cp;
  Error e = cp.apply_grade(img, g.exposure, g.contrast, g.saturation);
  if (!e.ok()) return e;

  // lift/gamma/gain + temperature on top
  uint8_t* p = img.data();
  const int n = img.width() * img.height();
  const float temp = std::clamp(g.temperature, -1.f, 1.f);
  for (int i = 0; i < n; ++i) {
    float r = p[0] / 255.f, gv = p[1] / 255.f, b = p[2] / 255.f;
    // lift
    r = r + g.lift; gv = gv + g.lift; b = b + g.lift;
    // gamma
    auto gam = [&](float x) {
      x = std::clamp(x, 0.f, 1.f);
      return g.gamma == 1.f ? x : std::pow(x, 1.f / std::max(0.01f, g.gamma));
    };
    r = gam(r); gv = gam(gv); b = gam(b);
    r *= g.gain; gv *= g.gain; b *= g.gain;
    // temperature: shift r/b
    r += temp * 0.05f;
    b -= temp * 0.05f;
    p[0] = static_cast<uint8_t>(std::clamp(r, 0.f, 1.f) * 255.f + 0.5f);
    p[1] = static_cast<uint8_t>(std::clamp(gv, 0.f, 1.f) * 255.f + 0.5f);
    p[2] = static_cast<uint8_t>(std::clamp(b, 0.f, 1.f) * 255.f + 0.5f);
    p += 4;
  }

  // Tonal split and vibrance, on top of lift/gamma/gain. Both weights are
  // squared around the luma, so the midtones barely move while the dark and
  // bright ends respond: that is the difference between this and a contrast
  // slider, which pivots the whole curve.
  const float shadows = std::clamp(g.shadows, -1.f, 1.f);
  const float highlights = std::clamp(g.highlights, -1.f, 1.f);
  const float vibrance = std::clamp(g.vibrance, -1.f, 1.f);
  if (std::fabs(shadows) > 1e-4f || std::fabs(highlights) > 1e-4f ||
      std::fabs(vibrance) > 1e-4f) {
    uint8_t* q = img.data();
    for (int i = 0; i < n; ++i) {
      float r = q[0] / 255.f, gv = q[1] / 255.f, b = q[2] / 255.f;
      const float luma = 0.2126f * r + 0.7152f * gv + 0.0722f * b;
      // Mid grey is the pivot: neither control moves a 50% pixel, which is what
      // separates this from a contrast slider, which pivots the whole curve.
      const float below = std::clamp((0.5f - luma) * 2.f, 0.f, 1.f);
      const float above = std::clamp((luma - 0.5f) * 2.f, 0.f, 1.f);
      const float shadow_w = below * below;
      const float high_w = above * above;
      const float shift = shadows * 0.5f * shadow_w + highlights * 0.5f * high_w;
      r += shift; gv += shift; b += shift;

      if (std::fabs(vibrance) > 1e-4f) {
        const float maxc = std::max(r, std::max(gv, b));
        const float minc = std::min(r, std::min(gv, b));
        const float sat = std::clamp(maxc - minc, 0.f, 1.f);
        const float amount = vibrance * (1.f - sat) * 0.8f;
        r += (r - luma) * amount;
        gv += (gv - luma) * amount;
        b += (b - luma) * amount;
      }

      q[0] = static_cast<uint8_t>(std::clamp(r, 0.f, 1.f) * 255.f + 0.5f);
      q[1] = static_cast<uint8_t>(std::clamp(gv, 0.f, 1.f) * 255.f + 0.5f);
      q[2] = static_cast<uint8_t>(std::clamp(b, 0.f, 1.f) * 255.f + 0.5f);
      q += 4;
    }
  }
  return Error::success();
}

Error export_graded_sequence(TimelinePlayer& player, const FinishExportOptions& opt) {
  namespace fs = std::filesystem;
  fs::create_directories(opt.out_dir);
  float fps = opt.fps > 1.f ? opt.fps : 24.f;
  Error pe = player.prepare();
  if (!pe.ok()) return pe;
  int frame_i = 0;
  for (TimeSec t = opt.start; t < opt.end - 1e-9; t += 1.0 / fps, ++frame_i) {
    ImageBuffer buf;
    Error e = player.render_frame(t, buf);
    if (!e.ok()) return e;
    if (opt.apply_grade) {
      e = apply_finishing_grade(buf, opt.grade);
      if (!e.ok()) return e;
    }
    std::ostringstream name;
    name << opt.out_dir << "/graded_" << std::setw(6) << std::setfill('0') << frame_i << ".ppm";
    std::ofstream f(name.str(), std::ios::binary);
    if (!f) return Error::make(ErrorCode::FilePermission, "write", name.str());
    f << "P6\n" << buf.width() << " " << buf.height() << "\n255\n";
    for (int y = 0; y < buf.height(); ++y) {
      const uint8_t* row = buf.data() + y * buf.stride();
      for (int x = 0; x < buf.width(); ++x) {
        f.put(static_cast<char>(row[x*4]));
        f.put(static_cast<char>(row[x*4+1]));
        f.put(static_cast<char>(row[x*4+2]));
      }
    }
  }
  std::ofstream done(opt.out_dir + "/finish_ok.txt");
  done << "frames=" << frame_i << "\n";
  return Error::success();
}

} // namespace ms
