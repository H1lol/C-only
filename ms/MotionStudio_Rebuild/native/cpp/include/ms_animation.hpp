#pragma once
/**
 * Pass 4 — Animation: multi-property curves, motion blur, simple expressions
 */
#include "ms_keyframe.hpp"
#include "ms_types.hpp"
#include "ms_image.hpp"
#include "ms_error.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

namespace ms {

/** Named animated properties for a layer/clip */
class PropertySet {
public:
  AnimCurve& curve(const std::string& name) { return curves_[name]; }
  const AnimCurve* find(const std::string& name) const {
    auto it = curves_.find(name);
    return it == curves_.end() ? nullptr : &it->second;
  }
  double get(const std::string& name, double time, double def = 0.0) const {
    auto it = curves_.find(name);
    if (it == curves_.end() || it->second.empty()) return def;
    return it->second.eval(time);
  }
  void set_key(const std::string& name, double time, double value,
               CurveInterp i = CurveInterp::Linear) {
    curves_[name].add(time, value, i);
  }
  const std::unordered_map<std::string, AnimCurve>& all() const { return curves_; }

private:
  std::unordered_map<std::string, AnimCurve> curves_;
};

/** Sample transform from property set at time */
Transform2D transform_at(const PropertySet& props, double time, const Transform2D& base = {});

/**
 * Motion blur: average N samples over shutter window around time.
 * sample_fn(t, ImageBuffer&) must render one frame.
 */
Error motion_blur_accumulate(
    double time, double shutter, int samples,
    int width, int height,
    const std::function<Error(double, ImageBuffer&)>& sample_fn,
    ImageBuffer& out);

/**
 * Tiny expression evaluator: supports number, + - * /, and $prop names from props at time.
 * Example: "1.0 + $opacity * 0.5"
 */
double eval_simple_expression(const std::string& expr, const PropertySet& props, double time);

/** Pass 4 — Text / shape layer sources */
Error render_text_placeholder(ImageBuffer& out, int w, int h,
                              const std::string& text,
                              float r, float g, float b);
Error render_shape_rect(ImageBuffer& out, int w, int h,
                        float r, float g, float b, float margin_n = 0.1f);
Error render_shape_ellipse(ImageBuffer& out, int w, int h,
                           float r, float g, float b);

} // namespace ms
