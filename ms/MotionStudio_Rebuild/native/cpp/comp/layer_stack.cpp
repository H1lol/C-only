#include "ms_layer.hpp"
#include "ms_cpu_effects.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ms {

uint64_t LayerStack::add_layer(Layer layer) {
  if (layer.id == 0) layer.id = next_id_++;
  else next_id_ = std::max(next_id_, layer.id + 1);
  layers_.push_back(std::move(layer));
  return layers_.back().id;
}

bool LayerStack::remove_layer(uint64_t id) {
  auto it = std::remove_if(layers_.begin(), layers_.end(),
                           [id](const Layer& L) { return L.id == id; });
  if (it == layers_.end()) return false;
  layers_.erase(it, layers_.end());
  return true;
}

Layer* LayerStack::find(uint64_t id) {
  for (auto& L : layers_) if (L.id == id) return &L;
  return nullptr;
}

const Layer* LayerStack::find(uint64_t id) const {
  for (const auto& L : layers_) if (L.id == id) return &L;
  return nullptr;
}

bool LayerStack::move_layer(uint64_t id, size_t new_index) {
  auto it = std::find_if(layers_.begin(), layers_.end(),
                         [id](const Layer& L) { return L.id == id; });
  if (it == layers_.end()) return false;
  Layer tmp = std::move(*it);
  layers_.erase(it);
  if (new_index > layers_.size()) new_index = layers_.size();
  layers_.insert(layers_.begin() + static_cast<long>(new_index), std::move(tmp));
  return true;
}

std::vector<const Layer*> LayerStack::active_at(double time_sec) const {
  std::vector<const Layer*> out;
  for (const auto& L : layers_) {
    if (!L.visible) continue;
    if (time_sec + 1e-9 < L.start || time_sec > L.end + 1e-9) continue;
    if (L.type == LayerType::Audio) continue;
    out.push_back(&L);
  }
  return out;
}

void blend_pixel(BlendMode mode,
                 float sr, float sg, float sb, float sa,
                 float& dr, float& dg, float& db, float& da) {
  auto clamp01 = [](float x) { return std::clamp(x, 0.f, 1.f); };
  float out_r = sr, out_g = sg, out_b = sb;
  switch (mode) {
    case BlendMode::Normal: break;
    case BlendMode::Multiply:
      out_r = dr * sr; out_g = dg * sg; out_b = db * sb; break;
    case BlendMode::Screen:
      out_r = 1.f - (1.f - dr) * (1.f - sr);
      out_g = 1.f - (1.f - dg) * (1.f - sg);
      out_b = 1.f - (1.f - db) * (1.f - sb); break;
    case BlendMode::Overlay:
      out_r = dr < 0.5f ? 2.f * dr * sr : 1.f - 2.f * (1.f - dr) * (1.f - sr);
      out_g = dg < 0.5f ? 2.f * dg * sg : 1.f - 2.f * (1.f - dg) * (1.f - sg);
      out_b = db < 0.5f ? 2.f * db * sb : 1.f - 2.f * (1.f - db) * (1.f - sb); break;
    case BlendMode::Darken:
      out_r = std::min(dr, sr); out_g = std::min(dg, sg); out_b = std::min(db, sb); break;
    case BlendMode::Lighten:
      out_r = std::max(dr, sr); out_g = std::max(dg, sg); out_b = std::max(db, sb); break;
    default: break;
  }
  const float a = clamp01(sa);
  dr = out_r * a + dr * (1.f - a);
  dg = out_g * a + dg * (1.f - a);
  db = out_b * a + db * (1.f - a);
  da = clamp01(sa + da * (1.f - sa));
}

static void fill_layer_source(const Layer& L, int w, int h, ImageBuffer& buf) {
  if (L.type == LayerType::Solid) {
    cpu_solid(buf, w, h, L.solid_r, L.solid_g, L.solid_b, L.solid_a);
    return;
  }
  if (L.type == LayerType::Adjustment) {
    cpu_solid(buf, w, h, 0.f, 0.f, 0.f, 0.f);
    return;
  }
  cpu_solid(buf, w, h, 0.15f, 0.15f, 0.18f, 1.f);
}

static std::unordered_map<std::string, double> params_to_map(const EffectInstance& fx) {
  std::unordered_map<std::string, double> m;
  for (const auto& p : fx.params) {
    m[p.name] = static_cast<double>(p.value);
  }
  return m;
}

Error composite_layers(const LayerStack& stack,
                       double time_sec,
                       int width, int height,
                       ImageBuffer& output) {
  if (width <= 0 || height <= 0) {
    return Error::make(ErrorCode::InvalidArgument, "composite size");
  }
  if (!output.allocate(width, height, PixelFormat::RGBA8)) {
    return Error::make(ErrorCode::OutOfMemory, "composite output");
  }
  output.clear(0.f, 0.f, 0.f, 0.f);

  auto active = stack.active_at(time_sec);
  ImageBuffer layer_buf;
  ImageBuffer fx_buf;

  for (const Layer* Lp : active) {
    const Layer& L = *Lp;
    fill_layer_source(L, width, height, layer_buf);

    ImageBuffer* cur = &layer_buf;
    for (const auto& fx : L.effects) {
      if (!fx.enabled) continue;
      auto pm = params_to_map(fx);
      Error e = cpu_effect_apply(fx.base_id, *cur, fx_buf, pm);
      if (!e.ok() && e.code != ErrorCode::EffectNotFound) return e;
      layer_buf.copy_from(fx_buf);
      cur = &layer_buf;
    }

    const float op = std::clamp(L.opacity, 0.f, 1.f);
    if (op < 0.999f) {
      cpu_opacity(*cur, fx_buf, op);
      layer_buf.copy_from(fx_buf);
      cur = &layer_buf;
    }

    const uint8_t* s = cur->data();
    uint8_t* d = output.data();
    const int n = width * height;
    for (int i = 0; i < n; ++i) {
      float sr = s[0] / 255.f, sg = s[1] / 255.f, sb = s[2] / 255.f, sa = s[3] / 255.f;
      float dr = d[0] / 255.f, dg = d[1] / 255.f, db = d[2] / 255.f, da = d[3] / 255.f;
      blend_pixel(L.blend, sr, sg, sb, sa, dr, dg, db, da);
      d[0] = static_cast<uint8_t>(std::clamp(dr, 0.f, 1.f) * 255.f + 0.5f);
      d[1] = static_cast<uint8_t>(std::clamp(dg, 0.f, 1.f) * 255.f + 0.5f);
      d[2] = static_cast<uint8_t>(std::clamp(db, 0.f, 1.f) * 255.f + 0.5f);
      d[3] = static_cast<uint8_t>(std::clamp(da, 0.f, 1.f) * 255.f + 0.5f);
      s += 4; d += 4;
    }
  }
  return Error::success();
}

} // namespace ms
