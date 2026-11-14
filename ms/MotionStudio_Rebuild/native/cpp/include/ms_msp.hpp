#pragma once
/**
 * MotionStudio Plugin (.msp) loader
 * Open format — app is open source; plugins recommended open source too.
 */
#include "ms_error.hpp"
#include "ms_image.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace ms {

struct MspParam {
  std::string id;
  std::string label;
  std::string type;   // float | int | bool | color | choice
  double default_value = 0.0;
  double min_value = 0.0;
  double max_value = 1.0;
  std::vector<std::string> choices;
};

struct MspManifest {
  int msp_version = 1;
  std::string id;
  std::string name;
  std::string version;
  std::string author;
  std::string license;
  std::string license_url;
  std::string source_url;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
  std::string min_app_version;
  std::string entry_type;      // shader | wasm | native | composite
  std::string entry_path;
  std::vector<MspParam> params;
  bool open_source = false;
  std::string install_dir;     // filled after extract
};

class MspPlugin {
public:
  virtual ~MspPlugin() = default;
  virtual const MspManifest& manifest() const = 0;
  virtual Error apply(const ImageBuffer& in, ImageBuffer& out,
                      const std::unordered_map<std::string, double>& params,
                      float time_sec) = 0;
};

using MspFactory = std::function<std::unique_ptr<MspPlugin>()>;

class MspRegistry {
public:
  static MspRegistry& instance();

  /** Scan <plugins_root> for extracted plugins (folders with manifest.json) */
  Error scan_installed(const std::string& plugins_root);

  /** Install a .msp archive (zip) into plugins_root/<id>/ */
  Error install_msp(const std::string& msp_path, const std::string& plugins_root);

  /** Uninstall by id */
  Error uninstall(const std::string& id, const std::string& plugins_root);

  std::vector<MspManifest> list() const;
  std::shared_ptr<MspPlugin> get(const std::string& id) const;

  void register_builtin(const std::string& id, MspFactory factory);

private:
  MspRegistry() = default;
  std::unordered_map<std::string, MspManifest> manifests_;
  std::unordered_map<std::string, MspFactory> factories_;
  std::unordered_map<std::string, std::shared_ptr<MspPlugin>> loaded_;
};

/** Parse manifest.json text into MspManifest */
Error parse_msp_manifest(const std::string& json, MspManifest& out);

} // namespace ms
