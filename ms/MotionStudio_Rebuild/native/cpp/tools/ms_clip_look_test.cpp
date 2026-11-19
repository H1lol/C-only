/**
 * Per-clip LUT look regression test.
 *
 * A look used to be display-wide only: picking one graded every frame in the
 * sequence and there was no way to match a single shot. This covers the per-clip
 * path end to end through the real renderer: the look reaches the pixels, it
 * stays inside its own clip, its strength blends, it survives a project round
 * trip, a missing file is refused instead of silently grading nothing, and
 * clearing it restores the ungraded clip.
 */
#include "ms_engine.hpp"
#include "ms_image.hpp"
#include "ms_project.hpp"
#include "ms_timeline_player.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace ms;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

/**
 * A cube that maps every input to one constant colour, so the result of applying
 * it is exact and does not depend on trilinear sampling.
 */
static bool write_constant_cube(const std::string& path, float r, float g, float b, int size = 5) {
    std::ofstream out(path);
    if (!out) return false;
    out << "LUT_3D_SIZE " << size << "\n";
    for (int i = 0; i < size * size * size; ++i) out << r << " " << g << " " << b << "\n";
    return static_cast<bool>(out);
}

static int channel_at(const ImageBuffer& img, int x, int y, int c) {
    return img.data()[static_cast<size_t>(y) * img.stride() + x * 4 + c];
}

int main() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "ms_clip_look_test";
    std::error_code ec;
    fs::create_directories(dir, ec);
    const std::string green_cube = (dir / "constant_green.cube").string();
    if (!write_constant_cube(green_cube, 0.f, 1.f, 0.f)) {
        std::printf("ms_clip_look_test: cannot write the test cube\n");
        return 1;
    }

    Engine& e = Engine::instance();
    if (!e.init(160, 90) || !e.new_project("Clip look test", 160, 90, 30.f)) {
        std::printf("ms_clip_look_test: engine setup failed\n");
        return 1;
    }
    const auto& tracks = e.project().tracks();
    if (tracks.empty()) {
        std::printf("ms_clip_look_test: default project has no track\n");
        return 1;
    }
    const TrackId track = tracks[0].id;

    ClipInfo a;
    a.name = "A";
    a.start = 0.0;
    a.duration = 2.0;
    a.source_in = 0.0;
    a.source_out = 2.0;
    a.motion.ensure_defaults();
    const ClipId clip_a = e.project().add_clip(track, a);

    ClipInfo b = a;
    b.name = "B";
    b.start = 2.0;
    b.duration = 2.0;
    const ClipId clip_b = e.project().add_clip(track, b);

    if (clip_a <= 0 || clip_b <= 0) {
        std::printf("ms_clip_look_test: cannot add clips\n");
        return 1;
    }

    TimelinePlayer player(&e.project());
    ImageBuffer frame;

    // 1. Baseline: no decoder means the player's slate fill, so the pixel value
    //    is known and any change is the look's doing.
    check(player.render_frame(0.5, frame).ok(), "baseline frame renders");
    const int base_r = channel_at(frame, 80, 45, 0);
    const int base_g = channel_at(frame, 80, 45, 1);
    check(std::abs(base_r - 76) <= 3, "baseline slate red");

    // 2. The look reaches clip A's pixels.
    check(e.set_clip_look(track, clip_a, green_cube, 1.f), "clip look accepted");
    check(player.render_frame(0.5, frame).ok(), "clip A renders with the look");
    check(channel_at(frame, 80, 45, 0) < 10, "look removes red from its own clip");
    check(channel_at(frame, 80, 45, 1) > 240, "look pushes green to full");

    // 3. It stays inside clip A: clip B on the same track is untouched.
    check(player.render_frame(2.5, frame).ok(), "clip B renders");
    check(std::abs(channel_at(frame, 80, 45, 0) - base_r) <= 3, "the look does not leak to clip B");
    check(std::abs(channel_at(frame, 80, 45, 1) - base_g) <= 3, "clip B green is untouched");

    // 4. Strength blends the graded result against the original.
    check(e.set_clip_look_strength(track, clip_a, 0.5f), "clip look strength applied");
    check(player.render_frame(0.5, frame).ok(), "half-strength frame renders");
    const int half_r = channel_at(frame, 80, 45, 0);
    const int half_g = channel_at(frame, 80, 45, 1);
    check(half_r > 25 && half_r < 50, "half strength sits between graded and original (red)");
    check(half_g > 110 && half_g < 160, "half strength sits between graded and original (green)");

    // 5. The getter reports what was set, and only for the clip that has one.
    std::string path;
    float strength = 0.f;
    check(e.clip_look(track, clip_a, &path, &strength), "clip A reports a look");
    check(path == green_cube, "clip look path round-trips");
    check(std::fabs(strength - 0.5f) < 1e-4f, "clip look strength reports");
    check(!e.clip_look(track, clip_b, &path, &strength), "clip B reports no look");

    // 6. A missing file is refused and does not disturb the existing look.
    check(!e.set_clip_look(track, clip_a, (dir / "missing.cube").string(), 1.f),
          "a missing cube is refused");
    check(e.clip_look(track, clip_a, &path, &strength) && path == green_cube,
          "a refused look leaves the previous one alone");

    // 7. Save and reload: the look is part of the document.
    const std::string json = e.project_to_json();
    check(json.find("\"look_path\"") != std::string::npos, "look path serialized");
    check(json.find("\"look_strength\"") != std::string::npos, "look strength serialized");
    check(e.load_project_json(json), "reload with a clip look");
    float loaded_strength = 0.f;
    std::string loaded_path;
    check(e.clip_look(track, clip_a, &loaded_path, &loaded_strength), "clip look survives reload");
    check(loaded_path == green_cube, "reloaded look path matches");
    check(std::fabs(loaded_strength - 0.5f) < 1e-4f, "reloaded look strength matches");
    check(!e.clip_look(track, clip_b, &loaded_path, &loaded_strength),
          "reload does not invent a look for clip B");

    // 8. Clearing goes back to the ungraded clip.
    check(e.clear_clip_look(track, clip_a), "clip look cleared");
    check(!e.clip_look(track, clip_a, &path, &strength), "cleared clip reports no look");
    check(player.render_frame(0.5, frame).ok(), "cleared clip renders");
    check(std::abs(channel_at(frame, 80, 45, 0) - base_r) <= 3, "cleared clip is back to the slate");
    check(e.set_clip_look_strength(track, clip_a, 1.f) == false,
          "strength cannot be set on a clip with no look");

    if (failures == 0) {
        std::printf("all clip look checks passed\n");
        return 0;
    }
    std::printf("%d clip look check(s) failed\n", failures);
    return 1;
}
