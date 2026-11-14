#include "ms_msp.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace ms {
namespace {

std::string read_file(const fs::path& path) {
  std::ifstream in(path);
  if (!in) return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string json_string_field(const std::string& json, const std::string& key) {
  const std::string pat = "\"" + key + "\"";
  auto pos = json.find(pat);
  if (pos == std::string::npos) return {};
  pos = json.find(':', pos);
  if (pos == std::string::npos) return {};
  // skip whitespace
  while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) ++pos;
  if (pos >= json.size()) return {};
  if (json[pos] == '"') {
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return json.substr(pos + 1, end - pos - 1);
  }
  // number or bool — read until comma/brace
  auto end = pos;
  while (end < json.size() && std::string(",}\n\r ").find(json[end]) == std::string::npos) ++end;
  return json.substr(pos, end - pos);
}

bool json_bool_field(const std::string& json, const std::string& key, bool def = false) {
  auto s = json_string_field(json, key);
  if (s == "true" || s == "1") return true;
  if (s == "false" || s == "0") return false;
  // also raw without quotes
  const std::string pat = "\"" + key + "\"";
  auto pos = json.find(pat);
  if (pos == std::string::npos) return def;
  pos = json.find(':', pos);
  if (pos == std::string::npos) return def;
  auto sub = json.substr(pos, 20);
  if (sub.find("true") != std::string::npos) return true;
  if (sub.find("false") != std::string::npos) return false;
  return def;
}

} // namespace

MspRegistry& MspRegistry::instance() {
  static MspRegistry r;
  return r;
}

Error parse_msp_manifest(const std::string& json, MspManifest& out) {
  out = MspManifest{};
  out.msp_version = 1;
  out.id = json_string_field(json, "id");
  out.name = json_string_field(json, "name");
  out.version = json_string_field(json, "version");
  out.author = json_string_field(json, "author");
  out.license = json_string_field(json, "license");
  out.license_url = json_string_field(json, "license_url");
  out.source_url = json_string_field(json, "source_url");
  out.description = json_string_field(json, "description");
  out.category = json_string_field(json, "category");
  out.min_app_version = json_string_field(json, "min_app_version");
  out.open_source = json_bool_field(json, "open_source", false);

  // entry block — find "entry" then type / shader
  auto entry_pos = json.find("\"entry\"");
  if (entry_pos != std::string::npos) {
    auto slice = json.substr(entry_pos, 400);
    out.entry_type = json_string_field(slice, "type");
    out.entry_path = json_string_field(slice, "shader");
    if (out.entry_path.empty()) out.entry_path = json_string_field(slice, "wasm");
    if (out.entry_path.empty()) out.entry_path = json_string_field(slice, "path");
  }

  if (out.id.empty() || out.name.empty()) {
    return Error::make(ErrorCode::FileCorrupt, "msp manifest missing id or name");
  }
  return Error::success();
}

Error MspRegistry::scan_installed(const std::string& plugins_root) {
  std::error_code ec;
  if (!fs::exists(plugins_root, ec)) {
    return Error::make(ErrorCode::FileNotFound, "plugins root missing", plugins_root);
  }
  for (const auto& ent : fs::directory_iterator(plugins_root, ec)) {
    if (!ent.is_directory()) continue;
    const fs::path man = ent.path() / "manifest.json";
    if (!fs::exists(man)) continue;
    const std::string json = read_file(man);
    MspManifest m;
    Error err = parse_msp_manifest(json, m);
    if (!err.ok()) continue;
    m.install_dir = ent.path().string();
    manifests_[m.id] = m;
  }
  return Error::success();
}

Error MspRegistry::install_msp(const std::string& msp_path, const std::string& plugins_root) {
  std::error_code ec;
  if (!fs::exists(msp_path, ec)) {
    return Error::make(ErrorCode::FileNotFound, "msp file not found", msp_path);
  }
  fs::create_directories(plugins_root, ec);

  // Extract via system unzip (portable enough for desktop; mobile: Flutter extracts)
  // First unzip to a temp dir, read id from manifest, then move to plugins_root/id
  const fs::path tmp = fs::temp_directory_path() / ("ms_msp_" + std::to_string(std::hash<std::string>{}(msp_path)));
  fs::remove_all(tmp, ec);
  fs::create_directories(tmp, ec);

  std::string cmd = "unzip -o -q \"" + msp_path + "\" -d \"" + tmp.string() + "\"";
  int rc = std::system(cmd.c_str());
  if (rc != 0) {
    // try bsdtar
    cmd = "bsdtar -xf \"" + msp_path + "\" -C \"" + tmp.string() + "\"";
    rc = std::system(cmd.c_str());
  }
  if (rc != 0) {
    return Error::make(ErrorCode::EffectLoadFailed, "failed to unzip msp", msp_path);
  }

  const fs::path man = tmp / "manifest.json";
  if (!fs::exists(man)) {
    fs::remove_all(tmp, ec);
    return Error::make(ErrorCode::FileCorrupt, "msp missing manifest.json");
  }
  MspManifest m;
  Error err = parse_msp_manifest(read_file(man), m);
  if (!err.ok()) {
    fs::remove_all(tmp, ec);
    return err;
  }

  const fs::path dest = fs::path(plugins_root) / m.id;
  fs::remove_all(dest, ec);
  fs::rename(tmp, dest, ec);
  if (ec) {
    // cross-device rename fail → copy
    fs::create_directories(dest, ec);
    fs::copy(tmp, dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    fs::remove_all(tmp, ec);
  }
  m.install_dir = dest.string();
  manifests_[m.id] = m;
  return Error::success();
}

Error MspRegistry::uninstall(const std::string& id, const std::string& plugins_root) {
  manifests_.erase(id);
  loaded_.erase(id);
  std::error_code ec;
  fs::remove_all(fs::path(plugins_root) / id, ec);
  if (ec) return Error::make(ErrorCode::FilePermission, "uninstall failed", id);
  return Error::success();
}

std::vector<MspManifest> MspRegistry::list() const {
  std::vector<MspManifest> v;
  v.reserve(manifests_.size());
  for (const auto& kv : manifests_) v.push_back(kv.second);
  std::sort(v.begin(), v.end(), [](const MspManifest& a, const MspManifest& b) {
    return a.name < b.name;
  });
  return v;
}

std::shared_ptr<MspPlugin> MspRegistry::get(const std::string& id) const {
  auto it = loaded_.find(id);
  if (it != loaded_.end()) return it->second;
  return nullptr;
}

void MspRegistry::register_builtin(const std::string& id, MspFactory factory) {
  factories_[id] = std::move(factory);
}

} // namespace ms
