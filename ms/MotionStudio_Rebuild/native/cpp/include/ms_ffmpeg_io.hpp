#pragma once
/**
 * FFmpeg media I/O
 * - Probe via ffprobe
 * - Decode frames via ffmpeg CLI → raw/ppm (no libav required)
 * - Encode sequence / simple export via ffmpeg CLI
 * Optional: compile with -DMS_HAVE_LIBAV for in-process libav (if installed)
 */
#include "ms_media.hpp"
#include "ms_error.hpp"
#include "ms_image.hpp"
#include <string>
#include <vector>
#include <memory>

namespace ms {

struct FFmpegPaths {
  std::string ffmpeg = "ffmpeg";
  std::string ffprobe = "ffprobe";
};

bool ffmpeg_available(const FFmpegPaths& paths = {});
bool ffprobe_available(const FFmpegPaths& paths = {});

/** Rich probe using ffprobe -v quiet -print_format json -show_format -show_streams */
MediaProbeResult ffmpeg_probe(const std::string& path, const FFmpegPaths& paths = {});

/**
 * Decode one video frame at time_sec into RGBA ImageBuffer.
 * Uses: ffmpeg -ss T -i path -frames:v 1 -f rawvideo -pix_fmt rgba -
 */
Error ffmpeg_decode_frame(const std::string& path, double time_sec,
                          ImageBuffer& out, int max_w = 0, int max_h = 0,
                          const FFmpegPaths& paths = {});

/**
 * Extract N frames as PPM sequence into out_dir (frame_000000.ppm …)
 */
Error ffmpeg_extract_sequence(const std::string& path, const std::string& out_dir,
                              double start, double duration, float fps,
                              int max_w = 0, const FFmpegPaths& paths = {});

/**
 * Encode image sequence (frame_%06d.ppm) or list pattern to mp4
 */
Error ffmpeg_encode_sequence(const std::string& pattern_or_dir,
                             const std::string& out_mp4,
                             float fps = 24.f,
                             const FFmpegPaths& paths = {});

/**
 * IMediaDecoder backed by FFmpeg CLI (caches last frame seek)
 */
class FFmpegDecoder : public IMediaDecoder {
public:
  explicit FFmpegDecoder(FFmpegPaths paths = {});
  Error open(const std::string& path) override;
  void close() override;
  Error seek_ms(int64_t ms) override;
  Error read_video(ImageBuffer& out, int64_t& pts_ms) override;
  Error read_audio(AudioBuffer& out, int64_t& pts_ms) override;
  const MediaProbeResult& info() const override { return info_; }

  void set_max_size(int w, int h) { max_w_ = w; max_h_ = h; }

private:
  FFmpegPaths paths_;
  MediaProbeResult info_;
  std::string path_;
  int64_t pos_ms_ = 0;
  bool open_ = false;
  int max_w_ = 1280;
  int max_h_ = 720;
  int frame_interval_ms_ = 33;
};

/** Factory: prefer FFmpeg if available, else SlateDecoder */
std::unique_ptr<IMediaDecoder> create_media_decoder(const std::string& path,
                                                    const FFmpegPaths& paths = {});

} // namespace ms
