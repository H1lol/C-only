#include "ms_cpu_effects.hpp"
#include "ms_online_catalog.hpp"
#include "ms_transitions.hpp"
#include <iostream>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Offline effects + online catalog ===\n";

  ImageBuffer in, out;
  cpu_solid(in, 32, 32, 0.8f, 0.4f, 0.2f, 1.f);

  CHECK(cpu_grayscale(in, out).ok(), "grayscale");
  CHECK(cpu_sepia(in, out, 0.8f).ok(), "sepia");
  CHECK(cpu_vignette(in, out, 0.7f, 1.0f).ok(), "vignette");
  CHECK(cpu_mirror(in, out, 1).ok(), "mirror");

  std::unordered_map<std::string, double> p;
  CHECK(cpu_effect_apply("grayscale", in, out, p).ok(), "apply grayscale");
  p["amount"] = 1.0;
  CHECK(cpu_effect_apply("sepia", in, out, p).ok(), "apply sepia");

  // little transitions still work
  ImageBuffer b, t;
  cpu_solid(b, 32, 32, 0.1f, 0.2f, 0.9f, 1.f);
  CHECK(apply_transition(in, b, TransitionType::Dissolve, 0.5f, 0.f, t).ok(), "dissolve");
  CHECK(apply_transition(in, b, TransitionType::WipeCircle, 0.4f, 0.05f, t).ok(), "wipe circle");

  auto offline = OnlineEffectsCatalog::offline_builtin_ids();
  CHECK(offline.size() >= 8, "offline builtins");
  std::cout << "  info: offline effects=" << offline.size() << "\n";

  OnlineEffectsCatalog cat;
  CHECK(cat.mode() == EffectsMode::Offline, "default offline");
  std::cout << "  info: catalog URL=" << cat.catalog_url() << "\n";
  CHECK(cat.catalog_url().find("H1lol") != std::string::npos, "points to H1lol");
  CHECK(cat.catalog_url().find("ms-effects") != std::string::npos, "ms-effects repo");

  // load local sample catalog (simulates online payload)
  CHECK(cat.load_from_file(
    "/home/workdir/artifacts/MotionStudio_Rebuild/catalog/catalog.json").ok(),
    "load local catalog");
  CHECK(cat.entries().size() >= 4, "catalog entries");
  std::cout << "  info: catalog entries=" << cat.entries().size() << "\n";

  cat.set_mode(EffectsMode::Online);
  CHECK(cat.is_online(), "online mode");
  // refresh may fail if repo empty — that's OK; we still have local catalog
  auto ref = cat.refresh();
  std::cout << "  info: online refresh " << (ref.ok() ? "ok" : ("skip: " + ref.message)) << "\n";

  std::cout << (g_fail ? "\nEFFECTS MODES FAILED\n" : "\nEFFECTS MODES PASSED\n");
  return g_fail ? 1 : 0;
}
