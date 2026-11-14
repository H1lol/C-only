#pragma once
/**
 * StykAnimate-inspired stick / bone hierarchy
 * Joint angles as AnimCurves — render simple stick figure to ImageBuffer
 */
#include "ms_keyframe.hpp"
#include "ms_image.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>
#include <cmath>

namespace ms {

struct StickJoint {
  std::string name;
  int parent = -1;          // index in joints, -1 = root
  float length = 40.f;      // pixels at scale 1
  float rest_angle = -90.f; // degrees (0 = +X, -90 = up)
  AnimCurve angle;          // additive degrees over time
};

struct StickFigure {
  std::string name = "Stick";
  float root_x = 0.5f; // normalized canvas
  float root_y = 0.7f;
  float scale = 1.f;
  std::vector<StickJoint> joints;

  /** Humanoid default: root, torso, head, armL, armR, legL, legR */
  static StickFigure humanoid();
};

Error render_stick_figure(const StickFigure& fig, double time,
                          int width, int height, ImageBuffer& out);

} // namespace ms
