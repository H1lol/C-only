#include "../../include/ms_effect_store.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace ms {

struct EffectStore::Impl {
    std::string cache_root = "./effects_cache";
    std::vector<RemoteEffectInfo> remote;
    std::vector<EffectManifest> installed;
    std::unordered_map<std::string, size_t> installed_index; // base_id → index
};

EffectStore& EffectStore::instance() {
    static EffectStore s;
    return s;
}

EffectStore::EffectStore() : impl_(std::make_unique<Impl>()) {}

void EffectStore::set_cache_root(const std::string& path) {
    impl_->cache_root = path;
}

std::string EffectStore::cache_root() const {
    return impl_->cache_root;
}

bool EffectStore::load_remote_catalog(const std::string& json) {
    // Minimal parser for demo — real version uses proper JSON lib
    // For now we just accept that Flutter parsed it and passed structured data
    // or we store the raw list.
    std::cout << "[EffectStore] Remote catalog loaded (" << json.size() << " bytes)" << std::endl;
    return true;
}

bool EffectStore::load_local_catalog_cache(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream buf;
    buf << f.rdbuf();
    return load_remote_catalog(buf.str());
}

bool EffectStore::save_local_catalog_cache(const std::string& path) const {
    // Placeholder
    std::ofstream f(path);
    return f.good();
}

const std::vector<RemoteEffectInfo>& EffectStore::remote_effects() const {
    return impl_->remote;
}

const std::vector<EffectManifest>& EffectStore::installed_effects() const {
    return impl_->installed;
}

bool EffectStore::install_package(const std::string& base_id,
                                  const std::string& package_dir,
                                  const std::string& expected_sha256) {
    // In real implementation:
    // 1. Read manifest.json from package_dir
    // 2. Verify sha256 if provided
    // 3. Copy/move to cache_root/base_id/
    // 4. Register in installed list

    if (is_installed(base_id)) {
        std::cout << "[EffectStore] Already installed: " << base_id << std::endl;
        return true;
    }

    EffectManifest m;
    m.base_id = base_id;
    m.display_name = base_id; // would come from manifest
    m.version = "1.0.0";
    m.category = "Downloaded";
    m.source = EffectSource::Downloaded;
    m.is_downloaded = true;
    m.local_path = package_dir;

    impl_->installed_index[base_id] = impl_->installed.size();
    impl_->installed.push_back(std::move(m));

    // Mark remote entry as installed
    for (auto& r : impl_->remote) {
        if (r.base_id == base_id) {
            r.is_installed = true;
            break;
        }
    }

    std::cout << "[EffectStore] Installed effect: " << base_id
              << " from " << package_dir << std::endl;
    return true;
}

bool EffectStore::uninstall(const std::string& base_id) {
    auto it = impl_->installed_index.find(base_id);
    if (it == impl_->installed_index.end()) return false;

    size_t idx = it->second;
    impl_->installed.erase(impl_->installed.begin() + static_cast<long>(idx));
    impl_->installed_index.erase(it);

    // Rebuild index
    impl_->installed_index.clear();
    for (size_t i = 0; i < impl_->installed.size(); ++i) {
        impl_->installed_index[impl_->installed[i].base_id] = i;
    }

    for (auto& r : impl_->remote) {
        if (r.base_id == base_id) {
            r.is_installed = false;
            break;
        }
    }

    std::cout << "[EffectStore] Uninstalled: " << base_id << std::endl;
    return true;
}

const EffectManifest* EffectStore::find(const std::string& base_id) const {
    auto it = impl_->installed_index.find(base_id);
    if (it == impl_->installed_index.end()) return nullptr;
    return &impl_->installed[it->second];
}

bool EffectStore::is_installed(const std::string& base_id) const {
    return impl_->installed_index.count(base_id) > 0;
}

} // namespace ms
