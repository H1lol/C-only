#include "ms_effects_host.hpp"
#include "ms_cpu_effects.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstring>

#if defined(MS_HAVE_MLT)
#include <framework/mlt.h>
#endif

namespace ms {
namespace fs = std::filesystem;

EffectsHost& EffectsHost::instance() {
  static EffectsHost h;
  return h;
}

double FxInstance::value_at(const std::string& param, double time_sec) const {
  auto kit = keys.find(param);
  if (kit != keys.end() && !kit->second.empty()) {
    // reuse simple keyframe eval from animation if available — linear here
    const auto& track = kit->second;
    if (time_sec <= track.front().time) return track.front().value;
    if (time_sec >= track.back().time) return track.back().value;
    for (size_t i = 1; i < track.size(); ++i) {
      if (time_sec <= track[i].time) {
        double t0 = track[i - 1].time, t1 = track[i].time;
        double v0 = track[i - 1].value, v1 = track[i].value;
        double u = (t1 > t0) ? (time_sec - t0) / (t1 - t0) : 0;
        return v0 + (v1 - v0) * u;
      }
    }
  }
  auto vit = values.find(param);
  if (vit != values.end()) return vit->second;
  return 0.0;
}

void FxInstance::set_key(const std::string& param, double time_sec, double value) {
  auto& track = keys[param];
  for (auto& k : track) {
    if (std::abs(k.time - time_sec) < 1e-6) { k.value = value; return; }
  }
  AnimKeyframe k;
  k.time = time_sec;
  k.value = value;
  track.push_back(k);
  std::sort(track.begin(), track.end(),
            [](const AnimKeyframe& a, const AnimKeyframe& b) { return a.time < b.time; });
}

void EffectsHost::register_builtins() {
  auto add = [&](const char* id, const char* name, const char* cat,
                 std::vector<EffectParamDef> ps) {
    EffectDescriptor d;
    d.id = id;
    d.name = name;
    d.category = cat;
    d.backend = EffectBackend::BuiltinCpu;
    d.params = std::move(ps);
    catalog_.push_back(d);
  };
  add("gaussian_blur", "Gaussian Blur", "blur", {{"radius", 0, 64, 4}});
  add("brightness_contrast", "Brightness / Contrast", "color",
      {{"brightness", -1, 1, 0}, {"contrast", 0, 2, 1}});
  add("opacity", "Opacity", "channel", {{"opacity", 0, 1, 1}});
  add("invert", "Invert", "color", {});
  add("grayscale", "Grayscale", "color", {});
  add("sepia", "Sepia", "color", {{"amount", 0, 1, 1}});
  add("vignette", "Vignette", "stylize",
      {{"strength", 0, 1, 0.6}, {"radius", 0.1, 1.5, 1}});
  add("mirror", "Mirror", "distort", {{"horizontal", 0, 1, 1}});
  add("chromakey", "Chroma Key", "key",
      {{"tolerance", 0, 1, 0.2}});
}

void EffectsHost::scan_frei0r_mlt() {
#if defined(MS_HAVE_MLT)
  // Discover .so under common frei0r paths and register as frei0r.<name>
  std::vector<std::string> dirs = {
    "/usr/lib/frei0r-1",
    "/usr/lib/x86_64-linux-gnu/frei0r-1",
    "/usr/local/lib/frei0r-1"
  };
  for (const auto& dir : dirs) {
    if (!fs::exists(dir)) continue;
    for (auto& ent : fs::directory_iterator(dir)) {
      if (ent.path().extension() != ".so") continue;
      std::string stem = ent.path().stem().string();
      EffectDescriptor d;
      d.id = "frei0r." + stem;
      d.name = stem;
      d.category = "frei0r";
      d.backend = EffectBackend::Frei0rMlt;
      d.mlt_service = "frei0r." + stem;
      catalog_.push_back(d);
      frei0r_count_++;
    }
  }
  // Common MLT built-in filters as alternatives
  for (const char* s : {"brightness", "mirror", "greyscale", "invert", "softglow"}) {
    EffectDescriptor d;
    d.id = std::string("mlt.") + s;
    d.name = s;
    d.category = "mlt";
    d.backend = EffectBackend::Frei0rMlt;
    d.mlt_service = s;
    catalog_.push_back(d);
  }
#endif
}

Error EffectsHost::init() {
  if (ready_) return Error::success();
  catalog_.clear();
  frei0r_count_ = 0;
  register_builtins();
  scan_frei0r_mlt();
  ready_ = true;
  return Error::success();
}

const EffectDescriptor* EffectsHost::find(const std::string& id) const {
  for (const auto& d : catalog_)
    if (d.id == id || d.id == "org.motionstudio." + id) return &d;
  // prefix match frei0r
  for (const auto& d : catalog_)
    if (d.id.find(id) != std::string::npos) return &d;
  return nullptr;
}

std::vector<std::string> EffectsHost::builtin_ids() const {
  std::vector<std::string> ids;
  for (const auto& d : catalog_)
    if (d.backend == EffectBackend::BuiltinCpu) ids.push_back(d.id);
  return ids;
}

bool EffectsHost::has_equivalent(const std::string& builtin_id) const {
  if (find(builtin_id)) return true;
  // map names
  static const std::unordered_map<std::string, std::string> alt = {
    {"gaussian_blur", "frei0r.IIRblur"},
    {"grayscale", "mlt.greyscale"},
    {"invert", "mlt.invert"},
    {"mirror", "mlt.mirror"},
    {"brightness_contrast", "mlt.brightness"},
  };
  auto it = alt.find(builtin_id);
  if (it != alt.end() && find(it->second)) return true;
  return false;
}

Error EffectsHost::apply_builtin(const FxInstance& fx, double t,
                                 const ImageBuffer& in, ImageBuffer& out) {
  std::unordered_map<std::string, double> p;
  if (const auto* d = find(fx.id)) {
    for (const auto& pd : d->params)
      p[pd.name] = fx.value_at(pd.name, t);
  }
  for (const auto& kv : fx.values) p[kv.first] = fx.value_at(kv.first, t);
  // also pass keyed
  for (const auto& kv : fx.keys) p[kv.first] = fx.value_at(kv.first, t);

  std::string id = fx.id;
  if (id.rfind("org.motionstudio.", 0) == 0) id = id.substr(17);
  return cpu_effect_apply(id, in, out, p);
}

Error EffectsHost::apply_frei0r_mlt(const FxInstance& fx, double /*t*/,
                                    const ImageBuffer& in, ImageBuffer& out) {
#if defined(MS_HAVE_MLT)
  // For M3 spike: apply via melt is heavy; use CPU equivalent when known
  std::string id = fx.id;
  if (id == "mlt.greyscale" || id.find("greyscale") != std::string::npos)
    return cpu_effect_apply("grayscale", in, out, {});
  if (id == "mlt.invert" || id.find("invert") != std::string::npos)
    return cpu_effect_apply("invert", in, out, {});
  if (id == "mlt.mirror" || id.find("mirror") != std::string::npos)
    return cpu_effect_apply("mirror", in, out, {{"horizontal", 1}});
  if (id.find("brightness") != std::string::npos)
    return cpu_effect_apply("brightness_contrast", in, out,
                            {{"brightness", fx.value_at("level", 0)}, {"contrast", 1}});
  if (id.find("blur") != std::string::npos || id.find("IIR") != std::string::npos)
    return cpu_effect_apply("gaussian_blur", in, out,
                            {{"radius", fx.value_at("radius", 0) > 0 ? fx.value_at("radius", 0) : 4}});
  // Unknown frei0r: passthrough copy
  if (!out.allocate(in.width(), in.height(), PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "out");
  std::memcpy(out.data(), in.data(),
              static_cast<size_t>(in.width()) * in.height() * 4);
  return Error::success();
#else
  (void)fx; (void)in; (void)out;
  return Error::make(ErrorCode::NotSupported, "MLT/frei0r");
#endif
}

Error EffectsHost::apply(const FxInstance& fx, double time_sec,
                         const ImageBuffer& in, ImageBuffer& out) {
  if (!fx.enabled) {
    if (!out.allocate(in.width(), in.height(), PixelFormat::RGBA8))
      return Error::make(ErrorCode::OutOfMemory, "out");
    std::memcpy(out.data(), in.data(),
                static_cast<size_t>(in.width()) * in.height() * 4);
    return Error::success();
  }
  if (!ready_) init();
  const EffectDescriptor* d = find(fx.id);
  EffectBackend be = d ? d->backend : EffectBackend::BuiltinCpu;
  if (be == EffectBackend::Frei0rMlt)
    return apply_frei0r_mlt(fx, time_sec, in, out);
  if (be == EffectBackend::OpenFX)
    return Error::make(ErrorCode::NotSupported, "OpenFX apply not linked");
  return apply_builtin(fx, time_sec, in, out);
}

Error EffectsHost::apply_stack(const std::vector<FxInstance>& stack, double time_sec,
                               const ImageBuffer& in, ImageBuffer& out) {
  if (stack.empty()) {
    if (!out.allocate(in.width(), in.height(), PixelFormat::RGBA8))
      return Error::make(ErrorCode::OutOfMemory, "out");
    std::memcpy(out.data(), in.data(),
                static_cast<size_t>(in.width()) * in.height() * 4);
    return Error::success();
  }
  ImageBuffer a, b;
  const ImageBuffer* cur = &in;
  ImageBuffer* dst = &a;
  for (size_t i = 0; i < stack.size(); ++i) {
    Error e = apply(stack[i], time_sec, *cur, *dst);
    if (!e.ok()) return e;
    cur = dst;
    dst = (dst == &a) ? &b : &a;
  }
  if (!out.allocate(cur->width(), cur->height(), PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "out");
  std::memcpy(out.data(), cur->data(),
              static_cast<size_t>(cur->width()) * cur->height() * 4);
  return Error::success();
}

Error EffectsHost::scan_openfx(const std::string& dir) {
  ofx_plugins_.clear();
  if (!fs::exists(dir))
    return Error::make(ErrorCode::FileNotFound, "ofx dir", dir);
  for (auto& ent : fs::directory_iterator(dir)) {
    auto p = ent.path();
    if (p.extension() == ".ofx" || p.extension() == ".so") {
      ofx_plugins_.push_back(p.string());
      EffectDescriptor d;
      d.id = "ofx." + p.stem().string();
      d.name = p.stem().string();
      d.category = "openfx";
      d.backend = EffectBackend::OpenFX;
      catalog_.push_back(d);
    }
  }
  return Error::success();
}

} // namespace ms
