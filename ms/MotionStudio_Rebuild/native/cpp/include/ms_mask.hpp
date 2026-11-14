#pragma once
/**
 * Pass 3 — Masks / mattes for compositing
 */
#include "ms_image.hpp"
#include "ms_error.hpp"
#include "ms_types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace ms {

enum class MaskMode : uint8_t {
  None = 0,
  Alpha,       // use mask alpha
  Luma,        // use mask luminance
  InvertedAlpha,
  InvertedLuma,
};

enum class MaskShape : uint8_t {
  Rect = 0,
  Ellipse,
  Full,        // full frame white
};

struct Mask {
  uint64_t id = 0;
  std::string name = "Mask";
  MaskShape shape = MaskShape::Rect;
  MaskMode mode = MaskMode::Alpha;
  // normalized 0..1 in layer space
  float x = 0.1f, y = 0.1f, w = 0.8f, h = 0.8f;
  float feather = 0.f; // pixels
  bool inverted = false;
  bool enabled = true;
};

/** Rasterize mask to single-channel strength 0..1 into out (RGBA, R=G=B=strength) */
Error rasterize_mask(const Mask& mask, int width, int height, ImageBuffer& out);

/** Multiply source alpha by mask strength */
Error apply_mask_to_image(const ImageBuffer& src, const ImageBuffer& mask_img,
                          MaskMode mode, ImageBuffer& out);

} // namespace ms
