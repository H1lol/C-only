#pragma once
#include "ms_image.hpp"
#include "ms_error.hpp"
#include <string>
#include <filesystem>
#include <cstdint>

namespace ms {

struct ProxySpec {
  int max_width = 1280;
  int max_height = 720;
  int video_bitrate_kbps = 4000;
  int audio_bitrate_kbps = 128;
  std::string codec = "libx264";
};

class ProxyManager {
public:
  explicit ProxyManager(std::string root = "./proxy_cache");
  void set_root(const std::string& root);
  std::string root() const;
  std::string proxy_path(const std::string& media_path, const ProxySpec& spec = {}) const;
  bool has_proxy(const std::string& media_path, const ProxySpec& spec = {}) const;
  Error build_proxy(const std::string& media_path, const ProxySpec& spec = {}) const;
  Error remove_proxy(const std::string& media_path, const ProxySpec& spec = {}) const;

private:
  std::string root_;
  static std::string key_for(const std::string& path, const ProxySpec& spec);
};

class DiskFrameCache {
public:
  explicit DiskFrameCache(std::string root = "./frame_cache");
  void set_root(const std::string& root);
  std::string root() const;
  bool get(const std::string& key, ImageBuffer& out) const;
  Error put(const std::string& key, const ImageBuffer& image) const;
  bool remove(const std::string& key) const;
  void clear() const;
  size_t bytes() const;

private:
  std::string root_;
  std::filesystem::path path_for(const std::string& key) const;
  static std::string hash_key(const std::string& key);
};

} // namespace ms
