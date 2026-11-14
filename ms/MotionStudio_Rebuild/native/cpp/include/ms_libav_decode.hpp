#pragma once
/**
 * Milestone 1 — in-process libav decode + frame prefetch
 * Desktop: software decode. Android: same API + MediaCodec hwaccel later.
 */
#include "ms_media.hpp"
#include "ms_error.hpp"
#include "ms_image.hpp"
#include "ms_frame_buffer.hpp"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <deque>
#include <condition_variable>

namespace ms {

struct PrefetchStats {
  int cache_size = 0;
  int hits = 0;
  int misses = 0;
  int decoded = 0;
};

/**
 * In-process demux/decode to RGBA via libav + swscale.
 * Build with -DMS_HAVE_LIBAV
 */
class LibavDecoder : public IMediaDecoder {
public:
  LibavDecoder();
  ~LibavDecoder() override;

  Error open(const std::string& path) override;
  void close() override;
  Error seek_ms(int64_t ms) override;
  Error read_video(ImageBuffer& out, int64_t& pts_ms) override;
  Error read_audio(AudioBuffer& out, int64_t& pts_ms) override;
  const MediaProbeResult& info() const override { return info_; }

  void set_output_size(int w, int h) { out_w_ = w; out_h_ = h; }
  bool is_open() const { return open_; }

  /** Prefetch window (frames ahead of playhead) */
  void set_prefetch(int frames);
  void start_prefetch();
  void stop_prefetch();
  PrefetchStats stats() const;

  /** Decode frame at time and publish to FrameBufferHub */
  Error decode_and_publish(double time_sec);

private:
  struct CachedFrame {
    int64_t pts_ms = 0;
    ImageBuffer image;
  };

  bool open_ = false;
  MediaProbeResult info_;
  std::string path_;
  int out_w_ = 0;
  int out_h_ = 0;
  int64_t pos_ms_ = 0;
  double fps_ = 24.0;

  // Opaque AV* held only when MS_HAVE_LIBAV
  void* fmt_ctx_ = nullptr;
  void* codec_ctx_ = nullptr;
  void* sws_ = nullptr;
  void* frame_ = nullptr;
  void* packet_ = nullptr;
  int video_stream_ = -1;

  // Prefetch
  int prefetch_n_ = 8;
  std::deque<CachedFrame> cache_;
  mutable std::mutex cache_mu_;
  std::mutex decode_mu_;
  std::thread worker_;
  std::atomic<bool> run_{false};
  std::atomic<int64_t> target_ms_{0};
  PrefetchStats stats_;

  Error open_internal(const std::string& path);
  void close_internal();
  Error decode_at(int64_t ms, ImageBuffer& out, int64_t& pts_ms);
  void prefetch_loop();
};

bool libav_available();

/** Factory: Libav if built-in, else FFmpeg CLI decoder */
std::unique_ptr<IMediaDecoder> create_best_decoder(const std::string& path);

/**
 * Playback that drives Libav + publishes every tick to frame buffer
 */
class LibavPlayback {
public:
  Error open(const std::string& path, int max_w = 1280, int max_h = 720);
  void close();
  Error play();
  void pause();
  void stop();
  Error seek(double sec);
  /** Advance clock; decode+publish when due. Returns true if new frame published */
  bool tick();
  bool is_playing() const { return playing_; }
  double time() const { return time_; }
  double duration() const { return duration_; }
  PrefetchStats stats() const;

private:
  std::unique_ptr<LibavDecoder> dec_;
  bool playing_ = false;
  double time_ = 0;
  double duration_ = 0;
  double fps_ = 24;
  std::chrono::steady_clock::time_point last_wall_{};
  bool clock_ = false;
};

} // namespace ms
