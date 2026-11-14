#pragma once
/**
 * Phase 3 — Layer stack (compositing)
 * Uses BlendMode / Transform2D / EffectInstance from ms_types.hpp
 */
#include "ms_error.hpp"
#include "ms_image.hpp"
#include "ms_types.hpp"
#include "ms_mask.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace ms {

enum class LayerType : uint8_t {
  Video = 0,
  Image,
  Solid,
  Adjustment,
  Audio,
  Text,
  Shape,
};

struct Layer {
  uint64_t id = 0;
  std::string name;
  LayerType type = LayerType::Video;
  bool visible = true;
  bool locked = false;
  BlendMode blend = BlendMode::Normal;
  Transform2D transform;
  float opacity = 1.f;  // 0..1 extra to transform
  std::vector<EffectInstance> effects;

  double start = 0.0;
  double end = 10.0;
  double in_point = 0.0;
  double out_point = 10.0;

  std::string media_ref;
  float solid_r = 0.f, solid_g = 0.f, solid_b = 0.f, solid_a = 1.f;
  std::vector<Mask> masks;
  std::string text; // for Text layers
  float shape_radius = 0.5f; // for Shape ellipse-ish
};


class LayerStack {
public:
  uint64_t add_layer(Layer layer);
  bool remove_layer(uint64_t id);
  Layer* find(uint64_t id);
  const Layer* find(uint64_t id) const;
  std::vector<Layer>& layers() { return layers_; }
  const std::vector<Layer>& layers() const { return layers_; }
  bool move_layer(uint64_t id, size_t new_index);
  std::vector<const Layer*> active_at(double time_sec) const;

private:
  std::vector<Layer> layers_;
  uint64_t next_id_ = 1;
};

Error composite_layers(const LayerStack& stack,
                       double time_sec,
                       int width, int height,
                       ImageBuffer& output);

void blend_pixel(BlendMode mode,
                 float sr, float sg, float sb, float sa,
                 float& dr, float& dg, float& db, float& da);

} // namespace ms
