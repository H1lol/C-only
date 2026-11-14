#pragma once

#include "ms_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace ms {

// ============================================================
// On-Demand Effect Store
// ============================================================

enum class EffectSource : uint8_t {
    Builtin = 0,    // shipped with app
    Downloaded,     // from public repo
    Local           // user-imported
};

struct EffectManifest {
    std::string base_id;
    std::string display_name;
    std::string version;
    std::string category;
    std::string description;
    std::string author;
    std::string license;
    std::string gpu_cost;          // low / medium / high
    std::string min_app_version;
    std::string vert_path;         // relative inside package
    std::string frag_path;
    std::string thumbnail_path;
    std::vector<EffectParam> params;
    std::vector<std::string> tags;
    EffectSource source = EffectSource::Builtin;
    bool         is_downloaded = false;
    std::string  local_path;       // where the package lives on disk
};

struct RemoteEffectInfo {
    std::string base_id;
    std::string display_name;
    std::string version;
    std::string category;
    std::string description;
    std::string download_url;
    std::string thumbnail_url;
    size_t      size_bytes = 0;
    std::string sha256;
    bool        is_installed = false;
};

// Callback for download progress (0.0 – 1.0)
using DownloadProgressFn = std::function<void(const std::string& base_id, float progress)>;

class EffectStore {
public:
    static EffectStore& instance();

    // Catalog
    bool load_remote_catalog(const std::string& json);          // from network
    bool load_local_catalog_cache(const std::string& path);
    bool save_local_catalog_cache(const std::string& path) const;

    const std::vector<RemoteEffectInfo>& remote_effects() const;
    const std::vector<EffectManifest>&   installed_effects() const;

    // Download & install (called after Flutter downloads the bytes)
    // The actual HTTP download happens in Flutter/Dart.
    // C++ only handles verification + registration of the package.
    bool install_package(const std::string& base_id,
                         const std::string& package_dir,   // extracted folder
                         const std::string& expected_sha256 = "");

    bool uninstall(const std::string& base_id);

    // Runtime lookup
    const EffectManifest* find(const std::string& base_id) const;
    bool is_installed(const std::string& base_id) const;

    // Paths
    void set_cache_root(const std::string& path);   // e.g. app documents/effects_cache
    std::string cache_root() const;

private:
    EffectStore();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ms
