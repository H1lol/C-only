// Regression test: opening a saved project must leave the engine ready to
// render. load_project_json() used to skip setting `initialized`, and
// render_frame() returns false while that flag is clear, so loading a project
// (which is what the Android app does from the starter screen) produced a
// permanently black preview.
#include "../bridge/ms_bridge.h"
#include "../include/ms_engine.hpp"

#include <cstdio>
#include <cstring>

static int failures = 0;

static void check(bool cond, const char* what) {
  std::printf("%s %s\n", cond ? "ok  " : "FAIL", what);
  if (!cond) ++failures;
}

int main() {
  auto& engine = ms::Engine::instance();

  // Build a small project, then shut down so the engine starts cold.
  check(ms_project_new("Load Test", 640, 360, 30.0) == 0, "create project");
  char* json = ms_project_to_json();
  check(json != nullptr, "serialise project");
  std::string saved = json ? json : "{}";
  std::free(json);

  ms_engine_shutdown();
  check(!engine.is_initialized(), "engine is cold after shutdown");

  // This is the path the app takes when a user taps an existing project.
  check(ms_project_load(saved.c_str()) == 0, "load project from JSON");
  check(engine.is_initialized(), "engine is initialized after load");
  check(engine.render_frame(), "render_frame works after load");

  // Size must come from the file, not from a previous project.
  const auto& meta = engine.project().meta();
  check(meta.width == 640 && meta.height == 360,
        "frame size restored from the loaded project");

  // ---- comp network round-trips through the project document --------------
  check(ms_comp_add_node(static_cast<int>(ms::NodeOp::Blur), 60.f, 40.f) > 0, "comp node added");
  const int before = ms_comp_node_count();
  check(before >= 1, "comp has a node");
  ms_comp_set_active(1);
  check(ms_comp_active() == 1, "comp activates");
  char* with_comp = ms_project_to_json();
  check(with_comp && std::string(with_comp).find("\"comp\":\"MSCOMP1") != std::string::npos,
        "project document carries the comp");
  std::string doc = with_comp ? with_comp : "{}";
  std::free(with_comp);

  ms_engine_shutdown();
  check(ms_project_load(doc.c_str()) == 0, "project with a comp loads");
  check(ms_comp_node_count() == before, "comp nodes are restored on load");

  std::printf("\n%s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
  return failures == 0 ? 0 : 1;
}
