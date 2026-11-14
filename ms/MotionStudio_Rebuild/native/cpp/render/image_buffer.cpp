#include "../include/ms_image.hpp"
#include <algorithm>
#include <cmath>

namespace ms {

ImageBuffer::ImageBuffer(int w, int h, PixelFormat fmt, ColorSpace cs) {
    allocate(w, h, fmt, cs);
}

bool ImageBuffer::allocate(int w, int h, PixelFormat fmt, ColorSpace cs) {
    if (w <= 0 || h <= 0) return false;
    width_ = w;
    height_ = h;
    format_ = fmt;
    color_space_ = cs;
    stride_ = static_cast<size_t>(w) * bytes_per_pixel(fmt);
    data_.assign(stride_ * static_cast<size_t>(h), 0);
    return true;
}

void ImageBuffer::release() {
    data_.clear();
    width_ = height_ = 0;
    stride_ = 0;
}

void ImageBuffer::clear(float r, float g, float b, float a) {
    if (!valid() || format_ != PixelFormat::RGBA8) return;
    uint8_t R = static_cast<uint8_t>(std::clamp(r, 0.f, 1.f) * 255.f);
    uint8_t G = static_cast<uint8_t>(std::clamp(g, 0.f, 1.f) * 255.f);
    uint8_t B = static_cast<uint8_t>(std::clamp(b, 0.f, 1.f) * 255.f);
    uint8_t A = static_cast<uint8_t>(std::clamp(a, 0.f, 1.f) * 255.f);
    for (size_t i = 0; i < data_.size(); i += 4) {
        data_[i+0] = R;
        data_[i+1] = G;
        data_[i+2] = B;
        data_[i+3] = A;
    }
}

bool ImageBuffer::copy_from(const ImageBuffer& src) {
    if (!src.valid()) return false;
    if (!allocate(src.width_, src.height_, src.format_, src.color_space_)) return false;
    std::memcpy(data_.data(), src.data_.data(), data_.size());
    return true;
}

// -------------------------------------------------------
// Color conversion
// -------------------------------------------------------

namespace color {

void convert_srgb_to_linear(ImageBuffer& img) {
    if (!img.valid() || img.format() != PixelFormat::RGBA8) return;
    uint8_t* p = img.data();
    size_t n = img.size_bytes() / 4;
    for (size_t i = 0; i < n; ++i) {
        float r = p[0] / 255.f;
        float g = p[1] / 255.f;
        float b = p[2] / 255.f;
        p[0] = static_cast<uint8_t>(std::clamp(srgb_to_linear(r), 0.f, 1.f) * 255.f);
        p[1] = static_cast<uint8_t>(std::clamp(srgb_to_linear(g), 0.f, 1.f) * 255.f);
        p[2] = static_cast<uint8_t>(std::clamp(srgb_to_linear(b), 0.f, 1.f) * 255.f);
        p += 4;
    }
}

void convert_linear_to_srgb(ImageBuffer& img) {
    if (!img.valid() || img.format() != PixelFormat::RGBA8) return;
    uint8_t* p = img.data();
    size_t n = img.size_bytes() / 4;
    for (size_t i = 0; i < n; ++i) {
        float r = p[0] / 255.f;
        float g = p[1] / 255.f;
        float b = p[2] / 255.f;
        p[0] = static_cast<uint8_t>(std::clamp(linear_to_srgb(r), 0.f, 1.f) * 255.f);
        p[1] = static_cast<uint8_t>(std::clamp(linear_to_srgb(g), 0.f, 1.f) * 255.f);
        p[2] = static_cast<uint8_t>(std::clamp(linear_to_srgb(b), 0.f, 1.f) * 255.f);
        p += 4;
    }
}

} // namespace color

} // namespace ms
