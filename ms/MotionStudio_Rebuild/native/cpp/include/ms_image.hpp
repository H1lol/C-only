#pragma once

#include "ms_types.hpp"
#include <cstdint>
#include <memory>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace ms {

// ============================================================
// Phase 2 — Image Buffers & Color
// ============================================================

enum class PixelFormat : uint8_t {
    RGBA8 = 0,      // 8-bit sRGB
    RGBA16F,        // 16-bit float (HDR-ish)
    RGBA32F,        // 32-bit float
    RGB8,
    R8,             // single channel
    YUV420,         // for video decode path
};

enum class ColorSpace : uint8_t {
    sRGB = 0,
    Linear,
    DisplayP3,      // future
    Rec2020,        // future
};

inline int bytes_per_pixel(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::RGBA8:   return 4;
        case PixelFormat::RGBA16F: return 8;
        case PixelFormat::RGBA32F: return 16;
        case PixelFormat::RGB8:    return 3;
        case PixelFormat::R8:      return 1;
        case PixelFormat::YUV420:  return 1; // approximate
        default: return 4;
    }
}

// ============================================================
// CPU Image Buffer
// ============================================================

class ImageBuffer {
public:
    ImageBuffer() = default;
    ImageBuffer(int w, int h, PixelFormat fmt = PixelFormat::RGBA8, ColorSpace cs = ColorSpace::sRGB);
    ~ImageBuffer() = default;

    ImageBuffer(const ImageBuffer&) = delete;
    ImageBuffer& operator=(const ImageBuffer&) = delete;
    ImageBuffer(ImageBuffer&&) noexcept = default;
    ImageBuffer& operator=(ImageBuffer&&) noexcept = default;

    bool allocate(int w, int h, PixelFormat fmt = PixelFormat::RGBA8, ColorSpace cs = ColorSpace::sRGB);
    void release();

    int  width()  const { return width_; }
    int  height() const { return height_; }
    PixelFormat format() const { return format_; }
    ColorSpace  color_space() const { return color_space_; }
    size_t      stride() const { return stride_; }      // bytes per row
    size_t      size_bytes() const { return data_.size(); }
    uint8_t*    data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }

    bool valid() const { return width_ > 0 && height_ > 0 && !data_.empty(); }

    // Clear to solid color (RGBA 0-1)
    void clear(float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);

    // Simple copy (same format assumed)
    bool copy_from(const ImageBuffer& src);

private:
    int         width_  = 0;
    int         height_ = 0;
    PixelFormat format_ = PixelFormat::RGBA8;
    ColorSpace  color_space_ = ColorSpace::sRGB;
    size_t      stride_ = 0;
    std::vector<uint8_t> data_;
};

// ============================================================
// GPU Texture handle (backend-agnostic)
// ============================================================

struct GpuTexture {
    uint32_t id     = 0;   // GL texture name / Vulkan image handle (opaque)
    int      width  = 0;
    int      height = 0;
    PixelFormat format = PixelFormat::RGBA8;
    bool     is_oes = false; // external OES texture from MediaCodec
};

// ============================================================
// Color conversion helpers
// ============================================================

namespace color {

// sRGB <-> Linear (approximate)
inline float srgb_to_linear(float c) {
    return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline float linear_to_srgb(float c) {
    return (c <= 0.0031308f) ? (12.92f * c) : (1.055f * std::pow(c, 1.0f/2.4f) - 0.055f);
}

// Convert whole buffer (CPU, slow — for correctness tests)
void convert_srgb_to_linear(ImageBuffer& img);
void convert_linear_to_srgb(ImageBuffer& img);

} // namespace color

} // namespace ms
