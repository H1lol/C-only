#pragma once
/**
 * Phase 3 — Motion on clips: keyframed opacity + string effect stack
 */
#include "ms_keyframe.hpp"
#include "ms_types.hpp"
#include "ms_image.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace ms {

/** Lightweight effect op for CPU stack (Phase 3) — avoids clashing with EffectInstance id type */
struct EffectOp {
  std::string id; // "gaussian_blur", "invert", "brightness_contrast"
  std::unordered_map<std::string, double> params;
  bool enabled = true;
};

struct ClipMotion {
  AnimCurve opacity;
  AnimCurve pos_x;
  AnimCurve pos_y;
  AnimCurve scale;
  std::vector<EffectOp> effects;

  void ensure_defaults() {
    if (opacity.empty()) opacity.add(0, 1.0);
    if (scale.empty()) scale.add(0, 1.0);
    if (pos_x.empty()) pos_x.add(0, 0.0);
    if (pos_y.empty()) pos_y.add(0, 0.0);
  }

  double opacity_at(TimeSec t) const {
    return opacity.empty() ? 1.0 : opacity.eval(t);
  }
};

class EffectStack {
public:
  static Error apply(const std::vector<EffectOp>& effects,
                     const ImageBuffer& in, ImageBuffer& out);
};

} // namespace ms
