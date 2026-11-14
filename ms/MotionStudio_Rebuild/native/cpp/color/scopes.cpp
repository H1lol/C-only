#include "ms_scopes.hpp"
#include <algorithm>

namespace ms {

Error Scopes::histogram(const ImageBuffer& img, ScopeHistogram& out) {
  if (!img.valid()) return Error::make(ErrorCode::InvalidArgument, "empty image");
  out = {};
  const uint8_t* p = img.data();
  const int n = img.width() * img.height();
  for (int i = 0; i < n; ++i) {
    uint8_t r = p[0], g = p[1], b = p[2];
    out.bins_r[r]++; out.bins_g[g]++; out.bins_b[b]++;
    int y = static_cast<int>(0.2126f * r + 0.7152f * g + 0.0722f * b + 0.5f);
    y = std::clamp(y, 0, 255);
    out.bins_y[y]++;
    p += 4;
  }
  return Error::success();
}

Error Scopes::waveform(const ImageBuffer& img, std::vector<WaveformColumn>& cols, int max_cols) {
  if (!img.valid()) return Error::make(ErrorCode::InvalidArgument, "empty image");
  max_cols = std::max(1, std::min(max_cols, img.width()));
  cols.assign(max_cols, {});
  const int w = img.width(), h = img.height();
  for (int c = 0; c < max_cols; ++c) {
    int x0 = c * w / max_cols;
    int x1 = (c + 1) * w / max_cols;
    float sum = 0; int count = 0;
    float mn = 1.f, mx = 0.f;
    for (int x = x0; x < x1; ++x) {
      for (int y = 0; y < h; ++y) {
        const uint8_t* p = img.data() + (y * w + x) * 4;
        float luma = (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.f;
        mn = std::min(mn, luma);
        mx = std::max(mx, luma);
        sum += luma; ++count;
      }
    }
    cols[c].min_y = mn;
    cols[c].max_y = mx;
    cols[c].avg_y = count ? sum / count : 0;
  }
  return Error::success();
}

} // namespace ms
