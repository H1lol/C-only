#include "ms_export.hpp"
#include "ms_ffmpeg_io.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <algorithm>

namespace ms {
namespace fs = std::filesystem;

static std::string container_ext(ExportContainer c) {
  switch (c) {
    case ExportContainer::Mov: return "mov";
    case ExportContainer::Mkv: return "mkv";
    case ExportContainer::Webm: return "webm";
    default: return "mp4";
  }
}

static std::string vcodec_args(const ExportPreset& p) {
  switch (p.video_codec) {
    case ExportCodec::H265:
      return "-c:v libx265 -tag:v hvc1 " + p.ffmpeg_vargs;
    case ExportCodec::Vp9:
      return "-c:v libvpx-vp9 " + p.ffmpeg_vargs;
    case ExportCodec::ProRes:
      return "-c:v prores_ks -profile:v 3 " + p.ffmpeg_vargs;
    case ExportCodec::Copy:
      return "-c:v copy";
    default:
      if (!p.ffmpeg_vargs.empty()) return "-c:v libx264 " + p.ffmpeg_vargs;
      return "-c:v libx264 -preset fast -crf 20";
  }
}

std::vector<ExportPreset> default_export_presets() {
  std::vector<ExportPreset> v;
  auto add = [&](const char* id, const char* name, ExportCodec c, int w, int h, int br,
                 const char* extra = "") {
    ExportPreset p;
    p.id = id; p.name = name; p.video_codec = c;
    p.width = w; p.height = h; p.video_bitrate_kbps = br;
    p.ffmpeg_vargs = extra;
    v.push_back(p);
  };
  add("yt_1080p", "YouTube 1080p", ExportCodec::H264, 1920, 1080, 12000, "-crf 20 -preset medium");
  // Additional delivery formats drawn from the portrait / square / UHD profile coverage
  // common to professional NLEs (Kdenlive) and modern social-first editors (Sprocket).
  add("yt_4k", "YouTube 4K UHD", ExportCodec::H265, 3840, 2160, 30000, "-crf 22 -preset medium");
  add("social_portrait_1080p", "Social Portrait 1080p", ExportCodec::H264, 1080, 1920, 12000, "-crf 20 -preset medium");
  add("social_portrait_4k", "Social Portrait 4K", ExportCodec::H265, 2160, 3840, 30000, "-crf 22 -preset medium");
  add("square_1080p", "Square 1080p", ExportCodec::H264, 1080, 1080, 10000, "-crf 20 -preset medium");
  add("webm_vp9_1080p", "WebM VP9 1080p", ExportCodec::Vp9, 1920, 1080, 8000, "-crf 31 -b:v 0 -row-mt 1");
  v.back().container = ExportContainer::Webm;
  add("prores_4k_master", "ProRes 4K Master", ExportCodec::ProRes, 3840, 2160, 0, "");
  v.back().container = ExportContainer::Mov;
  add("yt_720p", "YouTube 720p", ExportCodec::H264, 1280, 720, 8000, "-crf 21 -preset fast");
  add("mobile_720p", "Mobile 720p", ExportCodec::H264, 1280, 720, 5000, "-crf 23 -preset veryfast");
  add("proxy_540p", "Proxy 540p", ExportCodec::H264, 960, 540, 2500, "-crf 28 -preset ultrafast");
  add("hevc_1080p", "HEVC 1080p", ExportCodec::H265, 1920, 1080, 8000, "-crf 24");
  add("lossless_copy", "Lossless (stream copy)", ExportCodec::Copy, 0, 0, 0, "");
  add("master_prores", "Master ProRes", ExportCodec::ProRes, 0, 0, 0, "");
  return v;
}

const ExportPreset* find_preset(const std::string& id) {
  static std::vector<ExportPreset> presets = default_export_presets();
  for (const auto& p : presets)
    if (p.id == id) return &p;
  return nullptr;
}

Error lossless_cut(const std::string& input, const std::string& output,
                   double in_sec, double out_sec) {
  if (!fs::exists(input))
    return Error::make(ErrorCode::FileNotFound, "input", input);
  if (out_sec <= in_sec && out_sec > 0)
    return Error::make(ErrorCode::InvalidArgument, "out <= in");
  fs::create_directories(fs::path(output).parent_path());
  std::ostringstream cmd;
  cmd << "ffmpeg -v error -y -ss " << in_sec << " -i \"" << input << "\"";
  if (out_sec > in_sec) cmd << " -to " << out_sec;
  cmd << " -c copy -avoid_negative_ts make_zero \"" << output << "\"";
  int ec = std::system(cmd.str().c_str());
  if (ec != 0 || !fs::exists(output))
    return Error::make(ErrorCode::Unknown, "lossless_cut failed", output);
  return Error::success();
}

Error lossless_join(const std::vector<std::string>& inputs, const std::string& output) {
  if (inputs.empty())
    return Error::make(ErrorCode::InvalidArgument, "no inputs");
  std::string list = output + ".concat.txt";
  {
    std::ofstream f(list);
    for (const auto& in : inputs) {
      if (!fs::exists(in))
        return Error::make(ErrorCode::FileNotFound, "segment", in);
      f << "file '" << in << "'\n";
    }
  }
  std::ostringstream cmd;
  cmd << "ffmpeg -v error -y -f concat -safe 0 -i \"" << list
      << "\" -c copy \"" << output << "\"";
  int ec = std::system(cmd.str().c_str());
  fs::remove(list);
  if (ec != 0 || !fs::exists(output))
    return Error::make(ErrorCode::Unknown, "lossless_join failed", output);
  return Error::success();
}

Error export_media_file(const std::string& input, const std::string& output,
                        const ExportPreset& preset,
                        double in_sec, double out_sec,
                        ExportProgressFn on_progress) {
  if (!fs::exists(input))
    return Error::make(ErrorCode::FileNotFound, "input", input);
  fs::create_directories(fs::path(output).parent_path());

  ExportJob job;
  job.output_path = output;
  job.preset = preset;
  job.in_sec = in_sec;
  job.out_sec = out_sec;
  job.status = "running";

  if (preset.video_codec == ExportCodec::Copy || preset.id == "lossless_copy") {
    job.lossless = true;
    auto e = lossless_cut(input, output, in_sec, out_sec > 0 ? out_sec : 1e9);
    job.progress = 1.f;
    job.status = e.ok() ? "done" : "error";
    job.message = e.message;
    if (on_progress) on_progress(job);
    return e;
  }

  std::ostringstream cmd;
  cmd << "ffmpeg -v error -y";
  if (in_sec > 0) cmd << " -ss " << in_sec;
  cmd << " -i \"" << input << "\"";
  if (out_sec > in_sec) cmd << " -to " << out_sec;

  if (preset.width > 0 && preset.height > 0)
    cmd << " -vf scale=" << preset.width << ":" << preset.height;

  cmd << " " << vcodec_args(preset);
  if (preset.video_codec != ExportCodec::Copy && preset.video_bitrate_kbps > 0 &&
      preset.ffmpeg_vargs.find("crf") == std::string::npos)
    cmd << " -b:v " << preset.video_bitrate_kbps << "k";

  cmd << " -c:a aac -b:a " << preset.audio_bitrate_kbps << "k";
  cmd << " \"" << output << "\"";

  if (on_progress) {
    job.progress = 0.1f;
    on_progress(job);
  }
  int ec = std::system(cmd.str().c_str());
  job.progress = 1.f;
  if (ec != 0 || !fs::exists(output)) {
    job.status = "error";
    job.message = "ffmpeg export failed";
    if (on_progress) on_progress(job);
    return Error::make(ErrorCode::Unknown, "export failed", output);
  }
  job.status = "done";
  if (on_progress) on_progress(job);
  return Error::success();
}

Error export_session(ProjectSession& session, const std::string& output,
                     const ExportPreset& preset,
                     ExportProgressFn on_progress) {
  // Prefer single-clip fast path when only one media on V1
  const MediaInfo* single = nullptr;
  double in_s = 0, out_s = 0;
  int clip_n = 0;
  for (const auto& tr : session.tracks()) {
    for (const auto& c : tr.clips) {
      ++clip_n;
      if (const MediaInfo* mi = session.find_media(c.media_id)) {
        single = mi;
        in_s = c.source_in;
        out_s = c.source_in + c.duration;
      }
    }
  }
  if (clip_n == 1 && single) {
    return export_media_file(single->path, output, preset, in_s, out_s, on_progress);
  }

  // Multi-clip: render via MLT XML + ffmpeg if available
  MltTimeline tl;
  if (tl.available()) {
    Error e = tl.build(session);
    if (!e.ok()) return e;
    std::string xml = output + ".mlt";
    e = tl.save_xml(xml);
    if (e.ok()) {
      std::ostringstream cmd;
      cmd << "ffmpeg -v error -y -i \"" << xml << "\" ";
      if (preset.width > 0 && preset.height > 0)
        cmd << "-vf scale=" << preset.width << ":" << preset.height << " ";
      cmd << vcodec_args(preset) << " -c:a aac \"" << output << "\"";
      int ec = std::system(cmd.str().c_str());
      fs::remove(xml);
      if (ec == 0 && fs::exists(output)) {
        if (on_progress) {
          ExportJob j;
          j.status = "done";
          j.progress = 1.f;
          j.output_path = output;
          on_progress(j);
        }
        return Error::success();
      }
    }
  }

  // Fallback: export first clip only
  if (single)
    return export_media_file(single->path, output, preset, in_s, out_s, on_progress);
  return Error::make(ErrorCode::InvalidArgument, "nothing to export");
}

std::string ExportQueue::enqueue(const ExportJob& job) {
  jobs_.push_back(job);
  if (jobs_.back().id.empty())
    jobs_.back().id = "job" + std::to_string(jobs_.size());
  jobs_.back().status = "queued";
  return jobs_.back().id;
}

Error ExportQueue::run_next(ProjectSession* session) {
  for (auto& j : jobs_) {
    if (j.status != "queued") continue;
    j.status = "running";
    Error e;
    if (j.lossless || j.preset.video_codec == ExportCodec::Copy) {
      // need input from session first media
      std::string input;
      if (session) {
        for (const auto& m : session->media()) { input = m.path; break; }
      }
      if (input.empty())
        e = Error::make(ErrorCode::InvalidArgument, "no input for lossless");
      else
        e = lossless_cut(input, j.output_path, j.in_sec,
                         j.out_sec > 0 ? j.out_sec : 1e9);
    } else if (session) {
      e = export_session(*session, j.output_path, j.preset);
    } else {
      e = Error::make(ErrorCode::InvalidArgument, "no session");
    }
    j.progress = 1.f;
    j.status = e.ok() ? "done" : "error";
    j.message = e.message;
    return e;
  }
  return Error::make(ErrorCode::InvalidArgument, "no queued jobs");
}

} // namespace ms
