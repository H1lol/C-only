#include "ms_transitions.hpp"
#include "ms_edit_ops.hpp"
#include "ms_stick_anim.hpp"
#include "ms_pattern.hpp"
#include "ms_project.hpp"
#include "ms_cpu_effects.hpp"
#include "ms_layer.hpp"
#include "ms_mask.hpp"
#include "ms_node_graph.hpp"
#include <iostream>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Components from donated apps ===\n";

  // Transitions (Kdenlive)
  ImageBuffer a, b, out;
  cpu_solid(a, 64, 36, 1.f, 0.f, 0.f, 1.f);
  cpu_solid(b, 64, 36, 0.f, 0.f, 1.f, 1.f);
  CHECK(apply_transition(a, b, TransitionType::Dissolve, 0.5f, 0.f, out).ok(), "dissolve");
  CHECK(apply_transition(a, b, TransitionType::WipeLeft, 0.4f, 0.05f, out).ok(), "wipe");
  CHECK(apply_transition(a, b, TransitionType::FadeBlack, 0.5f, 0.f, out).ok(), "fade black");

  // Markers + ripple (Kdenlive/OVE)
  ProjectSession s;
  s.new_project("Comp", 320, 180, 24);
  {
    FILE* f = fopen("/tmp/ms_comp.bin", "wb");
    fwrite("ftypisomxxxx", 1, 12, f); fclose(f);
  }
  MediaId m = s.import_media("/tmp/ms_comp.bin");
  TrackId tid = s.tracks()[0].id;
  ClipInfo c1; c1.media_id = m; c1.start = 0; c1.duration = 1; c1.name = "A";
  ClipInfo c2; c2.media_id = m; c2.start = 1; c2.duration = 1; c2.name = "B";
  ClipInfo c3; c3.media_id = m; c3.start = 2; c3.duration = 1; c3.name = "C";
  ClipId id1 = s.add_clip(tid, c1);
  ClipId id2 = s.add_clip(tid, c2);
  ClipId id3 = s.add_clip(tid, c3);
  MarkerTrack markers;
  markers.add(1.0, "Cut");
  CHECK(markers.all().size() == 1, "marker");
  CHECK(ripple_delete_clip(s, tid, id2).ok(), "ripple delete");
  // C should slide left toward 1
  TrackInfo* tr = s.find_track(tid);
  CHECK(tr && tr->clips.size() == 2, "2 clips after ripple");

  // Stick (StykAnimate)
  StickFigure fig = StickFigure::humanoid();
  fig.joints[3].angle.add(0, 0);
  fig.joints[3].angle.add(1, 40);
  ImageBuffer stick;
  CHECK(render_stick_figure(fig, 0.5, 128, 128, stick).ok(), "stick render");
  CHECK(stick.valid(), "stick valid");

  // Pattern (LMMS)
  Pattern pat;
  pat.bpm = 120; pat.steps = 8;
  pat.notes.push_back({0, 60, 0.9f, 1});
  pat.notes.push_back({4, 67, 0.7f, 1});
  AudioBuffer audio;
  CHECK(render_pattern(pat, audio, 48000).ok(), "pattern render");
  CHECK(audio.samples.size() > 1000, "pattern samples");

  // Layers + mask (Natron/comp)
  LayerStack stack;
  Layer L; L.type = LayerType::Solid; L.solid_r = 0.2f; L.solid_g = 0.8f; L.solid_b = 0.3f;
  L.start = 0; L.end = 5; L.opacity = 1.f;
  Mask mk; mk.shape = MaskShape::Ellipse; mk.w = 0.5f; mk.h = 0.5f; mk.x = 0.25f; mk.y = 0.25f;
  L.masks.push_back(mk);
  stack.add_layer(L);
  ImageBuffer comp;
  CHECK(composite_layers(stack, 0.5, 64, 64, comp).ok(), "composite");

  // Nodes (Natron)
  NodeGraph g;
  GraphNode c; c.op = NodeOp::Constant; c.solid_r = 0.5f; c.solid_g = 0.2f; c.solid_b = 0.8f;
  auto ic = g.add(c);
  GraphNode v; v.op = NodeOp::Viewer;
  auto iv = g.add(v);
  g.connect(ic, iv);
  ImageBuffer node_out;
  CHECK(g.evaluate(iv, 0, 32, 32, node_out).ok(), "node graph");

  std::cout << (g_fail ? "\nCOMPONENTS FAILED\n" : "\nCOMPONENTS PASSED\n");
  return g_fail ? 1 : 0;
}
