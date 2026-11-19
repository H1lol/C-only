/**
 * Multi-mask regression test.
 *
 * Covers the three things that used to be wrong or missing: the matte strength
 * reaching alpha (so MaskMode::Alpha is not a no-op), several masks combining as
 * union-minus-subtract, and the mask list surviving a project round trip.
 */
#include "ms_mask.hpp"
#include "ms_project.hpp"
#include "ms_engine.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;

static void check(bool ok, const char* what) {
  if (ok) {
    std::printf("ok   %s\n", what);
  } else {
    std::printf("FAIL %s\n", what);
    ++g_failures;
  }
}

static int alpha_at(const ms::ImageBuffer& img, int x, int y) {
  return img.data()[static_cast<size_t>(y) * img.stride() + x * 4 + 3];
}

int main() {
  // 1. One rect mask writes its strength into alpha, and the in-place matte
  //    applies it. Before this, the strength lived only in RGB while the apply
  //    step read alpha, so an Alpha mask did nothing at all.
  {
    ms::Mask m;
    m.shape = ms::MaskShape::Rect;
    m.x = 0.25f; m.y = 0.25f; m.w = 0.5f; m.h = 0.5f;
    ms::ImageBuffer matte;
    check(ms::rasterize_mask(m, 64, 64, matte).ok(), "single rect rasterises");
    check(alpha_at(matte, 32, 32) > 250, "rect centre opaque in alpha");
    check(alpha_at(matte, 1, 1) < 5, "rect outside clear in alpha");

    ms::ImageBuffer frame;
    frame.allocate(64, 64);
    frame.clear(1.f, 1.f, 1.f, 1.f);
    check(ms::apply_mask_inplace(frame, matte, ms::MaskMode::Alpha).ok(), "in-place matte");
    check(alpha_at(frame, 32, 32) > 250, "centre survives the matte");
    check(alpha_at(frame, 1, 1) < 5, "outside is matted away");
  }

  // 2. Two Add masks union, and the gap between them stays clear.
  {
    ms::Mask a;
    a.shape = ms::MaskShape::Rect;
    a.x = 0.05f; a.y = 0.05f; a.w = 0.3f; a.h = 0.3f;
    ms::Mask b = a;
    b.x = 0.6f; b.y = 0.6f;
    ms::ImageBuffer matte;
    check(ms::rasterize_masks({a, b}, 100, 100, matte).ok(), "two adds rasterise");
    check(alpha_at(matte, 20, 20) > 250, "first add visible");
    check(alpha_at(matte, 80, 80) > 250, "second add visible");
    check(alpha_at(matte, 50, 50) < 5, "gap between adds stays clear");
  }

  // 3. Add + Subtract: the subtract mask punches a hole in the additive union.
  {
    ms::Mask add;
    add.shape = ms::MaskShape::Rect;
    add.x = 0.1f; add.y = 0.1f; add.w = 0.8f; add.h = 0.8f;
    ms::Mask cut;
    cut.shape = ms::MaskShape::Ellipse;
    cut.op = ms::MaskOp::Subtract;
    cut.x = 0.35f; cut.y = 0.35f; cut.w = 0.3f; cut.h = 0.3f;
    ms::ImageBuffer matte;
    check(ms::rasterize_masks({add, cut}, 100, 100, matte).ok(), "add + subtract rasterise");
    check(alpha_at(matte, 50, 50) < 5, "subtract hole is clear");
    check(alpha_at(matte, 50, 15) > 250, "add area away from the hole survives");
  }

  // 4. A lone Subtract mask starts from a full frame instead of hiding the clip.
  {
    ms::Mask cut;
    cut.shape = ms::MaskShape::Rect;
    cut.op = ms::MaskOp::Subtract;
    cut.x = 0.4f; cut.y = 0.4f; cut.w = 0.2f; cut.h = 0.2f;
    ms::ImageBuffer matte;
    check(ms::rasterize_masks({cut}, 100, 100, matte).ok(), "lone subtract rasterises");
    check(alpha_at(matte, 50, 50) < 5, "lone subtract hole is clear");
    check(alpha_at(matte, 5, 5) > 250, "rest of the frame stays visible");
  }

  // 5. Disabled masks are skipped; an all-disabled list leaves the frame alone.
  {
    ms::Mask m;
    m.shape = ms::MaskShape::Rect;
    m.enabled = false;
    ms::ImageBuffer matte;
    check(ms::rasterize_masks({m}, 32, 32, matte).ok(), "disabled list rasterises");
    check(alpha_at(matte, 16, 16) > 250, "disabled mask leaves the frame alone");
  }

  // 6. Feathering ramps down across the edge instead of stepping.
  {
    ms::Mask m;
    m.shape = ms::MaskShape::Rect;
    m.x = 0.25f; m.y = 0.25f; m.w = 0.5f; m.h = 0.5f;
    m.feather = 20.f;
    ms::ImageBuffer matte;
    check(ms::rasterize_mask(m, 100, 100, matte).ok(), "feathered rect rasterises");
    const int inside = alpha_at(matte, 50, 50);
    const int edge = alpha_at(matte, 24, 50);
    // The rect starts at x = 25px, so x = 1 is 24px outside: past a 20px feather.
    const int outside = alpha_at(matte, 1, 50);
    check(inside > 250, "feathered centre opaque");
    check(edge > 5 && edge < 250, "feather edge is partial");
    check(outside < 5, "feather falls off to clear");
  }

  // 7. Engine list API: add, read back, remove, and refuse to grow past the cap.
  {
    ms::Engine& e = ms::Engine::instance();
    check(e.init(320, 180), "engine init");
    check(e.new_project("mask test", 320, 180, 24.f), "new project");
    const ms::TrackId track = e.add_track("V1", 0);
    ms::ClipInfo c;
    c.name = "clip";
    c.start = 0.0;
    c.duration = 2.0;
    const ms::ClipId clip = e.project().add_clip(track, c);
    check(clip > 0, "clip added");

    check(e.add_clip_mask(track, clip, 0, 0.2f, 0.2f, 0.5f, 0.5f, 4.f, false, 0) == 1,
          "first mask added");
    check(e.add_clip_mask(track, clip, 1, 0.3f, 0.3f, 0.2f, 0.2f, 0.f, true, 1) == 2,
          "second mask added");
    check(e.clip_mask_count(track, clip) == 2, "mask count is two");

    float v[6] = {};
    bool inv = false;
    int op = 0;
    check(e.clip_mask_at(track, clip, 1, v, &inv, &op), "second mask readable");
    check(op == 1 && inv, "second mask keeps op and invert");
    check(std::fabs(v[3] - 0.2f) < 1e-4f, "second mask width");
    check(std::fabs(v[5] - 0.f) < 1e-4f, "second mask feather");

    // The legacy single-mask read still reports the first mask.
    check(e.clip_mask(track, clip, v, &inv), "legacy read reports the first mask");
    check(std::fabs(v[3] - 0.5f) < 1e-4f, "legacy read is the first mask");

    const std::string json = e.project_to_json();
    check(json.find("\"masks\": [") != std::string::npos, "mask list serialized");
    check(e.load_project_json(json), "reload with masks");
    check(e.clip_mask_count(track, clip) == 2, "both masks restored");
    check(e.clip_mask_at(track, clip, 1, v, &inv, &op) && op == 1, "subtract op restored");
    check(std::fabs(v[1] - 0.3f) < 1e-3f, "mask geometry restored after reload");

    check(e.remove_clip_mask_at(track, clip, 0), "first mask removed");
    check(e.clip_mask_count(track, clip) == 1, "one mask left");
    check(e.clip_mask_at(track, clip, 0, v, &inv, &op) && op == 1, "survivor is the subtract mask");
    check(e.remove_clip_mask_at(track, clip, 0), "last mask removed");
    check(e.clip_mask_count(track, clip) == 0, "no masks left");
    check(!e.clip_mask(track, clip, v, &inv), "legacy read reports no mask");

    for (size_t i = 0; i < ms::kMaxClipMasks; ++i)
      e.add_clip_mask(track, clip, 0, 0.1f, 0.1f, 0.2f, 0.2f, 0.f, false, 0);
    check(e.clip_mask_count(track, clip) == static_cast<int>(ms::kMaxClipMasks),
          "clip reaches the mask cap");
    check(e.add_clip_mask(track, clip, 0, 0.1f, 0.1f, 0.2f, 0.2f, 0.f, false, 0) == -1,
          "adding past the cap is refused");
  }

  if (g_failures == 0) {
    std::printf("all mask checks passed\n");
    return 0;
  }
  std::printf("%d mask check(s) failed\n", g_failures);
  return 1;
}
