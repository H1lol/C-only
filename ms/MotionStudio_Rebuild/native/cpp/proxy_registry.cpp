#include "include/ms_proxy_registry.hpp"

namespace ms {

void ProxyRegistry::set(const std::string& media_path, const std::string& proxy_path) {
  if (media_path.empty() || proxy_path.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  auto it = proxies_.find(media_path);
  if (it != proxies_.end() && it->second == proxy_path) return;
  proxies_[media_path] = proxy_path;
  ++generation_;
}

void ProxyRegistry::erase(const std::string& media_path) {
  std::lock_guard<std::mutex> lock(mu_);
  if (proxies_.erase(media_path) > 0) ++generation_;
}

void ProxyRegistry::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  if (proxies_.empty()) return;
  proxies_.clear();
  ++generation_;
}

std::string ProxyRegistry::proxy_for(const std::string& media_path) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = proxies_.find(media_path);
  return it == proxies_.end() ? std::string() : it->second;
}

std::string ProxyRegistry::resolve(const std::string& media_path) const {
  std::lock_guard<std::mutex> lock(mu_);
  if (!enabled_) return media_path;
  auto it = proxies_.find(media_path);
  return it == proxies_.end() ? media_path : it->second;
}

void ProxyRegistry::set_enabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mu_);
  if (enabled_ == enabled) return;
  enabled_ = enabled;
  ++generation_;
}

bool ProxyRegistry::enabled() const {
  std::lock_guard<std::mutex> lock(mu_);
  return enabled_;
}

int ProxyRegistry::generation() const {
  std::lock_guard<std::mutex> lock(mu_);
  return generation_;
}

size_t ProxyRegistry::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return proxies_.size();
}

ProxyRegistry& proxy_registry() {
  static ProxyRegistry registry;
  return registry;
}

} // namespace ms
