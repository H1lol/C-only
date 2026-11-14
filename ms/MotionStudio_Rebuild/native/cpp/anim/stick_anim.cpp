#include "ms_stick_anim.hpp"
#include <algorithm>

namespace ms {

StickFigure StickFigure::humanoid() {
  StickFigure f;
  f.name = "Humanoid";
  // 0 root (hip)
  StickJoint root; root.name = "hip"; root.parent = -1; root.length = 0; root.rest_angle = 0;
  // 1 torso
  StickJoint torso; torso.name = "torso"; torso.parent = 0; torso.length = 50; torso.rest_angle = -90;
  // 2 head
  StickJoint head; head.name = "head"; head.parent = 1; head.length = 25; head.rest_angle = -90;
  // 3 arm L
  StickJoint armL; armL.name = "armL"; armL.parent = 1; armL.length = 40; armL.rest_angle = -160;
  // 4 arm R
  StickJoint armR; armR.name = "armR"; armR.parent = 1; armR.length = 40; armR.rest_angle = -20;
  // 5 leg L
  StickJoint legL; legL.name = "legL"; legL.parent = 0; legL.length = 45; legL.rest_angle = 120;
  // 6 leg R
  StickJoint legR; legR.name = "legR"; legR.parent = 0; legR.length = 45; legR.rest_angle = 60;
  f.joints = {root, torso, head, armL, armR, legL, legR};
  return f;
}

Error render_stick_figure(const StickFigure& fig, double time,
                          int width, int height, ImageBuffer& out) {
  if (!out.allocate(width, height, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "stick");
  out.clear(0.05f, 0.05f, 0.07f, 1.f);

  struct Node { float x, y; };
  std::vector<Node> pos(fig.joints.size());
  float ox = fig.root_x * width;
  float oy = fig.root_y * height;

  for (size_t i = 0; i < fig.joints.size(); ++i) {
    const auto& j = fig.joints[i];
    if (j.parent < 0) {
      pos[i] = {ox, oy};
      continue;
    }
    float ang = j.rest_angle;
    if (!j.angle.empty()) ang += static_cast<float>(j.angle.eval(time));
    float rad = ang * 3.14159265f / 180.f;
    float len = j.length * fig.scale;
    const Node& p = pos[static_cast<size_t>(j.parent)];
    pos[i] = {p.x + std::cos(rad) * len, p.y + std::sin(rad) * len};
  }

  auto plot = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    uint8_t* p = out.data() + (y * width + x) * 4;
    p[0] = 180; p[1] = 200; p[2] = 255; p[3] = 255;
  };
  auto line = [&](float x0, float y0, float x1, float y1) {
    int steps = std::max(2, static_cast<int>(std::hypot(x1 - x0, y1 - y0)));
    for (int s = 0; s <= steps; ++s) {
      float t = (float)s / steps;
      plot(static_cast<int>(x0 + (x1 - x0) * t), static_cast<int>(y0 + (y1 - y0) * t));
    }
  };

  for (size_t i = 0; i < fig.joints.size(); ++i) {
    if (fig.joints[i].parent < 0) continue;
    const Node& a = pos[static_cast<size_t>(fig.joints[i].parent)];
    const Node& b = pos[i];
    line(a.x, a.y, b.x, b.y);
    // joint dot
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        plot(static_cast<int>(b.x) + dx, static_cast<int>(b.y) + dy);
  }
  return Error::success();
}

} // namespace ms
