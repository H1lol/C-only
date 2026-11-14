#include "ms_keyframe.hpp"
#include "ms_media.hpp"
#include "ms_node_graph.hpp"
#include "ms_scene.hpp"
#include "ms_color_pipeline.hpp"
#include "ms_cpu_effects.hpp"
#include "ms_cpu_effects_extra.hpp"
#include "ms_layer.hpp"
#include <iostream>

int main() {
  using namespace ms;
  int fail = 0;

  // Keyframes
  AnimCurve c;
  c.add(0, 0, CurveInterp::Linear);
  c.add(1, 10, CurveInterp::EaseInOut);
  double v = c.eval(0.5);
  std::cout << "keyframe mid=" << v << "\n";
  if (v <= 0 || v >= 10) { std::cerr << "keyframe fail\n"; ++fail; }

  // Media probe
  auto pr = MediaProbe::probe("/nonexistent/x.mp4");
  SlateDecoder dec;
  // create temp empty file
  {
    FILE* f = fopen("/tmp/ms_slate_test.bin", "wb");
    if (f) { fwrite("ftypisom", 1, 8, f); fclose(f); }
  }
  Error e = dec.open("/tmp/ms_slate_test.bin");
  std::cout << "decoder open: " << (e.ok()?"OK":e.message) << "\n";
  ImageBuffer frame;
  int64_t pts = 0;
  e = dec.read_video(frame, pts);
  std::cout << "slate frame: " << (e.ok()?"OK ":"FAIL ") << frame.width() << "x" << frame.height() << "\n";
  if (!e.ok()) ++fail;

  // Node graph
  NodeGraph g;
  GraphNode read; read.name = "bg"; read.op = NodeOp::Constant;
  read.solid_r = 0.2f; read.solid_g = 0.4f; read.solid_b = 0.8f;
  auto id_r = g.add(read);
  GraphNode blur; blur.name = "blur"; blur.op = NodeOp::Blur;
  blur.params.push_back({"radius", 2.0});
  auto id_b = g.add(blur);
  g.connect(id_r, id_b);
  GraphNode view; view.name = "view"; view.op = NodeOp::Viewer;
  auto id_v = g.add(view);
  g.connect(id_b, id_v);
  ImageBuffer gout;
  e = g.evaluate(id_v, 0.0, 32, 32, gout);
  std::cout << "node graph: " << (e.ok()?"OK":e.message) << " " << gout.size_bytes() << "B\n";
  if (!e.ok()) ++fail;

  // Scene + job json (UE/MoonRay shaped)
  Scene scene;
  SceneObject cam; cam.name = "Cam"; cam.type = SceneObjectType::Camera; cam.focal_mm = 50;
  scene.add(cam);
  SceneObject light; light.name = "Key"; light.type = SceneObjectType::Light; light.intensity = 3;
  scene.add(light);
  scene.render().backend = "moonray";
  scene.render().samples = 128;
  auto job = scene.to_job_json();
  std::cout << "scene job bytes=" << job.size() << "\n";
  if (job.find("moonray") == std::string::npos) ++fail;

  // Color
  ColorPipeline cp;
  if (frame.valid()) {
    e = cp.apply_grade(frame, 0.5f, 1.1f, 0.9f);
    std::cout << "grade: " << (e.ok()?"OK":e.message) << "\n";
  }

  // Chromakey
  ImageBuffer keyed;
  if (frame.valid()) {
    e = cpu_chromakey_simple(frame, keyed, 0.0f, 1.0f, 0.0f, 0.4f);
    std::cout << "chromakey: " << (e.ok()?"OK":e.message) << "\n";
  }

  std::cout << (fail ? "FAIL\n" : "ALL VFX MODULES OK\n");
  return fail ? 1 : 0;
}
