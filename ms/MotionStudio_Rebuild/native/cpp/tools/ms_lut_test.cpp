// Verifies .cube parsing and look application (strength blend, clamping, errors).
#include "ms_lut.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace ms;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

static std::string identity_cube(int n) {
    std::string s = "# identity\nTITLE \"Identity\"\nLUT_3D_SIZE " + std::to_string(n) + "\n";
    char buf[128];
    for (int b = 0; b < n; ++b) {
        for (int g = 0; g < n; ++g) {
            for (int r = 0; r < n; ++r) {
                std::snprintf(buf, sizeof(buf), "%.6f %.6f %.6f\n",
                              r / float(n - 1), g / float(n - 1), b / float(n - 1));
                s += buf;
            }
        }
    }
    return s;
}

static std::string swap_rb_cube() {
    // 2x2x2 cube where the output is the input with red and blue exchanged.
    std::string s = "TITLE \"Swap\"\nLUT_3D_SIZE 2\n";
    char buf[128];
    for (int b = 0; b < 2; ++b) {
        for (int g = 0; g < 2; ++g) {
            for (int r = 0; r < 2; ++r) {
                std::snprintf(buf, sizeof(buf), "%.6f %.6f %.6f\n", float(b), float(g), float(r));
                s += buf;
            }
        }
    }
    return s;
}

static ImageBuffer solid(uint8_t r, uint8_t g, uint8_t b) {
    ImageBuffer img(4, 4, PixelFormat::RGBA8);
    uint8_t* p = img.data();
    for (int i = 0; i < 16; ++i) {
        p[i * 4 + 0] = r;
        p[i * 4 + 1] = g;
        p[i * 4 + 2] = b;
        p[i * 4 + 3] = 255;
    }
    return img;
}

int main() {
    Lut3D identity;
    check(parse_cube_lut(identity_cube(17), identity), "parse identity cube");
    check(identity.size == 17, "cube size");
    check(identity.valid(), "cube valid");

    {
        ImageBuffer img = solid(200, 100, 50);
        check(apply_lut(img, identity, 1.f), "apply identity");
        const uint8_t* p = img.data();
        check(std::abs(int(p[0]) - 200) <= 1, "identity keeps red");
        check(std::abs(int(p[1]) - 100) <= 1, "identity keeps green");
        check(std::abs(int(p[2]) - 50) <= 1, "identity keeps blue");
        check(p[3] == 255, "alpha untouched");
    }

    {
        Lut3D swap;
        check(parse_cube_lut(swap_rb_cube(), swap), "parse swap cube");
        ImageBuffer img = solid(200, 100, 50);
        check(apply_lut(img, swap, 1.f), "apply swap");
        const uint8_t* p = img.data();
        check(std::abs(int(p[0]) - 50) <= 1, "swap moves blue into red");
        check(std::abs(int(p[2]) - 200) <= 1, "swap moves red into blue");
    }

    {
        Lut3D swap;
        parse_cube_lut(swap_rb_cube(), swap);
        ImageBuffer img = solid(200, 100, 50);
        check(apply_lut(img, swap, 0.5f), "apply half strength");
        const uint8_t* p = img.data();
        check(std::abs(int(p[0]) - 125) <= 2, "half strength blends red");
        check(std::abs(int(p[2]) - 125) <= 2, "half strength blends blue");
    }

    {
        ImageBuffer img = solid(10, 10, 10);
        Lut3D empty;
        check(!apply_lut(img, empty, 1.f), "reject empty lut");
        const uint8_t* p = img.data();
        check(p[0] == 10, "rejected lut leaves pixels alone");
    }

    check(!parse_cube_lut("not a cube", identity), "reject garbage");
    check(!parse_cube_lut("LUT_3D_SIZE 2\n0 0 0\n1 1 1\n", identity), "reject short data");
    check(!parse_cube_lut("LUT_1D_SIZE 16\n", identity), "reject 1d cube");
    check(!parse_cube_lut("DOMAIN_MAX 4 4 4\nLUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n"
                          "1 1 1\n", identity),
          "reject non-unit domain");

    if (failures == 0) {
        std::printf("ms_lut_test: all checks passed\n");
        return 0;
    }
    std::printf("ms_lut_test: %d failure(s)\n", failures);
    return 1;
}
