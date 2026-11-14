#pragma once
/**
 * Online effects catalog — points at H1lol GitHub repos
 * Offline = built-in CPU effects only
 * Online  = fetch catalog.json + download .msp on demand
 */
#include "ms_error.hpp"
#include "ms_msp.hpp"
#include <string>
#include <vector>

namespace ms {

enum class EffectsMode : uint8_t { Offline = 0, Online = 1 };

struct OnlineEffectEntry {
  std::string id;
  std::string name;
  std::string category;
  std::string msp_file;     // relative path in repo
  std::string download_url; // full URL if present
  bool open_source = true;
  bool installed = false;
};

struct CatalogConfig {
  /** Owner/repo for effects catalog (your account) */
  std::string github_owner = "H1lol";
  std::string github_repo = "ms-effects";
  std::string branch = "main";
  std::string catalog_path = "catalog.json";
  /** Fallback secondary repo */
  std::string fallback_repo = "Source-codes";
  std::string local_cache_dir = "plugins/online";
};

class OnlineEffectsCatalog {
public:
  explicit OnlineEffectsCatalog(CatalogConfig cfg = {});

  void set_mode(EffectsMode m) { mode_ = m; }
  EffectsMode mode() const { return mode_; }
  bool is_online() const { return mode_ == EffectsMode::Online; }

  const CatalogConfig& config() const { return cfg_; }
  void set_config(const CatalogConfig& c) { cfg_ = c; }

  /** Build raw.githubusercontent.com URL for catalog */
  std::string catalog_url() const;
  std::string effect_url(const OnlineEffectEntry& e) const;

  /**
   * Fetch catalog over network (curl). Online mode only.
   * On failure reports Error and keeps last cache if any.
   */
  Error refresh();

  /** Load from local JSON file (offline cache or shipped) */
  Error load_from_file(const std::string& path);

  const std::vector<OnlineEffectEntry>& entries() const { return entries_; }

  /** Download one effect .msp into cache dir and register with MspRegistry */
  Error download_effect(const std::string& id);

  /** List ids available online (or cached) */
  std::vector<std::string> list_ids() const;

  /** Builtin offline effect ids always available */
  static std::vector<std::string> offline_builtin_ids();

private:
  CatalogConfig cfg_;
  EffectsMode mode_ = EffectsMode::Offline;
  std::vector<OnlineEffectEntry> entries_;
  Error parse_catalog_json(const std::string& json);
  static std::string http_get(const std::string& url, int& exit_code);
  static Error http_download(const std::string& url, const std::string& out_path);
};

} // namespace ms
