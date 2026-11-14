#pragma once
/** Pass 3 — Apply Transform2D when sampling layers */
#include "ms_image.hpp"
#include "ms_types.hpp"
#include "ms_error.hpp"

namespace ms {

/**
 * Draw src into dst using transform (position in pixels from center,
 * scale, rotation degrees, anchor 0..1).
 * Bilinear sample; areas outside src are transparent.
 */
Error blit_transformed(const ImageBuffer& src, ImageBuffer& dst,
                       const Transform2D& xf, float opacity = 1.f);

} // namespace ms
