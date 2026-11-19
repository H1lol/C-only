/**
 * Tonal split (shadows / highlights / vibrance) regression test.
 *
 * The point of these controls is that they are not contrast: each has to move
 * one end of the range and leave the other alone, and vibrance has to favour
 * muted colours over already-saturated ones.
 */
#include "ms_finishing.hpp"
#include "ms_engine.hpp"

#include <cmath>
#include <cstdio>
#include <string>

static int g_failures = 0;

static void check(bool ok, const char* what) {
  if (ok) {
    std::printf("ok   %s\n", what);
  } else {
    std::printf("FAIL %s\n", what);
    ++g_failures;
  }
}

/** A single solid pixel value, graded, as 0..255 on the red channel. */
static int graded(float value, float shadows, float highlights, float vibrance) {
  ms::ImageBuffer img;
  img.allocate(1, 1);
  img.clear(value, value, value, 1.f);
  ms::GradeSettings g;
  g.shadows = shadows;
  g.highlights = highlights;
  g.vibrance = vibrance;
  if (!ms::apply_finishing_grade(img, g).ok()) return -1;
  return img.data()[0];
}

/** A solid colour, graded, as its red channel after a vibrance move. */
static int graded_red(float r, float gv, float b, float vibrance) {
  ms::ImageBuffer img;
  img.allocate(1, 1);
  img.clear(r, gv, b, 1.f);
  ms::GradeSettings g;
  g.vibrance = vibrance;
  if (!ms::apply_finishing_grade(img, g).ok()) return -1;
  return img.data()[0];
}

int main() {
  // Neutral grade is a no-op (to the byte).
  auto raw = [](float v) -> int {
    ms::ImageBuffer img;
    img.allocate(1, 1);
    img.clear(v, v, v, 1.f);
    return img.data()[0];
  };
  check(std::abs(graded(0.25f, 0.f, 0.f, 0.f) - raw(0.25f)) <= 1,
        "neutral grade leaves a dark pixel alone");
  check(std::abs(graded(0.75f, 0.f, 0.f, 0.f) - raw(0.75f)) <= 1,
        "neutral grade leaves a bright pixel alone");

  // Shadows move the dark end and barely touch the bright end.
  const int dark_up = graded(0.2f, 0.5f, 0.f, 0.f);
  const int bright_same = graded(0.9f, 0.5f, 0.f, 0.f);
  check(dark_up > graded(0.2f, 0.f, 0.f, 0.f), "positive shadows lift a dark pixel");
  check(dark_up - graded(0.2f, 0.f, 0.f, 0.f) > 20, "the shadow lift is visible");
  check(std::abs(bright_same - graded(0.9f, 0.f, 0.f, 0.f)) < 4,
        "shadows leave a bright pixel almost untouched");

  const int dark_down = graded(0.2f, -0.5f, 0.f, 0.f);
  check(dark_down < graded(0.2f, 0.f, 0.f, 0.f), "negative shadows lower a dark pixel");

  // Highlights move the bright end and barely touch the dark end.
  const int bright_down = graded(0.9f, 0.f, -0.5f, 0.f);
  const int dark_same = graded(0.2f, 0.f, -0.5f, 0.f);
  check(bright_down < graded(0.9f, 0.f, 0.f, 0.f), "negative highlights pull a bright pixel down");
  check(graded(0.9f, 0.f, 0.f, 0.f) - bright_down > 20, "the highlight move is visible");
  check(std::abs(dark_same - graded(0.2f, 0.f, 0.f, 0.f)) < 4,
        "highlights leave a dark pixel almost untouched");

  // Midtones are the pivot, so a shadow move should barely register there.
  const int mid_neutral = graded(0.5f, 0.f, 0.f, 0.f);
  const int mid_shadowed = graded(0.5f, 0.5f, 0.f, 0.f);
  check(mid_shadowed - mid_neutral < 15, "shadows do not wash out the midtones");

  // Vibrance boosts a muted colour more than a saturated one.
  const int muted_boost = graded_red(0.55f, 0.5f, 0.45f, 0.8f) - graded_red(0.55f, 0.5f, 0.45f, 0.f);
  const int vivid_boost = graded_red(1.f, 0.f, 0.f, 0.8f) - graded_red(1.f, 0.f, 0.f, 0.f);
  check(muted_boost > 0, "positive vibrance enriches a muted colour");
  check(vivid_boost < muted_boost, "vibrance moves a muted colour more than a saturated one");

  // A grey pixel has no saturation to push, so vibrance must leave it alone.
  check(std::abs(graded_red(0.5f, 0.5f, 0.5f, 0.9f) - graded_red(0.5f, 0.5f, 0.5f, 0.f)) <= 1,
        "vibrance leaves a neutral pixel alone");

  // Engine plumbing: the tonal slots survive the project round trip.
  {
    ms::Engine& e = ms::Engine::instance();
    check(e.init(320, 180), "engine init");
    check(e.new_project("grade test", 320, 180, 24.f), "new project");
    const ms::TrackId track = e.add_track("V1", 0);
    ms::ClipInfo c;
    c.name = "clip";
    c.start = 0.0;
    c.duration = 2.0;
    const ms::ClipId clip = e.project().add_clip(track, c);
    check(clip > 0, "clip added");

    check(e.set_clip_grade_tonal(track, clip, 0.3f, -0.2f, 0.6f), "clip tonal set");
    float g[10] = {};
    check(e.clip_grade(track, clip, g), "clip grade readable");
    check(std::fabs(g[7] - 0.3f) < 1e-4f, "clip shadows");
    check(std::fabs(g[8] + 0.2f) < 1e-4f, "clip highlights");
    check(std::fabs(g[9] - 0.6f) < 1e-4f, "clip vibrance");
    check(std::fabs(g[1] - 1.f) < 1e-4f, "base contrast untouched by a tonal move");

    e.set_grade(0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 1.f);
    e.set_grade_tonal(0.1f, 0.2f, 0.3f);
    float m[10] = {};
    check(e.get_grade(m), "master grade readable");
    check(std::fabs(m[7] - 0.1f) < 1e-4f && std::fabs(m[9] - 0.3f) < 1e-4f,
          "master tonal values stored");

    const std::string json = e.project_to_json();
    check(e.load_project_json(json), "reload with a grade");
    float m2[10] = {};
    check(e.get_grade(m2), "master grade readable after reload");
    check(std::fabs(m2[7] - 0.1f) < 1e-3f && std::fabs(m2[9] - 0.3f) < 1e-3f,
          "master tonal values survive the round trip");
    float g2[10] = {};
    check(e.clip_grade(track, clip, g2), "clip grade readable after reload");
    check(std::fabs(g2[7] - 0.3f) < 1e-3f && std::fabs(g2[8] + 0.2f) < 1e-3f,
          "clip tonal values survive the round trip");
  }

  if (g_failures == 0) {
    std::printf("all grade checks passed\n");
    return 0;
  }
  std::printf("%d grade check(s) failed\n", g_failures);
  return 1;
}
