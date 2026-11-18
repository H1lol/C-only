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
  void clear() { keys_.clear(); seeded_ = false; }

  /**
   * True while the curve holds only the static placeholder that
   * `ClipMotion::ensure_defaults()` inserts. The first authored key replaces
   * that placeholder, and only that one: clearing on every add while the curve
   * happened to hold a single key deleted the key you just made.
   */
  bool seeded() const { return seeded_; }
  void set_seeded(bool value) { seeded_ = value; }
  const std::vector<AnimKeyframe>& keys() const { return keys_; }
  bool empty() const { return keys_.empty(); }

  const AnimKeyframe* at(size_t index) const {
    return index < keys_.size() ? &keys_[index] : nullptr;
  }

  bool set_interp(size_t index, CurveInterp i) {
    if (index >= keys_.size()) return false;
    keys_[index].interp = i;
    return true;
  }

  /** Retimes/retargets one key. Keys stay sorted by time, so indices can move. */
  bool set_key(size_t index, double time, double value) {
    if (index >= keys_.size()) return false;
    keys_[index].time = std::max(0.0, time);
    keys_[index].value = value;
    sort_keys();
    return true;
  }

  /** Slope in value per second at `i`, used by the smooth (bezier) segment. */
  double slope(size_t i) const {
    if (keys_.size() < 2) return 0.0;
    if (i == 0)
      return (keys_[1].value - keys_[0].value) /
             std::max(1e-9, keys_[1].time - keys_[0].time);
    if (i + 1 >= keys_.size())
      return (keys_[i].value - keys_[i - 1].value) /
             std::max(1e-9, keys_[i].time - keys_[i - 1].time);
    return (keys_[i + 1].value - keys_[i - 1].value) /
           std::max(1e-9, keys_[i + 1].time - keys_[i - 1].time);
  }

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
        if (a.interp == CurveInterp::Bezier) {
          // Cubic Hermite. Tangents default to the Catmull-Rom slope through the
          // neighbouring keys, so the curve stays smooth across every key; an
          // explicit in_tan/out_tan overrides that.
          const double h = b.time - a.time;
          double m0 = a.out_tan;
          double m1 = b.in_tan;
          if (std::abs(m0) < 1e-9) m0 = slope(i);
          if (std::abs(m1) < 1e-9) m1 = slope(i + 1);
          const double u2 = u * u;
          const double u3 = u2 * u;
          const double h00 = 2.0 * u3 - 3.0 * u2 + 1.0;
          const double h10 = u3 - 2.0 * u2 + u;
          const double h01 = -2.0 * u3 + 3.0 * u2;
          const double h11 = u3 - u2;
          return h00 * a.value + h10 * h * m0 + h01 * b.value + h11 * h * m1;
        }
        u = ease(u, a.interp);
        return a.value + (b.value - a.value) * u;
      }
    }
    return keys_.back().value;
  }

private:
  std::vector<AnimKeyframe> keys_;
  bool seeded_ = false;
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
