#include "ms_layer.hpp"
#include "ms_cpu_effects.hpp"
#include "ms_msp.hpp"
#include <iostream>

int main() {
  using namespace ms;
  LayerStack stack;

  Layer a;
  a.name = "Red Solid";
  a.type = LayerType::Solid;
  a.solid_r = 0.8f; a.solid_g = 0.1f; a.solid_b = 0.1f; a.solid_a = 1.f;
  a.start = 0; a.end = 10;
  EffectInstance blur;
  blur.base_id = "org.motionstudio.gaussian_blur";
  blur.enabled = true;
  EffectParam radius;
  radius.name = "radius";
  radius.value = 3.f;
  blur.params.push_back(radius);
  a.effects.push_back(blur);
  stack.add_layer(std::move(a));

  Layer b;
  b.name = "Blue";
  b.type = LayerType::Solid;
  b.solid_r = 0.1f; b.solid_g = 0.2f; b.solid_b = 0.9f; b.solid_a = 0.5f;
  b.start = 0; b.end = 10;
  b.blend = BlendMode::Screen;
  b.opacity = 0.7f;
  stack.add_layer(std::move(b));

  ImageBuffer out;
  Error e = composite_layers(stack, 1.0, 64, 64, out);
  if (!e.ok()) {
    std::cerr << "composite failed: " << e.message << "\n";
    return 1;
  }
  std::cout << "composite OK " << out.width() << "x" << out.height()
            << " bytes=" << out.size_bytes() << "\n";

  const char* sample =
      R"({"id":"org.motionstudio.gaussian_blur","name":"Gaussian Blur","version":"1.0.0","entry":{"type":"shader","shader":"shaders/blur.frag"},"open_source": true})";
  MspManifest m;
  e = parse_msp_manifest(sample, m);
  std::cout << "manifest parse: " << (e.ok() ? "OK" : e.message)
            << " id=" << m.id << " name=" << m.name
            << " open_source=" << (m.open_source ? "true" : "false") << "\n";
  return e.ok() ? 0 : 2;
}
