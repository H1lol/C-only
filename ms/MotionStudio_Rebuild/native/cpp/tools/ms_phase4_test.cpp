#include "ms_node_graph.hpp"
#include "ms_color_pipeline.hpp"
#include "ms_scopes.hpp"
#include "ms_cpu_effects.hpp"
#include <iostream>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Phase 4 — Nodes / color ===\n";

  NodeGraph g;
  GraphNode c1;
  c1.name = "Red"; c1.op = NodeOp::Constant;
  c1.solid_r = 0.9f; c1.solid_g = 0.1f; c1.solid_b = 0.1f;
  uint64_t id_c = g.add(c1);

  GraphNode blur;
  blur.name = "Blur"; blur.op = NodeOp::Blur;
  blur.params.push_back({"radius", 3.0});
  uint64_t id_b = g.add(blur);
  g.connect(id_c, id_b);

  GraphNode grade;
  grade.name = "Grade"; grade.op = NodeOp::Grade;
  grade.params.push_back({"exposure", 0.5});
  grade.params.push_back({"contrast", 1.1});
  uint64_t id_g = g.add(grade);
  g.connect(id_b, id_g);

  GraphNode viewer;
  viewer.name = "Viewer"; viewer.op = NodeOp::Viewer;
  uint64_t id_v = g.add(viewer);
  g.connect(id_g, id_v);

  ImageBuffer out;
  CHECK(g.evaluate(id_v, 0.0, 64, 64, out).ok(), "node graph evaluate");
  CHECK(out.valid() && out.width() == 64, "viewer frame");

  // Merge two constants
  NodeGraph g2;
  GraphNode a, b, m, v;
  a.op = NodeOp::Constant; a.solid_r = 1; a.solid_g = 0; a.solid_b = 0;
  b.op = NodeOp::Constant; b.solid_r = 0; b.solid_g = 0; b.solid_b = 1;
  m.op = NodeOp::Merge; m.params.push_back({"mix", 0.5});
  v.op = NodeOp::Viewer;
  auto ia = g2.add(a), ib = g2.add(b), im = g2.add(m), iv = g2.add(v);
  g2.connect(ia, im, 0, 0);
  g2.connect(ib, im, 0, 1);
  g2.connect(im, iv);
  ImageBuffer merged;
  CHECK(g2.evaluate(iv, 0, 32, 32, merged).ok(), "merge evaluate");
  CHECK(merged.valid(), "merged valid");

  ColorPipeline cp;
  CHECK(cp.spaces().size() >= 4, "color spaces registered");
  ImageBuffer img;
  cpu_solid(img, 32, 32, 0.4f, 0.5f, 0.6f, 1.f);
  CHECK(cp.apply_grade(img, 0.5f, 1.2f, 0.9f).ok(), "apply_grade");

  ScopeHistogram hist;
  CHECK(Scopes::histogram(img, hist).ok(), "histogram");
  int sumy = 0;
  for (int i = 0; i < 256; ++i) sumy += hist.bins_y[i];
  CHECK(sumy == 32 * 32, "histogram pixel count");

  std::vector<WaveformColumn> wf;
  CHECK(Scopes::waveform(img, wf, 16).ok(), "waveform");
  CHECK(wf.size() == 16, "waveform cols");

  std::cout << (g_fail ? "\nPHASE 4 FAILED\n" : "\nPHASE 4 PASSED\n");
  return g_fail ? 1 : 0;
}
