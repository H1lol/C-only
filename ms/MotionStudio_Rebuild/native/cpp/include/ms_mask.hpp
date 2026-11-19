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

/**
 * How a mask combines with the other masks on the same clip.
 *
 * Add masks union together. Subtract masks are carved out of that union. A clip
 * whose masks are all Subtract starts from a full frame, so one subtract mask
 * alone punches a hole in an otherwise unmatted clip instead of hiding it all.
 */
enum class MaskOp : uint8_t {
  Add = 0,
  Subtract = 1,
};

/** Hard cap on masks per clip, so a document cannot carry an unbounded list. */
constexpr size_t kMaxClipMasks = 8;

struct Mask {
  uint64_t id = 0;
  std::string name = "Mask";
  MaskShape shape = MaskShape::Rect;
  MaskMode mode = MaskMode::Alpha;
  MaskOp op = MaskOp::Add;
  // normalized 0..1 in layer space
  float x = 0.1f, y = 0.1f, w = 0.8f, h = 0.8f;
  float feather = 0.f; // pixels
  bool inverted = false;
  bool enabled = true;
};

/** 0 rect, 1 ellipse, anything else full frame. */
MaskShape mask_shape_from_index(int index);

/**
 * Strength of one mask shape at one pixel, 0..1, with its own invert applied.
 * Exposed so the single- and multi-mask paths cannot drift apart.
 */
float mask_strength_at(const Mask& mask, int x, int y, int width, int height);

/**
 * Rasterize one mask to RGBA with the strength in every channel (including
 * alpha, which is what MaskMode::Alpha reads).
 */
Error rasterize_mask(const Mask& mask, int width, int height, ImageBuffer& out);

/**
 * Combine a clip's masks into one matte. Add masks union, Subtract masks cut,
 * disabled masks are skipped.
 */
Error rasterize_masks(const std::vector<Mask>& masks, int width, int height,
                      ImageBuffer& out);

/** Multiply source alpha by mask strength */
Error apply_mask_to_image(const ImageBuffer& src, const ImageBuffer& mask_img,
                          MaskMode mode, ImageBuffer& out);

/**
 * Same as apply_mask_to_image, in place, so matting a clip costs no second
 * full-size frame buffer.
 */
Error apply_mask_inplace(ImageBuffer& img, const ImageBuffer& mask_img, MaskMode mode);

} // namespace ms
