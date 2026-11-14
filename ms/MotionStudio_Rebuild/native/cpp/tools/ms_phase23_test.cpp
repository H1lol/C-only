#include "ms_project.hpp"
#include "ms_timeline_player.hpp"
#include "ms_clip_motion.hpp"
#include "ms_cpu_effects.hpp"
#include "ms_keyframe.hpp"
#include "ms_media.hpp"
#include <iostream>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Phase 2 — Media pipeline ===\n";

  ProjectSession s;
  CHECK(s.new_project("P23", 320, 180, 24.f).ok(), "new_project");

  {
    FILE* f = fopen("/tmp/ms_p23.bin", "wb");
    fwrite("ftypisomxxxx", 1, 12, f);
    fclose(f);
  }
  MediaId mid = s.import_media("/tmp/ms_p23.bin");
  CHECK(mid != 0, "import_media");

  TrackId tid = s.tracks()[0].id;
  ClipInfo c;
  c.media_id = mid; c.name = "A"; c.start = 0; c.duration = 1.0;
  c.source_in = 0; c.source_out = 1.0; c.opacity = 1.f;
  ClipId cid = s.add_clip(tid, c);
  CHECK(cid != 0, "add_clip");

  TimelinePlayer player(&s);
  CHECK(player.prepare().ok(), "player.prepare");

  ImageBuffer frame;
  CHECK(player.render_frame(0.0, frame).ok(), "render_frame t=0");
  CHECK(frame.valid() && frame.width() == 320 && frame.height() == 180, "frame size 320x180");

  CHECK(player.scrub(0.5, frame).ok(), "scrub 0.5");

  SequenceExporter exp;
  SequenceExporter::Options opt;
  opt.out_dir = "/tmp/ms_p23_export";
  opt.start = 0;
  opt.end = 0.25; // ~6 frames at 24fps
  opt.fps = 24.f;
  CHECK(exp.export_png_sequence(player, opt).ok(), "export sequence");
  {
    FILE* f = fopen("/tmp/ms_p23_export/export_ok.txt", "r");
    CHECK(f != nullptr, "export_ok.txt exists");
    if (f) fclose(f);
  }

  std::cout << "\n=== Phase 3 — Motion / effects ===\n";

  AnimCurve curve;
  curve.add(0.0, 0.0, CurveInterp::Linear);
  curve.add(1.0, 10.0, CurveInterp::Linear);
  double midv = curve.eval(0.5);
  CHECK(midv > 4.5 && midv < 5.5, "keyframe mid ~5");

  ClipMotion motion;
  motion.opacity.add(0.0, 1.0);
  motion.opacity.add(1.0, 0.0, CurveInterp::EaseIn);
  CHECK(motion.opacity_at(0.0) > 0.9, "opacity start");
  CHECK(motion.opacity_at(1.0) < 0.1, "opacity end");

  ImageBuffer solid, blurred, inverted;
  CHECK(cpu_solid(solid, 64, 64, 0.8f, 0.2f, 0.1f, 1.f).ok(), "cpu_solid");
  CHECK(cpu_gaussian_blur(solid, blurred, 3.f).ok(), "gaussian_blur");
  CHECK(blurred.valid(), "blurred valid");

  std::vector<EffectOp> stack;
  EffectOp e1; e1.id = "gaussian_blur"; e1.params["radius"] = 2.0;
  EffectOp e2; e2.id = "invert";
  stack.push_back(e1);
  stack.push_back(e2);
  ImageBuffer stacked;
  CHECK(EffectStack::apply(stack, solid, stacked).ok() || stacked.valid(), "effect stack");
  CHECK(stacked.valid(), "stacked valid");

  // brightness via apply
  std::unordered_map<std::string, double> p;
  p["brightness"] = 0.1; p["contrast"] = 1.2;
  ImageBuffer bc;
  CHECK(cpu_effect_apply("brightness_contrast", solid, bc, p).ok(), "brightness_contrast");

  std::cout << (g_fail ? "\nPHASE 2+3 FAILED\n" : "\nPHASE 2+3 PASSED\n");
  return g_fail ? 1 : 0;
}
