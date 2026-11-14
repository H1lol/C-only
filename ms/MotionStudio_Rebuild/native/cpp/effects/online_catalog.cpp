#include "ms_online_catalog.hpp"
#include "ms_error_ui.hpp"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sys/wait.h>

namespace ms {
namespace fs = std::filesystem;

OnlineEffectsCatalog::OnlineEffectsCatalog(CatalogConfig cfg) : cfg_(std::move(cfg)) {}

std::string OnlineEffectsCatalog::catalog_url() const {
  return "https://raw.githubusercontent.com/" + cfg_.github_owner + "/" +
         cfg_.github_repo + "/" + cfg_.branch + "/" + cfg_.catalog_path;
}

std::string OnlineEffectsCatalog::effect_url(const OnlineEffectEntry& e) const {
  if (!e.download_url.empty()) return e.download_url;
  return "https://raw.githubusercontent.com/" + cfg_.github_owner + "/" +
         cfg_.github_repo + "/" + cfg_.branch + "/" + e.msp_file;
}

std::vector<std::string> OnlineEffectsCatalog::offline_builtin_ids() {
  return {
    "gaussian_blur", "brightness_contrast", "opacity", "invert",
    "grayscale", "sepia", "vignette", "mirror", "chromakey"
  };
}

std::string OnlineEffectsCatalog::http_get(const std::string& url, int& exit_code) {
  exit_code = -1;
  // curl -fsSL
  std::string cmd = "curl -fsSL --max-time 30 \"" + url + "\"";
  FILE* pipe = popen((cmd + " 2>/dev/null").c_str(), "r");
  if (!pipe) return {};
  std::string out;
  char buf[4096];
  while (fgets(buf, sizeof(buf), pipe)) out += buf;
  int st = pclose(pipe);
  if (WIFEXITED(st)) exit_code = WEXITSTATUS(st);
  return out;
}

Error OnlineEffectsCatalog::http_download(const std::string& url, const std::string& out_path) {
  fs::create_directories(fs::path(out_path).parent_path());
  std::string cmd = "curl -fsSL --max-time 60 -o \"" + out_path + "\" \"" + url + "\"";
  int ec = std::system(cmd.c_str());
  if (ec != 0 || !fs::exists(out_path))
    return Error::make(ErrorCode::NetworkHttpError, "download failed", url);
  return Error::success();
}

static std::string json_str_field(const std::string& j, const std::string& key) {
  std::string pat = "\"" + key + "\"";
  auto pos = j.find(pat);
  if (pos == std::string::npos) return {};
  pos = j.find(':', pos);
  if (pos == std::string::npos) return {};
  while (pos < j.size() && (j[pos]==':'||j[pos]==' '||j[pos]=='\t')) ++pos;
  if (pos < j.size() && j[pos]=='"') {
    auto end = j.find('"', pos+1);
    if (end==std::string::npos) return {};
    return j.substr(pos+1, end-pos-1);
  }
  return {};
}

Error OnlineEffectsCatalog::parse_catalog_json(const std::string& json) {
  entries_.clear();
  // naive: split on "id"
  size_t pos = 0;
  while ((pos = json.find("\"id\"", pos)) != std::string::npos) {
    size_t block = json.rfind('{', pos);
    size_t end = json.find('}', pos);
    if (block == std::string::npos || end == std::string::npos) { pos += 4; continue; }
    std::string chunk = json.substr(block, end - block + 1);
    OnlineEffectEntry e;
    e.id = json_str_field(chunk, "id");
    e.name = json_str_field(chunk, "name");
    e.category = json_str_field(chunk, "category");
    e.msp_file = json_str_field(chunk, "msp");
    if (e.msp_file.empty()) e.msp_file = json_str_field(chunk, "msp_file");
    e.download_url = json_str_field(chunk, "url");
    if (e.id.empty()) { pos = end + 1; continue; }
    entries_.push_back(e);
    pos = end + 1;
  }
  return Error::success();
}

Error OnlineEffectsCatalog::load_from_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) return Error::make(ErrorCode::FileNotFound, "catalog file", path);
  std::ostringstream ss; ss << in.rdbuf();
  return parse_catalog_json(ss.str());
}

Error OnlineEffectsCatalog::refresh() {
  if (mode_ != EffectsMode::Online) {
    return Error::make(ErrorCode::NotSupported, "catalog refresh requires online mode");
  }
  int ec = 0;
  std::string url = catalog_url();
  std::string body = http_get(url, ec);
  if (ec != 0 || body.empty()) {
    // try fallback repo
    std::string fb = "https://raw.githubusercontent.com/" + cfg_.github_owner + "/" +
                     cfg_.fallback_repo + "/" + cfg_.branch + "/ms-effects/catalog.json";
    body = http_get(fb, ec);
    if (ec != 0 || body.empty()) {
      Error err = Error::make(ErrorCode::NetworkUnavailable,
                              "cannot fetch effects catalog", url, "OnlineEffects");
      report_error(err, "Online catalog");
      return err;
    }
  }
  Error e = parse_catalog_json(body);
  if (!e.ok()) return e;
  // cache locally
  try {
    fs::create_directories(cfg_.local_cache_dir);
    std::ofstream out(cfg_.local_cache_dir + "/catalog.json");
    out << body;
  } catch (...) {}
  return Error::success();
}

Error OnlineEffectsCatalog::download_effect(const std::string& id) {
  if (mode_ != EffectsMode::Online) {
    return Error::make(ErrorCode::NotSupported, "download requires online mode");
  }
  const OnlineEffectEntry* found = nullptr;
  for (auto& e : entries_) if (e.id == id) { found = &e; break; }
  if (!found)
    return Error::make(ErrorCode::EffectNotFound, "not in catalog", id);

  std::string url = effect_url(*found);
  std::string dest = cfg_.local_cache_dir + "/" + found->id + "/";
  fs::create_directories(dest);
  // download as package or manifest
  std::string out_file = dest + (found->msp_file.empty() ? "effect.msp" : fs::path(found->msp_file).filename().string());
  Error e = http_download(url, out_file);
  if (!e.ok()) {
    report_error(e, "Download effect " + id);
    return e;
  }
  // if it's a json manifest, scan_installed parent
  e = MspRegistry::instance().scan_installed(cfg_.local_cache_dir);
  for (auto& ent : entries_) if (ent.id == id) ent.installed = true;
  return e.ok() ? Error::success() : e;
}

std::vector<std::string> OnlineEffectsCatalog::list_ids() const {
  std::vector<std::string> ids;
  for (const auto& e : entries_) ids.push_back(e.id);
  return ids;
}

} // namespace ms
