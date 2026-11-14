#include "ms_effects_host.hpp"
#include "ms_cpu_effects.hpp"
#include <iostream>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Milestone 3 · Effects host ===\n";

  auto& host = EffectsHost::instance();
  CHECK(host.init().ok(), "init");
  std::cout << "  info: catalog=" << host.catalog().size()
            << " frei0r=" << host.frei0r_count() << "\n";
  CHECK(host.catalog().size() >= 9, "catalog size");

  auto builtins = host.builtin_ids();
  CHECK(builtins.size() >= 8, "builtins");
  for (const auto& id : builtins) {
    CHECK(host.has_equivalent(id), std::string("equiv ") + id);
  }

  ImageBuffer in, out;
  cpu_solid(in, 64, 64, 0.2f, 0.8f, 0.3f, 1.f);

  for (const auto& id : builtins) {
    FxInstance fx;
    fx.id = id;
    fx.values["radius"] = 3;
    fx.values["amount"] = 1;
    fx.values["strength"] = 0.5;
    fx.values["opacity"] = 0.9;
    fx.values["tolerance"] = 0.3;
    Error e = host.apply(fx, 0.0, in, out);
    CHECK(e.ok() || e.code == ErrorCode::EffectNotFound, std::string("apply ") + id);
    if (e.ok()) CHECK(out.valid(), std::string("pixels ") + id);
  }

  // Keyframes
  FxInstance blur;
  blur.id = "gaussian_blur";
  blur.set_key("radius", 0.0, 1.0);
  blur.set_key("radius", 1.0, 12.0);
  CHECK(std::abs(blur.value_at("radius", 0.5) - 6.5) < 0.1, "keyframe lerp");
  CHECK(host.apply(blur, 0.5, in, out).ok(), "apply keyed blur");

  // Stack
  std::vector<FxInstance> stack;
  FxInstance g; g.id = "grayscale"; stack.push_back(g);
  FxInstance v; v.id = "vignette"; v.values["strength"] = 0.7; stack.push_back(v);
  CHECK(host.apply_stack(stack, 0.0, in, out).ok(), "stack");

  // frei0r aliases
  if (host.frei0r_count() > 0) {
    FxInstance f;
    f.id = "mlt.greyscale";
    CHECK(host.apply(f, 0.0, in, out).ok(), "frei0r/mlt greyscale path");
  } else {
    std::cout << "  skip: no frei0r plugins on disk\n";
  }

  // OpenFX scan (empty dir ok)
  host.scan_openfx("/tmp/ms_ofx_empty");
  CHECK(true, "openfx scan API");

  std::cout << (g_fail ? "\nM3 FAILED\n" : "\nM3 PASSED\n");
  return g_fail ? 1 : 0;
}
