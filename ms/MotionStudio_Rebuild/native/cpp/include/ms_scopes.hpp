#pragma once
/** Phase 4 — simple waveform / histogram scopes (CPU) */
#include "ms_image.hpp"
#include "ms_error.hpp"
#include <vector>
#include <cstdint>

namespace ms {

struct ScopeHistogram {
  int bins_r[256]{};
  int bins_g[256]{};
  int bins_b[256]{};
  int bins_y[256]{};
};

struct WaveformColumn {
  float min_y = 1.f, max_y = 0.f, avg_y = 0.f;
};

class Scopes {
public:
  static Error histogram(const ImageBuffer& img, ScopeHistogram& out);
  /** One column per x (or decimated) of luma range */
  static Error waveform(const ImageBuffer& img, std::vector<WaveformColumn>& cols, int max_cols = 256);
};

} // namespace ms
