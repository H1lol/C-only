// Retime + adjustment-layer regression test.
//
// An adjustment layer must reach the pixels the tracks below already produced,
// not add a layer of its own; retime must keep the same slice of source and move
// the clip's timeline length with it.
#include "ms_engine.hpp"
#include "ms_image.hpp"
#include "ms_project.hpp"
#include "ms_text.hpp"
#include "ms_timeline_player.hpp"

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

static double mean_red(const ImageBuffer& img) {
    if (!img.valid()) return 0.0;
    double sum = 0.0;
    size_t n = 0;
    for (int y = 0; y < img.height(); ++y) {
        const uint8_t* row = img.data() + static_cast<size_t>(y) * img.stride();
        for (int x = 0; x < img.width(); ++x) {
            sum += row[x * 4];
            ++n;
        }
    }
    return n ? sum / static_cast<double>(n) : 0.0;
}

int main() {
    Engine& e = Engine::instance();
    if (!e.init(320, 180) || !e.new_project("Adjustment test", 320, 180, 30.f)) {
        std::printf("ms_adjustment_test: engine setup failed\n");
        return 1;
    }
    const auto& tracks = e.project().tracks();
    if (tracks.empty()) {
        std::printf("ms_adjustment_test: default project has no track\n");
        return 1;
    }
    const TrackId track = tracks[0].id;

    ClipInfo c;
    c.name = "Clip A";
    c.start = 0.0;
    c.duration = 2.0;
    c.source_in = 0.0;
    c.source_out = 2.0;
    c.motion.ensure_defaults();
    const ClipId clip = e.project().add_clip(track, c);
    check(clip != 0, "media clip added");

    TimelinePlayer player(&e.project());
    ImageBuffer base;
    check(player.render_frame(0.5, base).ok(), "baseline frame renders");
    const double base_red = mean_red(base);
    check(base_red > 0.0, "baseline frame has pixels");

    // ---- Retime -----------------------------------------------------------
    check(e.set_clip_speed(track, clip, 2.f), "2x speed accepted");
    {
        float s = 0.f;
        check(e.clip_speed(track, clip, &s) && std::fabs(s - 2.f) < 1e-4, "speed read back");
    }
    const ClipInfo* rc = e.project().find_clip(track, clip);
    check(rc && std::fabs(rc->duration - 1.0) < 1e-3, "2x halves the timeline length");
    check(rc && std::fabs(rc->source_out - 2.0) < 1e-3, "2x keeps the same source slice");
    check(e.set_clip_speed(track, clip, 1.f), "1x speed accepted");
    rc = e.project().find_clip(track, clip);
    check(rc && std::fabs(rc->duration - 2.0) < 1e-3, "back to 1x restores the length");

    const ClipId title = e.add_text_clip(track, 4.0, 1.0, "Title", TextStyle{});
    check(title != 0, "title clip added");
    check(!e.set_clip_speed(track, title, 2.f), "a title cannot be retimed");

    // ---- Adjustment layer -------------------------------------------------
    const ClipId adj = e.add_adjustment_clip(track, 0.0, 2.0);
    check(adj != 0, "adjustment layer added");
    const ClipInfo* ac = e.project().find_clip(track, adj);
    check(ac && ac->is_adjustment, "clip is marked as an adjustment layer");
    check(!e.set_clip_speed(track, adj, 2.f), "an adjustment layer cannot be retimed");

    ImageBuffer neutral;
    check(player.render_frame(0.5, neutral).ok(), "frame with a neutral adjustment renders");
    check(std::fabs(mean_red(neutral) - base_red) < 0.5,
          "a neutral adjustment layer does not change the picture");

    check(e.set_clip_grade(track, adj, 1.5f, 1.f, 1.f, 0.f, 0.f, 1.f, 1.f),
          "grade set on the adjustment layer");
    ImageBuffer graded;
    check(player.render_frame(0.5, graded).ok(), "frame through the adjustment renders");
    check(mean_red(graded) > base_red + 1.0,
          "adjustment grade reaches the composite below it");

    ImageBuffer outside;
    check(player.render_frame(2.5, outside).ok(), "frame outside the range renders");
    check(mean_red(outside) < 40.0,
          "the adjustment only applies over the range it covers");

    // ---- Persistence ------------------------------------------------------
    const std::string json = e.project_to_json();
    check(json.find("\"adj\": true") != std::string::npos, "adjustment flag serialized");
    check(e.load_project_json(json), "project with an adjustment reloads");
    bool restored = false;
    for (const auto& t : e.project().tracks()) {
        for (const auto& x : t.clips) {
            if (x.id == adj) restored = x.is_adjustment;
        }
    }
    check(restored, "adjustment survives save/load");

    if (failures == 0) std::printf("ms_adjustment_test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
