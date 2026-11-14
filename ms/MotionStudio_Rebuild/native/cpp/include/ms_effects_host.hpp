#pragma once
/**
 * Milestone 3 — Effects host: builtins + frei0r (via MLT) + OpenFX stub + keyframes
 */
#include "ms_error.hpp"
#include "ms_image.hpp"
#include "ms_keyframe.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace ms {

enum class EffectBackend : uint8_t {
  BuiltinCpu = 0,
  Frei0rMlt = 1,
  OpenFX = 2,
};

struct EffectParamDef {
  std::string name;
  double min_v = 0;
  double max_v = 1;
  double default_v = 0;
};

struct EffectDescriptor {
  std::string id;          // e.g. gaussian_blur, frei0r.sobel
  std::string name;
  std::string category;
  EffectBackend backend = EffectBackend::BuiltinCpu;
  std::string mlt_service; // for Frei0rMlt: "frei0r.sobel" or "brightness"
  std::vector<EffectParamDef> params;
};

/** One effect instance on a clip/layer with keyframed params */
struct FxInstance {
  std::string id;
  bool enabled = true;
  /** param name → keyframe track (time_sec → value) */
  std::unordered_map<std::string, std::vector<AnimKeyframe>> keys;
  /** static fallback when no keys */
  std::unordered_map<std::string, double> values;

  double value_at(const std::string& param, double time_sec) const;
  void set_key(const std::string& param, double time_sec, double value);
};

class EffectsHost {
public:
  static EffectsHost& instance();

  /** Register builtins + scan frei0r via MLT factory */
  Error init();
  const std::vector<EffectDescriptor>& catalog() const { return catalog_; }
  const EffectDescriptor* find(const std::string& id) const;

  /** Every MS builtin has an equivalent entry (G3) */
  std::vector<std::string> builtin_ids() const;
  bool has_equivalent(const std::string& builtin_id) const;

  /** Apply one effect to RGBA buffer at time */
  Error apply(const FxInstance& fx, double time_sec,
              const ImageBuffer& in, ImageBuffer& out);

  /** Apply ordered stack */
  Error apply_stack(const std::vector<FxInstance>& stack, double time_sec,
                    const ImageBuffer& in, ImageBuffer& out);

  /** OpenFX: list discovered plugin paths (stub until ofx host linked) */
  std::vector<std::string> openfx_plugins() const { return ofx_plugins_; }
  Error scan_openfx(const std::string& dir);

  int frei0r_count() const { return frei0r_count_; }

private:
  EffectsHost() = default;
  std::vector<EffectDescriptor> catalog_;
  std::vector<std::string> ofx_plugins_;
  int frei0r_count_ = 0;
  bool ready_ = false;

  void register_builtins();
  void scan_frei0r_mlt();
  Error apply_builtin(const FxInstance& fx, double t,
                      const ImageBuffer& in, ImageBuffer& out);
  Error apply_frei0r_mlt(const FxInstance& fx, double t,
                         const ImageBuffer& in, ImageBuffer& out);
};

} // namespace ms
