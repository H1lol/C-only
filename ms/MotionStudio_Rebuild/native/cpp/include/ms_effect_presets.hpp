#pragma once
#include "ms_clip_motion.hpp"
#include <string>
#include <vector>
namespace ms {
struct EffectPreset { std::string id,name,category,description,author,license; std::vector<EffectOp> effects; std::vector<std::string> tags; };
class EffectPresetLibrary {
public:
  void add(EffectPreset p); bool remove(const std::string& id); const EffectPreset* find(const std::string& id) const;
  std::vector<EffectPreset> search(const std::string& query, const std::string& category="") const;
  bool save(const std::string& path) const; bool load(const std::string& path);
  const std::vector<EffectPreset>& all() const { return presets_; }
  static EffectPresetLibrary builtins();
private: std::vector<EffectPreset> presets_;
};
}
