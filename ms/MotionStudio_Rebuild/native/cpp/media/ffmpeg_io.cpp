#include "ms_ffmpeg_io.hpp"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <sys/wait.h>
#include <cmath>

namespace ms {
namespace fs = std::filesystem;

namespace {

std::string run_capture(const std::string& cmd, int& exit_code) {
  exit_code = -1;
  FILE* pipe = popen((cmd + " 2>/dev/null").c_str(), "r");
  if (!pipe) return {};
  std::string out;
  char buf[4096];
  while (fgets(buf, sizeof(buf), pipe)) out += buf;
  int st = pclose(pipe);
  if (WIFEXITED(st)) exit_code = WEXITSTATUS(st);
  return out;
}

bool cmd_exists(const std::string& bin) {
  int ec = 1;
  run_capture("command -v " + bin, ec);
  return ec == 0;
}

// Minimal JSON field helpers (same style as MSP)
std::string json_str(const std::string& j, const std::string& key) {
  std::string pat = "\"" + key + "\"";
  auto pos = j.find(pat);
  if (pos == std::string::npos) return {};
  pos = j.find(':', pos);
  if (pos == std::string::npos) return {};
  while (pos < j.size() && (j[pos] == ':' || j[pos] == ' ' || j[pos] == '\t')) ++pos;
  if (pos < j.size() && j[pos] == '"') {
    auto end = j.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return j.substr(pos + 1, end - pos - 1);
  }
  auto end = pos;
  while (end < j.size() && std::string(",}\n\r ").find(j[end]) == std::string::npos) ++end;
  return j.substr(pos, end - pos);
}

double json_num(const std::string& j, const std::string& key, double def = 0) {
  auto s = json_str(j, key);
  if (s.empty()) return def;
  try { return std::stod(s); } catch (...) { return def; }
}

} // namespace

bool ffmpeg_available(const FFmpegPaths& paths) {
  return cmd_exists(paths.ffmpeg);
}

bool ffprobe_available(const FFmpegPaths& paths) {
  return cmd_exists(paths.ffprobe);
}

MediaProbeResult ffmpeg_probe(const std::string& path, const FFmpegPaths& paths) {
  MediaProbeResult r;
  r.path = path;
  if (!ffprobe_available(paths)) {
    // fall back to light probe
    r = MediaProbe::probe(path);
    r.error = r.ok ? "" : r.error;
    return r;
  }
  std::ostringstream cmd;
  cmd << paths.ffprobe << " -v quiet -print_format json -show_format -show_streams \""
      << path << "\"";
  int ec = 0;
  std::string json = run_capture(cmd.str(), ec);
  if (ec != 0 || json.empty()) {
    r.error = "ffprobe failed";
    r = MediaProbe::probe(path);
    return r;
  }

  r.ok = true;
  r.container = json_str(json, "format_name");
  double dur = json_num(json, "duration", 0);
  // duration may be under format
  if (dur <= 0) {
    auto p = json.find("\"format\"");
    if (p != std::string::npos)
      dur = json_num(json.substr(p, 500), "duration", 0);
  }
  r.duration_ms = static_cast<int64_t>(dur * 1000.0);

  // Split streams by finding codec_type
  size_t pos = 0;
  while ((pos = json.find("\"codec_type\"", pos)) != std::string::npos) {
    size_t block = json.rfind('{', pos);
    size_t end = json.find('}', pos);
    if (block == std::string::npos || end == std::string::npos) { pos += 10; continue; }
    std::string chunk = json.substr(block, end - block + 1);
    MediaStreamInfo s;
    s.type = json_str(chunk, "codec_type");
    s.codec = json_str(chunk, "codec_name");
    s.width = static_cast<int>(json_num(chunk, "width", 0));
    s.height = static_cast<int>(json_num(chunk, "height", 0));
    s.sample_rate = static_cast<int>(json_num(chunk, "sample_rate", 0));
    s.channels = static_cast<int>(json_num(chunk, "channels", 0));
    // avg_frame_rate like "24/1"
    std::string fr = json_str(chunk, "avg_frame_rate");
    if (fr.empty()) fr = json_str(chunk, "r_frame_rate");
    if (!fr.empty()) {
      auto slash = fr.find('/');
      if (slash != std::string::npos) {
        double num = std::atof(fr.c_str());
        double den = std::atof(fr.c_str() + slash + 1);
        if (den > 0) s.fps = num / den;
      }
    }
    double sd = json_num(chunk, "duration", 0);
    if (sd > 0) s.duration_ms = static_cast<int64_t>(sd * 1000);
    else s.duration_ms = r.duration_ms;
    s.index = static_cast<int>(r.streams.size());
    r.streams.push_back(s);
    pos = end + 1;
  }

  if (r.streams.empty()) {
    MediaProbeResult lite = MediaProbe::probe(path);
    return lite.ok ? lite : r;
  }
  return r;
}

Error ffmpeg_decode_frame(const std::string& path, double time_sec,
                          ImageBuffer& out, int max_w, int max_h,
                          const FFmpegPaths& paths) {
  if (!ffmpeg_available(paths))
    return Error::make(ErrorCode::NotSupported, "ffmpeg not found");

  // Probe size
  auto info = ffmpeg_probe(path, paths);
  int w = 640, h = 360;
  for (const auto& s : info.streams) {
    if (s.type == "video" && s.width > 0) { w = s.width; h = s.height; break; }
  }
  if (max_w > 0 && w > max_w) {
    h = h * max_w / w;
    w = max_w;
  }
  if (max_h > 0 && h > max_h) {
    w = w * max_h / h;
    h = max_h;
  }

  std::ostringstream cmd;
  cmd << paths.ffmpeg << " -v error -ss " << time_sec
      << " -i \"" << path << "\""
      << " -frames:v 1 -f rawvideo -pix_fmt rgba"
      << " -s " << w << "x" << h
      << " -";
  FILE* pipe = popen(cmd.str().c_str(), "r");
  if (!pipe) return Error::make(ErrorCode::MediaDecodeFailed, "popen ffmpeg");

  if (!out.allocate(w, h, PixelFormat::RGBA8)) {
    pclose(pipe);
    return Error::make(ErrorCode::OutOfMemory, "frame");
  }
  size_t need = static_cast<size_t>(w) * h * 4;
  size_t got = fread(out.data(), 1, need, pipe);
  int st = pclose(pipe);
  if (got < need) {
    // partial — fill rest black
    std::memset(out.data() + got, 0, need - got);
    if (got == 0)
      return Error::make(ErrorCode::MediaDecodeFailed, "no frame data", path);
  }
  (void)st;
  return Error::success();
}

Error ffmpeg_extract_sequence(const std::string& path, const std::string& out_dir,
                              double start, double duration, float fps,
                              int max_w, const FFmpegPaths& paths) {
  if (!ffmpeg_available(paths))
    return Error::make(ErrorCode::NotSupported, "ffmpeg not found");
  fs::create_directories(out_dir);
  std::ostringstream scale;
  if (max_w > 0) scale << " -vf scale=" << max_w << ":-2";
  std::ostringstream cmd;
  cmd << paths.ffmpeg << " -v error -y -ss " << start
      << " -i \"" << path << "\""
      << " -t " << duration
      << " -r " << fps
      << scale.str()
      << " \"" << out_dir << "/frame_%06d.ppm\"";
  int ec = std::system(cmd.str().c_str());
  if (ec != 0) return Error::make(ErrorCode::MediaDecodeFailed, "extract sequence failed");
  return Error::success();
}

Error ffmpeg_encode_sequence(const std::string& pattern_or_dir,
                             const std::string& out_mp4,
                             float fps,
                             const FFmpegPaths& paths) {
  if (!ffmpeg_available(paths))
    return Error::make(ErrorCode::NotSupported, "ffmpeg not found");
  std::string pattern = pattern_or_dir;
  if (fs::is_directory(pattern_or_dir))
    pattern = pattern_or_dir + "/frame_%06d.ppm";
  std::ostringstream cmd;
  cmd << paths.ffmpeg << " -v error -y -framerate " << fps
      << " -i \"" << pattern << "\""
      << " -c:v libx264 -pix_fmt yuv420p \"" << out_mp4 << "\"";
  int ec = std::system(cmd.str().c_str());
  if (ec != 0) return Error::make(ErrorCode::Unknown, "encode failed", out_mp4);
  return Error::success();
}

FFmpegDecoder::FFmpegDecoder(FFmpegPaths paths) : paths_(std::move(paths)) {}

Error FFmpegDecoder::open(const std::string& path) {
  path_ = path;
  info_ = ffmpeg_probe(path, paths_);
  if (!info_.ok && !ffmpeg_available(paths_)) {
    return Error::make(ErrorCode::MediaOpenFailed, "cannot open", path);
  }
  info_.path = path;
  if (info_.duration_ms <= 0) info_.duration_ms = 10000;
  double fps = 30;
  for (const auto& s : info_.streams)
    if (s.type == "video" && s.fps > 1) { fps = s.fps; break; }
  frame_interval_ms_ = static_cast<int>(1000.0 / fps);
  if (frame_interval_ms_ < 1) frame_interval_ms_ = 33;
  open_ = true;
  pos_ms_ = 0;
  return Error::success();
}

void FFmpegDecoder::close() { open_ = false; }

Error FFmpegDecoder::seek_ms(int64_t ms) {
  if (!open_) return Error::make(ErrorCode::NotInitialized, "closed");
  pos_ms_ = std::max<int64_t>(0, ms);
  return Error::success();
}

Error FFmpegDecoder::read_video(ImageBuffer& out, int64_t& pts_ms) {
  if (!open_) return Error::make(ErrorCode::NotInitialized, "closed");
  pts_ms = pos_ms_;
  double t = pos_ms_ / 1000.0;
  Error e;
  if (ffmpeg_available(paths_)) {
    e = ffmpeg_decode_frame(path_, t, out, max_w_, max_h_, paths_);
  } else {
    // slate fallback
    SlateDecoder slate;
    slate.open(path_);
    slate.seek_ms(pos_ms_);
    e = slate.read_video(out, pts_ms);
  }
  pos_ms_ += frame_interval_ms_;
  return e;
}

Error FFmpegDecoder::read_audio(AudioBuffer& out, int64_t& pts_ms) {
  out.sample_rate = 48000;
  out.channels = 2;
  out.samples.assign(48000 / 30 * 2, 0.f);
  pts_ms = pos_ms_;
  return Error::success();
}

std::unique_ptr<IMediaDecoder> create_media_decoder(const std::string& path,
                                                    const FFmpegPaths& paths) {
  if (ffmpeg_available(paths)) {
    auto d = std::make_unique<FFmpegDecoder>(paths);
    if (d->open(path).ok()) return d;
  }
  auto s = std::make_unique<SlateDecoder>();
  s->open(path);
  return s;
}

} // namespace ms
