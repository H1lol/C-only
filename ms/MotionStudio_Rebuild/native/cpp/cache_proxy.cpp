#include "include/ms_cache_proxy.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>
#include <cstdlib>

namespace ms {
namespace fs = std::filesystem;

static uint32_t read_u32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24); }
static void write_u32(std::ofstream& f, uint32_t v) { char p[4] = {char(v),char(v>>8),char(v>>16),char(v>>24)}; f.write(p,4); }

ProxyManager::ProxyManager(std::string root) : root_(std::move(root)) {}
void ProxyManager::set_root(const std::string& root) { root_ = root; }
std::string ProxyManager::root() const { return root_; }
std::string ProxyManager::key_for(const std::string& path, const ProxySpec& s) {
  std::hash<std::string> h;
  std::ostringstream o; o << std::hex << h(path + "|" + std::to_string(s.max_width) + "x" + std::to_string(s.max_height) + "|" + s.codec);
  return o.str();
}
std::string ProxyManager::proxy_path(const std::string& media_path, const ProxySpec& spec) const {
  fs::path p(root_); p /= key_for(media_path, spec) + ".mp4"; return p.string();
}
bool ProxyManager::has_proxy(const std::string& media_path, const ProxySpec& spec) const { return fs::exists(proxy_path(media_path, spec)); }
Error ProxyManager::build_proxy(const std::string& media_path, const ProxySpec& spec) const {
  if (!fs::exists(media_path)) return Error::make(ErrorCode::FileNotFound, "media not found", media_path);
  fs::create_directories(root_);
  const auto out = proxy_path(media_path, spec);
  std::ostringstream cmd;
  cmd << "ffmpeg -y -v error -i \"" << media_path << "\" -vf \"scale='min(" << spec.max_width << ",iw)':'min(" << spec.max_height << ",ih)':force_original_aspect_ratio=decrease\""
       << " -c:v " << spec.codec << " -preset veryfast -b:v " << spec.video_bitrate_kbps << "k"
       << " -c:a aac -b:a " << spec.audio_bitrate_kbps << "k \"" << out << "\"";
  int rc = std::system(cmd.str().c_str());
  return rc == 0 && fs::exists(out) ? Error::success() : Error::make(ErrorCode::MediaDecodeFailed, "proxy generation failed", out);
}
Error ProxyManager::remove_proxy(const std::string& media_path, const ProxySpec& spec) const {
  std::error_code ec; fs::remove(proxy_path(media_path, spec), ec);
  return ec ? Error::make(ErrorCode::ProjectSaveFailed, "proxy removal failed", ec.message()) : Error::success();
}

DiskFrameCache::DiskFrameCache(std::string root) : root_(std::move(root)) {}
void DiskFrameCache::set_root(const std::string& root) { root_ = root; }
std::string DiskFrameCache::root() const { return root_; }
std::string DiskFrameCache::hash_key(const std::string& key) {
  std::hash<std::string> h; std::ostringstream o; o << std::hex << h(key); return o.str();
}
fs::path DiskFrameCache::path_for(const std::string& key) const { return fs::path(root_) / (hash_key(key) + ".msfc"); }
bool DiskFrameCache::get(const std::string& key, ImageBuffer& out) const {
  std::ifstream f(path_for(key), std::ios::binary); if (!f) return false;
  char magic[4]; f.read(magic,4); if (std::string(magic,4)!="MSFC") return false;
  uint32_t w=0,h=0,fmt=0,cs=0,bytes=0; f.read(reinterpret_cast<char*>(&w),4); f.read(reinterpret_cast<char*>(&h),4); f.read(reinterpret_cast<char*>(&fmt),4); f.read(reinterpret_cast<char*>(&cs),4); f.read(reinterpret_cast<char*>(&bytes),4);
  if (!w || !h || bytes > 512u*1024u*1024u) return false;
  if (!out.allocate((int)w,(int)h,(PixelFormat)fmt,(ColorSpace)cs)) return false;
  if (bytes != out.size_bytes()) return false;
  f.read(reinterpret_cast<char*>(out.data()), bytes); return f.good();
}
Error DiskFrameCache::put(const std::string& key, const ImageBuffer& image) const {
  if (!image.valid()) return Error::make(ErrorCode::InvalidArgument,"invalid image");
  fs::create_directories(root_); auto p=path_for(key); std::string tmp=p.string()+".tmp";
  std::ofstream f(tmp,std::ios::binary); if(!f) return Error::make(ErrorCode::FilePermission,"cannot create cache entry",tmp);
  f.write("MSFC",4); uint32_t w=image.width(),h=image.height(),fmt=(uint32_t)image.format(),cs=(uint32_t)image.color_space(),bytes=(uint32_t)image.size_bytes();
  f.write(reinterpret_cast<char*>(&w),4); f.write(reinterpret_cast<char*>(&h),4); f.write(reinterpret_cast<char*>(&fmt),4); f.write(reinterpret_cast<char*>(&cs),4); f.write(reinterpret_cast<char*>(&bytes),4); f.write(reinterpret_cast<const char*>(image.data()),bytes); f.close();
  std::error_code ec; fs::rename(tmp,p,ec); if(ec){fs::remove(tmp); return Error::make(ErrorCode::ProjectSaveFailed,"cache commit failed",ec.message());} return Error::success();
}
bool DiskFrameCache::remove(const std::string& key) const { std::error_code ec; fs::remove(path_for(key),ec); return !ec; }
void DiskFrameCache::clear() const { std::error_code ec; if(fs::exists(root_)) fs::remove_all(root_,ec); }
size_t DiskFrameCache::bytes() const { size_t n=0; std::error_code ec; if(!fs::exists(root_)) return 0; for(auto& e:fs::recursive_directory_iterator(root_,ec)) if(e.is_regular_file()) n += (size_t)e.file_size(); return n; }
} // namespace ms
