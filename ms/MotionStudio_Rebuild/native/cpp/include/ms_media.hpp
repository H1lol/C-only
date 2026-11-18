#pragma once
#include "ms_error.hpp"
#include "ms_image.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace ms {

struct MediaStreamInfo {
  int index = 0;
  std::string type; // video | audio | data
  std::string codec;
  int width = 0, height = 0;
  double fps = 0.0;
  int sample_rate = 0;
  int channels = 0;
  int64_t duration_ms = 0;
  int64_t bitrate = 0;
};

struct MediaProbeResult {
  std::string path;
  std::string container;
  int64_t duration_ms = 0;
  std::vector<MediaStreamInfo> streams;
  bool ok = false;
  std::string error;
};

/** FFmpeg-backed in production; file probe stub that records intent */
class MediaProbe {
public:
  static MediaProbeResult probe(const std::string& path);
};

struct AudioBuffer {
  int sample_rate = 48000;
  int channels = 2;
  std::vector<float> samples; // interleaved
  void clear() { samples.clear(); }
};

class IMediaDecoder {
public:
  virtual ~IMediaDecoder() = default;
  virtual Error open(const std::string& path) = 0;
  virtual void close() = 0;
  virtual Error seek_ms(int64_t ms) = 0;
  virtual Error read_video(ImageBuffer& out, int64_t& pts_ms) = 0;

  /**
   * Reads the frame that should be showing at `ms`.
   *
   * The default is a seek followed by one read, which makes every frame cost a
   * keyframe seek. Decoders that can decode forward override this to reuse the
   * codec position when the request is a small step ahead of the last frame,
   * which is what sequential playback and export actually ask for.
   */
  virtual Error read_video_at(int64_t ms, ImageBuffer& out, int64_t& pts_ms) {
    Error e = seek_ms(ms);
    if (!e.ok()) return e;
    return read_video(out, pts_ms);
  }
  virtual Error read_audio(AudioBuffer& out, int64_t& pts_ms) = 0;
  virtual const MediaProbeResult& info() const = 0;
};

/** Placeholder decoder — generates slate frames until FFmpeg is linked */
class SlateDecoder : public IMediaDecoder {
public:
  Error open(const std::string& path) override;
  void close() override;
  Error seek_ms(int64_t ms) override;
  Error read_video(ImageBuffer& out, int64_t& pts_ms) override;
  Error read_audio(AudioBuffer& out, int64_t& pts_ms) override;
  const MediaProbeResult& info() const override { return info_; }
private:
  MediaProbeResult info_;
  int64_t pos_ms_ = 0;
  bool open_ = false;
};

} // namespace ms
