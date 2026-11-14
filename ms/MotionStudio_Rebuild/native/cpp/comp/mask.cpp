#include "ms_mask.hpp"
#include <algorithm>
#include <cmath>

namespace ms {

Error rasterize_mask(const Mask& mask, int width, int height, ImageBuffer& out) {
  if (width <= 0 || height <= 0)
    return Error::make(ErrorCode::InvalidArgument, "mask size");
  if (!out.allocate(width, height, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "mask buffer");

  if (!mask.enabled || mask.shape == MaskShape::Full) {
    out.clear(1.f, 1.f, 1.f, 1.f);
    return Error::success();
  }

  out.clear(0.f, 0.f, 0.f, 1.f);
  const float fx0 = mask.x * width;
  const float fy0 = mask.y * height;
  const float fx1 = (mask.x + mask.w) * width;
  const float fy1 = (mask.y + mask.h) * height;
  const float cx = 0.5f * (fx0 + fx1);
  const float cy = 0.5f * (fy0 + fy1);
  const float rx = std::max(1.f, 0.5f * (fx1 - fx0));
  const float ry = std::max(1.f, 0.5f * (fy1 - fy0));
  const float feather = std::max(0.f, mask.feather);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float s = 0.f;
      if (mask.shape == MaskShape::Rect) {
        // distance to rect edge
        float dx = 0.f, dy = 0.f;
        if (x < fx0) dx = fx0 - x;
        else if (x > fx1) dx = x - fx1;
        if (y < fy0) dy = fy0 - y;
        else if (y > fy1) dy = y - fy1;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dx == 0.f && dy == 0.f) s = 1.f;
        else if (feather > 0.f) s = 1.f - std::clamp(dist / feather, 0.f, 1.f);
        else s = 0.f;
        if (x >= fx0 && x <= fx1 && y >= fy0 && y <= fy1) s = 1.f;
        else if (feather > 0.f) s = 1.f - std::clamp(dist / feather, 0.f, 1.f);
      } else { // Ellipse
        float nx = (x - cx) / rx;
        float ny = (y - cy) / ry;
        float d = std::sqrt(nx * nx + ny * ny);
        if (d <= 1.f) s = 1.f;
        else if (feather > 0.f) {
          float px = (d - 1.f) * std::max(rx, ry);
          s = 1.f - std::clamp(px / feather, 0.f, 1.f);
        } else s = 0.f;
      }
      if (mask.inverted) s = 1.f - s;
      uint8_t* p = out.data() + (y * width + x) * 4;
      uint8_t v = static_cast<uint8_t>(std::clamp(s, 0.f, 1.f) * 255.f + 0.5f);
      p[0] = p[1] = p[2] = v;
      p[3] = 255;
    }
  }
  return Error::success();
}

Error apply_mask_to_image(const ImageBuffer& src, const ImageBuffer& mask_img,
                          MaskMode mode, ImageBuffer& out) {
  if (!src.valid() || !mask_img.valid())
    return Error::make(ErrorCode::InvalidArgument, "mask apply inputs");
  const int w = src.width(), h = src.height();
  if (mask_img.width() != w || mask_img.height() != h)
    return Error::make(ErrorCode::InvalidArgument, "mask size mismatch");
  if (!out.allocate(w, h, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "mask apply out");

  const uint8_t* s = src.data();
  const uint8_t* m = mask_img.data();
  uint8_t* d = out.data();
  const int n = w * h;
  for (int i = 0; i < n; ++i) {
    float strength = 1.f;
    switch (mode) {
      case MaskMode::None:
        strength = 1.f; break;
      case MaskMode::Alpha:
        strength = m[3] / 255.f; break;
      case MaskMode::Luma:
        strength = (0.2126f * m[0] + 0.7152f * m[1] + 0.0722f * m[2]) / 255.f; break;
      case MaskMode::InvertedAlpha:
        strength = 1.f - m[3] / 255.f; break;
      case MaskMode::InvertedLuma:
        strength = 1.f - (0.2126f * m[0] + 0.7152f * m[1] + 0.0722f * m[2]) / 255.f; break;
    }
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
    d[3] = static_cast<uint8_t>(std::clamp((s[3] / 255.f) * strength, 0.f, 1.f) * 255.f + 0.5f);
    s += 4; m += 4; d += 4;
  }
  return Error::success();
}

} // namespace ms
