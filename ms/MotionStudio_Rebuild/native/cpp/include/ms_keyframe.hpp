#pragma once
/** Keyframes — OpenShot / Natron / AE-style param animation */
#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>

namespace ms {

enum class CurveInterp : uint8_t {
  Linear = 0,
  Hold,
  EaseIn,
  EaseOut,
  EaseInOut,
  Bezier
};

struct AnimKeyframe {
  double time = 0.0;
  double value = 0.0;
  CurveInterp interp = CurveInterp::Linear;
  double in_tan = 0.0;
  double out_tan = 0.0;
};

class AnimCurve {
public:
  void add(double time, double value, CurveInterp i = CurveInterp::Linear) {
    AnimKeyframe k;
    k.time = time;
    k.value = value;
    k.interp = i;
    keys_.push_back(k);
    sort_keys();
  }
  void clear() { keys_.clear(); }
  const std::vector<AnimKeyframe>& keys() const { return keys_; }
  bool empty() const { return keys_.empty(); }

  double eval(double t) const {
    if (keys_.empty()) return 0.0;
    if (t <= keys_.front().time) return keys_.front().value;
    if (t >= keys_.back().time) return keys_.back().value;
    for (size_t i = 0; i + 1 < keys_.size(); ++i) {
      const AnimKeyframe& a = keys_[i];
      const AnimKeyframe& b = keys_[i + 1];
      if (t >= a.time && t <= b.time) {
        if (a.interp == CurveInterp::Hold) return a.value;
        double u = (b.time - a.time) > 1e-12 ? (t - a.time) / (b.time - a.time) : 0.0;
        u = ease(u, a.interp);
        return a.value + (b.value - a.value) * u;
      }
    }
    return keys_.back().value;
  }

private:
  std::vector<AnimKeyframe> keys_;
  void sort_keys() {
    std::sort(keys_.begin(), keys_.end(),
              [](const AnimKeyframe& x, const AnimKeyframe& y) { return x.time < y.time; });
  }
  static double ease(double u, CurveInterp i) {
    u = std::clamp(u, 0.0, 1.0);
    switch (i) {
      case CurveInterp::EaseIn: return u * u;
      case CurveInterp::EaseOut: return 1.0 - (1.0 - u) * (1.0 - u);
      case CurveInterp::EaseInOut:
        return u < 0.5 ? 2.0 * u * u : 1.0 - 2.0 * (1.0 - u) * (1.0 - u);
      default: return u;
    }
  }
};

} // namespace ms
