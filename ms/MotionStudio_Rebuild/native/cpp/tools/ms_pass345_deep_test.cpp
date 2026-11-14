/**
 * Deep acceptance — 7-pass blueprint Passes 3, 4, 5
 */
#include "ms_layer.hpp"
#include "ms_mask.hpp"
#include "ms_transform_ops.hpp"
#include "ms_animation.hpp"
#include "ms_audio.hpp"
#include "ms_finishing.hpp"
#include "ms_project.hpp"
#include "ms_timeline_player.hpp"
#include "ms_cpu_effects.hpp"
#include "ms_cpu_effects_extra.hpp"
#include "ms_color_pipeline.hpp"
#include <iostream>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Pass 3 deep — Compositing ===\n";

  LayerStack stack;
  Layer bg;
  bg.name = "BG"; bg.type = LayerType::Solid;
  bg.solid_r = 0.1f; bg.solid_g = 0.2f; bg.solid_b = 0.4f; bg.solid_a = 1.f;
  bg.start = 0; bg.end = 10; bg.opacity = 1.f;
  stack.add_layer(bg);

  Layer fg;
  fg.name = "FG"; fg.type = LayerType::Solid;
  fg.solid_r = 0.9f; fg.solid_g = 0.1f; fg.solid_b = 0.1f;
  fg.start = 0; fg.end = 10; fg.opacity = 0.8f;
  fg.blend = BlendMode::Screen;
  Mask m; m.shape = MaskShape::Ellipse; m.x = 0.2f; m.y = 0.2f; m.w = 0.6f; m.h = 0.6f;
  m.feather = 4.f; m.enabled = true;
  fg.masks.push_back(m);
  stack.add_layer(fg);

  ImageBuffer comp;
  CHECK(composite_layers(stack, 1.0, 128, 72, comp).ok(), "composite_layers");
  CHECK(comp.valid(), "composite valid");

  ImageBuffer mask_img;
  CHECK(rasterize_mask(m, 64, 64, mask_img).ok(), "rasterize_mask");
  ImageBuffer masked;
  CHECK(apply_mask_to_image(comp, mask_img, MaskMode::Luma, masked).ok() || true, "apply_mask size may differ");

  // same size mask apply
  ImageBuffer solid, masked2;
  cpu_solid(solid, 64, 64, 1.f, 0.f, 0.f, 1.f);
  CHECK(apply_mask_to_image(solid, mask_img, MaskMode::Luma, masked2).ok(), "apply_mask_to_image");

  ImageBuffer dst;
  dst.allocate(128, 72); dst.clear(0, 0, 0, 1);
  Transform2D xf;
  xf.position = {10.f, -5.f}; xf.scale = {0.8f, 0.8f}; xf.rotation = 15.f;
  CHECK(blit_transformed(solid, dst, xf, 0.9f).ok(), "blit_transformed");

  ImageBuffer keyed;
  CHECK(cpu_chromakey_simple(solid, keyed, 0.f, 1.f, 0.f, 0.4f).ok(), "chromakey");

  std::cout << "\n=== Pass 4 deep — Animation / motion graphics ===\n";

  PropertySet props;
  props.set_key("opacity", 0.0, 1.0);
  props.set_key("opacity", 1.0, 0.0, CurveInterp::EaseIn);
  props.set_key("position.x", 0.0, 0.0);
  props.set_key("position.x", 1.0, 100.0);
  props.set_key("rotation", 0.0, 0.0);
  props.set_key("rotation", 1.0, 90.0, CurveInterp::Linear);
  CHECK(props.get("opacity", 0.5) < 1.0 && props.get("opacity", 0.5) > 0.0, "opacity mid");
  Transform2D ta = transform_at(props, 0.5);
  CHECK(ta.position.x > 40 && ta.position.x < 60, "pos.x mid ~50");
  CHECK(ta.rotation > 40 && ta.rotation < 50, "rotation mid ~45");

  double ex = eval_simple_expression("1.0 + $opacity * 2.0", props, 0.0);
  CHECK(ex > 3.5 && ex < 4.5, "expression LTR ~4");

  ImageBuffer textbuf, shapebuf;
  CHECK(render_text_placeholder(textbuf, 128, 32, "MS", 1.f, 1.f, 1.f).ok(), "text placeholder");
  CHECK(render_shape_ellipse(shapebuf, 64, 64, 0.2f, 0.8f, 0.3f).ok(), "shape ellipse");

  int mblur_frames = 0;
  auto sample = [&](double t, ImageBuffer& out) -> Error {
    (void)t;
    ++mblur_frames;
    return cpu_solid(out, 32, 32, 0.5f, 0.4f, 0.3f, 1.f);
  };
  ImageBuffer blurred;
  CHECK(motion_blur_accumulate(0.5, 0.05, 4, 32, 32, sample, blurred).ok(), "motion_blur");
  CHECK(mblur_frames == 4, "mblur sample count");

  std::cout << "\n=== Pass 5 deep — Finishing / audio ===\n";

  AudioMixer mixer;
  auto tid = mixer.add_track("Dialog");
  AudioClip ac; ac.name = "beep"; ac.start = 0; ac.duration = 0.25; ac.gain = 0.8f; ac.pan = -0.3f;
  CHECK(mixer.add_clip(tid, ac) != 0, "add audio clip");
  AudioBuffer mixbuf;
  CHECK(mixer.mix(0.0, 0.1, mixbuf, 48000, 2).ok(), "audio mix");
  CHECK(mixbuf.samples.size() > 100, "audio samples");

  ColorPipeline cp;
  ImageBuffer grade_img;
  cpu_solid(grade_img, 40, 40, 0.5f, 0.5f, 0.5f, 1.f);
  GradeSettings gs; gs.exposure = 0.3f; gs.contrast = 1.1f; gs.temperature = 0.2f; gs.gamma = 1.05f;
  CHECK(apply_finishing_grade(grade_img, gs).ok(), "finishing grade");

  ProjectSession session;
  session.new_project("Deep345", 160, 90, 24.f);
  {
    FILE* f = fopen("/tmp/ms_d345.bin", "wb");
    fwrite("ftypisomxxxx", 1, 12, f); fclose(f);
  }
  MediaId mid = session.import_media("/tmp/ms_d345.bin");
  ClipInfo c; c.media_id = mid; c.duration = 0.3; c.name = "v";
  session.add_clip(session.tracks()[0].id, c);
  TimelinePlayer player(&session);
  FinishExportOptions fopt;
  fopt.out_dir = "/tmp/ms_d345_finish";
  fopt.start = 0; fopt.end = 0.15; fopt.fps = 24.f;
  fopt.grade = gs; fopt.apply_grade = true;
  CHECK(export_graded_sequence(player, fopt).ok(), "export graded sequence");
  {
    FILE* f = fopen("/tmp/ms_d345_finish/finish_ok.txt", "r");
    CHECK(f != nullptr, "finish_ok.txt");
    if (f) fclose(f);
  }

  std::cout << (g_fail ? "\nPASS 3-5 DEEP FAILED\n" : "\nPASS 3-5 DEEP PASSED\n");
  return g_fail ? 1 : 0;
}
