/**
 * Visual shot harness.
 *
 * Every check in this tree asserts on numbers. This one writes pictures: it
 * builds a small timeline and renders frames of the real compositor to PPM, so
 * a human (or an agent) can look at what the app actually puts on screen —
 * glyph shapes, matte edges, blur kernel, grade response, layer order.
 *
 *   ms_shots <out_dir> [font_dir]
 *
 * Not part of ctest: it has no pass/fail, only output to inspect.
 */
#include "ms_engine.hpp"
#include "ms_image.hpp"
#include "ms_project.hpp"
#include "ms_text.hpp"
#include "ms_timeline_player.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace ms;

namespace fs = std::filesystem;

static bool write_ppm(const std::string& path, const ImageBuffer& img) {
    if (!img.valid()) return false;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << img.width() << " " << img.height() << "\n255\n";
    for (int y = 0; y < img.height(); ++y) {
        const uint8_t* row = img.data() + static_cast<size_t>(y) * img.stride();
        for (int x = 0; x < img.width(); ++x) {
            f.put(static_cast<char>(row[x * 4 + 0]));
            f.put(static_cast<char>(row[x * 4 + 1]));
            f.put(static_cast<char>(row[x * 4 + 2]));
        }
    }
    return true;
}

struct Shot {
    std::string name;
    TimeSec time;
};

int main(int argc, char** argv) {
    const std::string out_dir = argc > 1 ? argv[1] : "shots";
    if (argc > 2) set_text_font_dir(argv[2]);
    else set_text_font_dir("android/app/src/main/assets/text");
    std::error_code ec;
    fs::create_directories(out_dir, ec);

    const int W = 640, H = 360;
    Engine& e = Engine::instance();
    if (!e.init(W, H) || !e.new_project("Shots", W, H, 30.f)) {
        std::printf("ms_shots: engine setup failed\n");
        return 1;
    }
    const auto tracks = e.project().tracks();
    if (tracks.empty()) {
        std::printf("ms_shots: no track\n");
        return 1;
    }
    const TrackId base = tracks[0].id;
    const TrackId overlay = e.add_track("Overlay", 0);

    TextStyle style;
    style.font = "anton";
    style.size = 70.f;
    style.align = TextAlign::Center;
    style.anchor = TextAnchor::Middle;

    std::vector<Shot> shots;

    // 1. Glyphs, unmasked and unblurred. Expect clean edges at 70px.
    {
        const ClipId c = e.add_text_clip(base, 0.0, 1.0, "MOTION STUDIO", style);
        shots.push_back({"01_text", 0.5});
        (void)c;
    }

    // 2. Same title with an ellipse matte, feathered, plus an outline and shadow,
    //    so the matte edge and the outline/shadow offsets are both visible.
    {
        TextStyle t = style;
        t.outline = 6.f;
        t.outline_r = t.outline_g = t.outline_b = 1.f;
        t.shadow = 10.f;
        t.shadow_alpha = 0.9f;
        t.r = 0.85f; t.g = 0.65f; t.b = 1.f;
        const ClipId c = e.add_text_clip(base, 2.0, 1.0, "MATTE EDGE", t);
        // Mask x/y is the top-left corner in normalised frame space.
        e.add_clip_mask(base, c, 1, 0.12f, 0.18f, 0.76f, 0.64f, 0.06f, false, 0);
        shots.push_back({"02_matte", 2.5});
    }

    // 3. The same title through a gaussian blur: the kernel has to spread, not
    //    just fade, and it must not drag the transparent background in as black.
    {
        TextStyle t = style;
        t.size = 54.f;
        t.r = t.g = t.b = 1.f;
        const ClipId c = e.add_text_clip(base, 4.0, 1.0, "BLUR 14", t);
        const int fx = e.add_effect(base, c, "gaussian_blur");
        if (fx >= 0) e.set_effect_param(base, c, fx, "radius", 14.f);
        shots.push_back({"03_blur", 4.5});
    }

    // 4. Grade response on the decoder-less slate fill (76, 38, 38). Shadows and
    //    highlights should move it in opposite directions and leave a visible
    //    ramp, not a flat lift.
    {
        ClipInfo c;
        c.name = "Slate";
        c.start = 6.0;
        c.duration = 1.0;
        c.source_in = 0.0;
        c.source_out = 1.0;
        c.motion.ensure_defaults();
        const ClipId id = e.project().add_clip(base, c);
        e.set_clip_grade_tonal(base, id, 0.6f, -0.4f, 0.0f);
        shots.push_back({"04_grade", 6.5});
    }

    // 5. Layer order and alpha: a full-screen slate on the base track with a
    //    title on the overlay track above it. The title must sit on the slate,
    //    not punch a hole in it.
    {
        ClipInfo c;
        c.name = "Slate";
        c.start = 8.0;
        c.duration = 1.0;
        c.motion.ensure_defaults();
        e.project().add_clip(base, c);
        if (overlay) e.add_text_clip(overlay, 8.0, 1.0, "OVERLAY", style);
        shots.push_back({"05_overlay", 8.5});
    }

    // 6. Everything at once through an adjustment layer: the layer above the
    //    picture blurs and grades what the tracks below already composited.
    {
        ClipInfo c;
        c.name = "Slate";
        c.start = 10.0;
        c.duration = 1.0;
        c.motion.ensure_defaults();
        e.project().add_clip(base, c);
        if (overlay) e.add_text_clip(overlay, 10.0, 1.0, "ADJUSTED", style);
        const ClipId adj = e.add_adjustment_clip(overlay, 10.0, 1.0);
        if (adj) {
            const int fx = e.add_effect(overlay, adj, "gaussian_blur");
            if (fx >= 0) e.set_effect_param(overlay, adj, fx, "radius", 3.f);
            e.set_clip_grade(overlay, adj, 0.4f, 1.f, 1.4f, 0.f, 0.f, 1.f, 1.f);
        }
        shots.push_back({"06_adjustment", 10.5});
    }

    // --- Diagnostics: isolate one feature per shot so a failure names itself ---
    {
        TextStyle t = style;  // mask only, default fill
        const ClipId c = e.add_text_clip(base, 12.0, 1.0, "MATTE ONLY", t);
        e.add_clip_mask(base, c, 1, 0.12f, 0.18f, 0.76f, 0.64f, 0.06f, false, 0);
        shots.push_back({"07_mask_only", 12.5});
    }
    {
        TextStyle t = style;  // outline only
        t.outline = 6.f;
        t.outline_r = t.outline_g = t.outline_b = 1.f;
        e.add_text_clip(base, 14.0, 1.0, "OUTLINE ONLY", t);
        shots.push_back({"08_outline_only", 14.5});
    }
    {
        TextStyle t = style;  // shadow only
        t.shadow = 10.f;
        t.shadow_alpha = 0.9f;
        e.add_text_clip(base, 16.0, 1.0, "SHADOW ONLY", t);
        shots.push_back({"09_shadow_only", 16.5});
    }
    {
        ClipInfo c;  // grade: shadows only
        c.name = "Slate";
        c.start = 18.0;
        c.duration = 1.0;
        c.motion.ensure_defaults();
        const ClipId id = e.project().add_clip(base, c);
        e.set_clip_grade_tonal(base, id, 0.6f, 0.0f, 0.0f);
        shots.push_back({"10_shadows_only", 18.5});
    }
    {
        ClipInfo c;  // grade: highlights only
        c.name = "Slate";
        c.start = 20.0;
        c.duration = 1.0;
        c.motion.ensure_defaults();
        const ClipId id = e.project().add_clip(base, c);
        e.set_clip_grade_tonal(base, id, 0.0f, -0.4f, 0.0f);
        shots.push_back({"11_highlights_only", 20.5});
    }

    TimelinePlayer player(&e.project());
    if (!player.prepare().ok()) {
        std::printf("ms_shots: prepare failed\n");
        return 1;
    }

    int written = 0;
    for (const auto& s : shots) {
        ImageBuffer frame;
        if (!player.render_frame(s.time, frame).ok()) {
            std::printf("ms_shots: %s failed to render\n", s.name.c_str());
            continue;
        }
        const std::string path = (fs::path(out_dir) / (s.name + ".ppm")).string();
        if (write_ppm(path, frame)) {
            // Mean of the centre third, so a flat slate and a title differ in print.
            double sum[3] = {0, 0, 0};
            long n = 0;
            for (int y = frame.height() / 3; y < frame.height() * 2 / 3; ++y) {
                const uint8_t* row = frame.data() + static_cast<size_t>(y) * frame.stride();
                for (int x = frame.width() / 3; x < frame.width() * 2 / 3; ++x) {
                    for (int c = 0; c < 3; ++c) sum[c] += row[x * 4 + c];
                    ++n;
                }
            }
            std::printf("wrote %s  centre mean %.1f %.1f %.1f\n", path.c_str(),
                        n ? sum[0] / n : 0.0, n ? sum[1] / n : 0.0, n ? sum[2] / n : 0.0);
            ++written;
        }
    }
    std::printf("ms_shots: %d/%zu shot(s) written to %s\n", written, shots.size(),
                out_dir.c_str());
    return written == static_cast<int>(shots.size()) ? 0 : 1;
}
