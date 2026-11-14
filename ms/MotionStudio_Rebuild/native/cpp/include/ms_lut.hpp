#pragma once
/**
 * 3D colour cube (look) support.
 *
 * Reads the widely supported Adobe `.cube` format (LUT_3D_SIZE + RGB triples,
 * red index varying fastest) and applies it to an 8-bit RGBA buffer with
 * trilinear interpolation and a strength blend.
 */
#include "ms_image.hpp"

#include <string>
#include <vector>

namespace ms {

struct Lut3D {
    int size = 0;
    std::vector<float> data;  // size^3 * 3 floats, red index varies fastest

    bool valid() const {
        return size > 1 &&
               data.size() == static_cast<size_t>(size) * static_cast<size_t>(size) *
                                  static_cast<size_t>(size) * 3u;
    }

    /** Trilinear sample of the cube; inputs and outputs are 0..1. */
    void sample(float r, float g, float b, float out[3]) const;
};

/** Parses the body of a `.cube` file. Returns false when malformed. */
bool parse_cube_lut(const std::string& text, Lut3D& out);

/** Reads and parses a `.cube` file from disk. */
bool load_cube_lut(const std::string& path, Lut3D& out);

/**
 * Applies the cube to an RGBA8 image.
 * `strength` in 0..1 blends graded against original; 0 is a no-op.
 */
bool apply_lut(ImageBuffer& img, const Lut3D& lut, float strength = 1.f);

} // namespace ms
