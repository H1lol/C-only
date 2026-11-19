#include "ms_mask.hpp"
#include <algorithm>
#include <cmath>

namespace ms {

MaskShape mask_shape_from_index(int index) {
  if (index <= 0) return MaskShape::Rect;
  if (index == 1) return MaskShape::Ellipse;
  return MaskShape::Full;
}

float mask_strength_at(const Mask& mask, int x, int y, int width, int height) {
  if (mask.shape == MaskShape::Full) return mask.inverted ? 0.f : 1.f;

  const float fx0 = mask.x * width;
  const float fy0 = mask.y * height;
  const float fx1 = (mask.x + mask.w) * width;
  const float fy1 = (mask.y + mask.h) * height;
  const float feather = std::max(0.f, mask.feather);
  const float px = static_cast<float>(x);
  const float py = static_cast<float>(y);

  float s = 0.f;
  if (mask.shape == MaskShape::Rect) {
    const float dx = px < fx0 ? fx0 - px : (px > fx1 ? px - fx1 : 0.f);
    const float dy = py < fy0 ? fy0 - py : (py > fy1 ? py - fy1 : 0.f);
    if (dx == 0.f && dy == 0.f) {
      s = 1.f;
    } else if (feather > 0.f) {
      s = 1.f - std::clamp(std::sqrt(dx * dx + dy * dy) / feather, 0.f, 1.f);
    } else {
      s = 0.f;
    }
  } else { // Ellipse
    const float cx = 0.5f * (fx0 + fx1);
    const float cy = 0.5f * (fy0 + fy1);
    const float rx = std::max(1.f, 0.5f * (fx1 - fx0));
    const float ry = std::max(1.f, 0.5f * (fy1 - fy0));
    const float nx = (px - cx) / rx;
    const float ny = (py - cy) / ry;
    const float d = std::sqrt(nx * nx + ny * ny);
    if (d <= 1.f) {
      s = 1.f;
    } else if (feather > 0.f) {
      s = 1.f - std::clamp(((d - 1.f) * std::max(rx, ry)) / feather, 0.f, 1.f);
    } else {
      s = 0.f;
    }
  }
  return mask.inverted ? 1.f - s : s;
}

Error rasterize_mask(const Mask& mask, int width, int height, ImageBuffer& out) {
  if (width <= 0 || height <= 0)
    return Error::make(ErrorCode::InvalidArgument, "mask size");
  if (!out.allocate(width, height, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "mask buffer");

  // A disabled mask leaves the clip untouched. A full-frame mask is a constant,
  // but it still has to honour invert (a black frame), which the old shared fast
  // path dropped: only the multi-mask path got that right.
  if (!mask.enabled) {
    out.clear(1.f, 1.f, 1.f, 1.f);
    return Error::success();
  }
  if (mask.shape == MaskShape::Full) {
    const float s = mask_strength_at(mask, 0, 0, width, height);
    out.clear(s, s, s, s);
    return Error::success();
  }

  out.clear(0.f, 0.f, 0.f, 0.f);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float s = std::clamp(mask_strength_at(mask, x, y, width, height), 0.f, 1.f);
      const uint8_t v = static_cast<uint8_t>(s * 255.f + 0.5f);
      uint8_t* p = out.data() + (y * width + x) * 4;
      // The strength used to live only in RGB while the apply step read alpha,
      // so an Alpha mask did nothing and an InvertedAlpha mask hid the clip.
      p[0] = p[1] = p[2] = p[3] = v;
    }
  }
  return Error::success();
}

// A mask's mode says how its matte is read: Alpha and Luma take the shape's
// strength as it is, the Inverted variants flip it. The combined matte is then
// applied as alpha, so the mode has to be folded in while combining.
static inline float mask_mode_strength(float strength, MaskMode mode) {
  switch (mode) {
    case MaskMode::InvertedAlpha:
    case MaskMode::InvertedLuma:  return 1.f - strength;
    case MaskMode::None:          return 1.f;
    default:                      return strength;
  }
}

Error rasterize_masks(const std::vector<Mask>& masks, int width, int height,
                      ImageBuffer& out) {
  if (width <= 0 || height <= 0)
    return Error::make(ErrorCode::InvalidArgument, "mask size");
  if (!out.allocate(width, height, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "mask buffer");

  std::vector<const Mask*> adds;
  std::vector<const Mask*> subs;
  for (const auto& m : masks) {
    if (!m.enabled) continue;
    (m.op == MaskOp::Add ? adds : subs).push_back(&m);
  }

  // Nothing enabled at all: leave the clip untouched rather than hiding it.
  if (adds.empty() && subs.empty()) {
    out.clear(1.f, 1.f, 1.f, 1.f);
    return Error::success();
  }

  // Only subtractive masks means "start from the whole frame and cut into it".
  const bool base_full = adds.empty();
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float union_strength = base_full ? 1.f : 0.f;
      for (const Mask* m : adds)
        union_strength = std::max(
            union_strength, mask_mode_strength(mask_strength_at(*m, x, y, width, height), m->mode));
      float cut = 0.f;
      for (const Mask* m : subs)
        cut = std::max(
            cut, mask_mode_strength(mask_strength_at(*m, x, y, width, height), m->mode));

      const float s = std::clamp(union_strength * (1.f - cut), 0.f, 1.f);
      const uint8_t v = static_cast<uint8_t>(s * 255.f + 0.5f);
      uint8_t* p = out.data() + (y * width + x) * 4;
      p[0] = p[1] = p[2] = p[3] = v;
    }
  }
  return Error::success();
}

static inline float mask_read_strength(const uint8_t* m, MaskMode mode) {
  switch (mode) {
    case MaskMode::None:          return 1.f;
    case MaskMode::Alpha:         return m[3] / 255.f;
    case MaskMode::Luma:          return (0.2126f * m[0] + 0.7152f * m[1] + 0.0722f * m[2]) / 255.f;
    case MaskMode::InvertedAlpha: return 1.f - m[3] / 255.f;
    case MaskMode::InvertedLuma:  return 1.f - (0.2126f * m[0] + 0.7152f * m[1] + 0.0722f * m[2]) / 255.f;
  }
  return 1.f;
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
    const float strength = mask_read_strength(m, mode);
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
    d[3] = static_cast<uint8_t>(std::clamp((s[3] / 255.f) * strength, 0.f, 1.f) * 255.f + 0.5f);
    s += 4; m += 4; d += 4;
  }
  return Error::success();
}

Error apply_mask_inplace(ImageBuffer& img, const ImageBuffer& mask_img, MaskMode mode) {
  if (!img.valid() || !mask_img.valid())
    return Error::make(ErrorCode::InvalidArgument, "mask apply inputs");
  const int w = img.width(), h = img.height();
  if (mask_img.width() != w || mask_img.height() != h)
    return Error::make(ErrorCode::InvalidArgument, "mask size mismatch");

  uint8_t* p = img.data();
  const uint8_t* m = mask_img.data();
  const int n = w * h;
  for (int i = 0; i < n; ++i) {
    const float strength = mask_read_strength(m, mode);
    p[3] = static_cast<uint8_t>(std::clamp((p[3] / 255.f) * strength, 0.f, 1.f) * 255.f + 0.5f);
    p += 4; m += 4;
  }
  return Error::success();
}

} // namespace ms
