#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ms {

/**
 * Which media files have a generated proxy, and whether decoding should use it.
 *
 * Proxies are made by the app (MediaCodec transcode on Android), which knows the
 * file it just wrote, so the mapping is registered explicitly rather than
 * recomputed from a hash: std::hash is not stable between the C++ library and
 * Kotlin, and a proxy that decodes from the wrong file is worse than none.
 *
 * [resolve] is the only thing the decoder factory calls. Preview keeps proxies
 * enabled; export turns them off so the master is still encoded from source
 * footage rather than from a 720p stand-in. Every change bumps [generation],
 * which is how the timeline player knows its cached decoders point at the wrong
 * file and must be reopened.
 */
class ProxyRegistry {
public:
  void set(const std::string& media_path, const std::string& proxy_path);
  void erase(const std::string& media_path);
  void clear();

  /** Registered proxy for this media, or empty. Ignores the enabled flag. */
  std::string proxy_for(const std::string& media_path) const;

  /** The path the decoder should open: the proxy when enabled and registered. */
  std::string resolve(const std::string& media_path) const;

  void set_enabled(bool enabled);
  bool enabled() const;

  /** Bumped whenever cached decoders could now be stale. */
  int generation() const;

  size_t size() const;

private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, std::string> proxies_;
  bool enabled_ = true;
  int generation_ = 1;
};

ProxyRegistry& proxy_registry();

} // namespace ms
