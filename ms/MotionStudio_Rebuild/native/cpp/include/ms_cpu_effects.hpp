#pragma once
#include "ms_error.hpp"
#include "ms_image.hpp"
#include <string>
#include <unordered_map>

namespace ms {

/** CPU reference implementations — real pixel work, no GPU required */
Error cpu_effect_apply(const std::string& effect_id,
                       const ImageBuffer& in,
                       ImageBuffer& out,
                       const std::unordered_map<std::string, double>& params);

Error cpu_gaussian_blur(const ImageBuffer& in, ImageBuffer& out, float radius);
Error cpu_brightness_contrast(const ImageBuffer& in, ImageBuffer& out,
                              float brightness, float contrast);
Error cpu_opacity(const ImageBuffer& in, ImageBuffer& out, float opacity);
Error cpu_grayscale(const ImageBuffer& in, ImageBuffer& out);
Error cpu_sepia(const ImageBuffer& in, ImageBuffer& out, float amount);
Error cpu_vignette(const ImageBuffer& in, ImageBuffer& out, float strength, float radius);
Error cpu_mirror(const ImageBuffer& in, ImageBuffer& out, int horizontal);
Error cpu_solid(ImageBuffer& out, int w, int h,
                float r, float g, float b, float a);

} // namespace ms
