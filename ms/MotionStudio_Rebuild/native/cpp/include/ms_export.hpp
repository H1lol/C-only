#pragma once
/**
 * Milestone 4 — Export: presets, encode, lossless cut/join
 */
#include "ms_error.hpp"
#include "ms_project.hpp"
#include "ms_mlt_timeline.hpp"
#include "ms_frame_buffer.hpp"
#include <string>
#include <vector>
#include <functional>

namespace ms {

enum class ExportContainer : uint8_t { Mp4 = 0, Mov, Mkv, Webm };
enum class ExportCodec : uint8_t {
  H264 = 0,
  H265,
  ProRes,   // desktop/ffmpeg if available
  Vp9,
  Copy,     // stream copy (lossless cut)
};

struct ExportPreset {
  std::string id;
  std::string name;
  ExportContainer container = ExportContainer::Mp4;
  ExportCodec video_codec = ExportCodec::H264;
  int width = 0;   // 0 = source
  int height = 0;
  int video_bitrate_kbps = 8000;
  int audio_bitrate_kbps = 192;
  int fps_num = 0; // 0 = source
  int fps_den = 1;
  std::string ffmpeg_vargs; // extra, e.g. -crf 18 -preset fast
};

struct ExportJob {
  std::string id;
  std::string output_path;
  ExportPreset preset;
  double in_sec = 0;
  double out_sec = 0; // 0 = full
  bool lossless = false;
  float progress = 0;
  std::string status; // queued|running|done|error
  std::string message;
};

using ExportProgressFn = std::function<void(const ExportJob&)>;

/** HandBrake-style built-in presets */
std::vector<ExportPreset> default_export_presets();
const ExportPreset* find_preset(const std::string& id);

/**
 * Lossless cut: stream-copy a segment [in,out) without re-encode (keyframe-aware)
 * Uses ffmpeg -ss -to -c copy
 */
Error lossless_cut(const std::string& input, const std::string& output,
                   double in_sec, double out_sec);

/** Concat demuxer join of same-codec segments (lossless) */
Error lossless_join(const std::vector<std::string>& inputs, const std::string& output);

/**
 * Frame-accurate re-encode export from MLT timeline or media file
 */
Error export_media_file(const std::string& input, const std::string& output,
                        const ExportPreset& preset,
                        double in_sec = 0, double out_sec = 0,
                        ExportProgressFn on_progress = nullptr);

/** Export session via MLT XML + ffmpeg, or frame dump + encode */
Error export_session(ProjectSession& session, const std::string& output,
                     const ExportPreset& preset,
                     ExportProgressFn on_progress = nullptr);

/** Simple job runner */
class ExportQueue {
public:
  std::string enqueue(const ExportJob& job);
  Error run_next(ProjectSession* session = nullptr);
  const std::vector<ExportJob>& jobs() const { return jobs_; }

private:
  std::vector<ExportJob> jobs_;
};

} // namespace ms
