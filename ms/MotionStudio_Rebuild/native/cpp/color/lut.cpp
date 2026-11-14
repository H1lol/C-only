#include "ms_lut.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ms {
namespace {

std::string trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

} // namespace

void Lut3D::sample(float r, float g, float b, float out[3]) const {
    if (!valid()) {
        out[0] = r;
        out[1] = g;
        out[2] = b;
        return;
    }
    const int n = size;
    const float fr = std::clamp(r, 0.f, 1.f) * static_cast<float>(n - 1);
    const float fg = std::clamp(g, 0.f, 1.f) * static_cast<float>(n - 1);
    const float fb = std::clamp(b, 0.f, 1.f) * static_cast<float>(n - 1);

    const int r0 = static_cast<int>(std::floor(fr));
    const int g0 = static_cast<int>(std::floor(fg));
    const int b0 = static_cast<int>(std::floor(fb));
    const int r1 = std::min(r0 + 1, n - 1);
    const int g1 = std::min(g0 + 1, n - 1);
    const int b1 = std::min(b0 + 1, n - 1);
    const float tr = fr - static_cast<float>(r0);
    const float tg = fg - static_cast<float>(g0);
    const float tb = fb - static_cast<float>(b0);

    auto at = [this, n](int ri, int gi, int bi, int c) -> float {
        const size_t idx = ((static_cast<size_t>(bi) * n + gi) * n + ri) * 3u +
                           static_cast<size_t>(c);
        return data[idx];
    };

    for (int c = 0; c < 3; ++c) {
        const float c00 = at(r0, g0, b0, c) * (1 - tr) + at(r1, g0, b0, c) * tr;
        const float c10 = at(r0, g1, b0, c) * (1 - tr) + at(r1, g1, b0, c) * tr;
        const float c01 = at(r0, g0, b1, c) * (1 - tr) + at(r1, g0, b1, c) * tr;
        const float c11 = at(r0, g1, b1, c) * (1 - tr) + at(r1, g1, b1, c) * tr;
        const float c0 = c00 * (1 - tg) + c10 * tg;
        const float c1 = c01 * (1 - tg) + c11 * tg;
        out[c] = c0 * (1 - tb) + c1 * tb;
    }
}

bool parse_cube_lut(const std::string& text, Lut3D& out) {
    std::istringstream in(text);
    std::string line;
    int declared = 0;
    std::vector<float> values;

    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (t.rfind("TITLE", 0) == 0) continue;
        if (t.rfind("DOMAIN_MIN", 0) == 0 || t.rfind("DOMAIN_MAX", 0) == 0) {
            // Only the default 0..1 domain is supported.
            if (t.rfind("DOMAIN_MAX", 0) == 0) {
                float a = 0, b = 0, c = 0;
                if (std::sscanf(t.c_str(), "DOMAIN_MAX %f %f %f", &a, &b, &c) == 3) {
                    if (std::fabs(a - 1.f) > 1e-4f || std::fabs(b - 1.f) > 1e-4f ||
                        std::fabs(c - 1.f) > 1e-4f) {
                        return false;
                    }
                }
            }
            continue;
        }
        if (t.rfind("LUT_3D_SIZE", 0) == 0) {
            declared = std::atoi(t.c_str() + 11);
            if (declared < 2 || declared > 64) return false;
            continue;
        }
        if (t.rfind("LUT_1D_SIZE", 0) == 0) return false;  // 1D cubes unsupported
        if (t[0] == 'L' || t[0] == 'D' || t[0] == 'T') continue;  // unknown keyword

        std::istringstream row(t);
        float r = 0, g = 0, b = 0;
        if (!(row >> r >> g >> b)) return false;
        values.push_back(r);
        values.push_back(g);
        values.push_back(b);
    }

    if (declared < 2) return false;
    const size_t expected = static_cast<size_t>(declared) * declared * declared * 3u;
    if (values.size() != expected) return false;

    out.size = declared;
    out.data = std::move(values);
    return out.valid();
}

bool load_cube_lut(const std::string& path, Lut3D& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse_cube_lut(ss.str(), out);
}

bool apply_lut(ImageBuffer& img, const Lut3D& lut, float strength) {
    if (!img.valid() || !lut.valid()) return false;
    if (img.format() != PixelFormat::RGBA8) return false;
    const float s = std::clamp(strength, 0.f, 1.f);
    if (s <= 0.f) return true;

    uint8_t* p = img.data();
    const size_t pixels = static_cast<size_t>(img.stride()) * img.height() / 4u;
    for (size_t i = 0; i < pixels; ++i) {
        uint8_t* px = p + i * 4u;
        const float r = px[0] / 255.f;
        const float g = px[1] / 255.f;
        const float b = px[2] / 255.f;
        float o[3];
        lut.sample(r, g, b, o);
        auto blend = [s](float orig, float graded) {
            const float v = orig + (graded - orig) * s;
            return static_cast<uint8_t>(std::lround(std::clamp(v, 0.f, 1.f) * 255.f));
        };
        px[0] = blend(r, o[0]);
        px[1] = blend(g, o[1]);
        px[2] = blend(b, o[2]);
    }
    return true;
}

} // namespace ms
