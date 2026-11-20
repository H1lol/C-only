/**
 * Alpha handling of the neighbour-averaging effects.
 *
 * The buffers an effect receives hold straight (not premultiplied) colour: a
 * fully transparent pixel can still carry any RGB value, and in practice it
 * carries black. Averaging straight values therefore dragged black into every
 * soft edge, and since the compositor multiplies by alpha again afterwards the
 * result was not just soft but dark — a blurred title came out dim:
 *
 *     blurred rgb = a·white, alpha = a  ->  composited = a² instead of a
 *
 * So the invariants below are: coverage (sum of alpha) is preserved, the blur
 * really spreads, and where alpha is partial the colour stays the original
 * colour rather than fading towards black.
 */
#include "ms_cpu_effects.hpp"
#include "ms_image.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace ms;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

static const uint8_t* at(const ImageBuffer& img, int x, int y) {
    return img.data() + static_cast<size_t>(y) * img.stride() + static_cast<size_t>(x) * 4;
}

/** Opaque white square on a fully transparent black field. Row packed, stride = w*4. */
static void white_square(ImageBuffer& img, int w, int h, int x0, int y0, int sw, int sh) {
    img.allocate(w, h, PixelFormat::RGBA8);
    img.clear(0.f, 0.f, 0.f, 0.f);
    for (int y = y0; y < y0 + sh; ++y) {
        for (int x = x0; x < x0 + sw; ++x) {
            uint8_t* p = img.data() + static_cast<size_t>(y) * img.stride() +
                         static_cast<size_t>(x) * 4;
            p[0] = p[1] = p[2] = p[3] = 255;
        }
    }
}

static double alpha_sum(const ImageBuffer& img) {
    double sum = 0.0;
    for (int y = 0; y < img.height(); ++y) {
        const uint8_t* row = at(img, 0, y);
        for (int x = 0; x < img.width(); ++x) sum += row[x * 4 + 3];
    }
    return sum;
}

/** Checks one blur: spread, coverage, and colour under partial alpha. */
static void check_blur(const char* name, Error (*blur)(const ImageBuffer&, ImageBuffer&, float),
                       float radius) {
    const int W = 64, H = 64;
    ImageBuffer in;
    white_square(in, W, H, 24, 24, 16, 16);

    ImageBuffer out;
    const Error e = blur(in, out, radius);
    check(e.ok() && out.valid(), name);

    // 1. It spreads: just outside the square there is now partial coverage.
    const uint8_t* edge = at(out, 21, 31);  // 3 px outside the square's left edge
    check(edge[3] > 0, "the blur spreads alpha outside the original square");
    check(edge[3] < 255, "the spread edge is partial, not a hard copy");

    // 2. Colour is not dragged towards black. Under any partial alpha the
    //    straight colour must stay white; the old code returned rgb ~= alpha.
    bool colour_ok = true;
    bool any_partial = false;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const uint8_t* p = at(out, x, y);
            if (p[3] == 0 || p[3] == 255) continue;
            any_partial = true;
            if (p[0] < 250 || p[1] < 250 || p[2] < 250) colour_ok = false;
        }
    }
    check(any_partial, "the blur produces partial alpha to inspect");
    check(colour_ok, "partial-alpha pixels keep the source colour instead of fading to black");

    // 3. Coverage is conserved: a normalised kernel moves alpha around, it does
    //    not create or destroy it. Allow a little for 8-bit rounding.
    const double before = alpha_sum(in);
    const double after = alpha_sum(out);
    check(std::fabs(after - before) <= before * 0.02 + 255.0,
          "the blur conserves total alpha");

    // 4. The untouched far corner stays fully transparent.
    check(at(out, 0, 0)[3] == 0, "far transparent area stays transparent");
}

static Error gaussian(const ImageBuffer& in, ImageBuffer& out, float r) {
    return cpu_gaussian_blur(in, out, r);
}
static Error box(const ImageBuffer& in, ImageBuffer& out, float r) {
    return cpu_box_blur(in, out, static_cast<int>(std::lround(r)));
}

int main() {
    check_blur("gaussian blur applies", gaussian, 4.f);
    check_blur("box blur applies", box, 4.f);

    // A zero radius is a byte-for-byte copy, including alpha.
    ImageBuffer in;
    white_square(in, 32, 32, 8, 8, 8, 8);
    ImageBuffer out;
    check(cpu_gaussian_blur(in, out, 0.f).ok(), "zero-radius gaussian returns");
    check(out.valid() && at(out, 10, 10)[3] == 255 && at(out, 0, 0)[3] == 0,
          "zero-radius gaussian is a passthrough");

    // Glow goes through the same blur and must not darken its input either.
    ImageBuffer glow;
    check(cpu_glow(in, glow, 0.5f, 4.f).ok(), "glow applies");
    check(glow.valid() && at(glow, 10, 10)[0] >= 250 && at(glow, 10, 10)[3] == 255,
          "glow leaves opaque white opaque white");

    if (failures == 0) {
        std::printf("ms_effects_alpha_test: all checks passed\n");
        return 0;
    }
    std::printf("%d alpha check(s) failed\n", failures);
    return 1;
}
