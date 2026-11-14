#include "ms_media.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>

namespace ms {

MediaProbeResult MediaProbe::probe(const std::string& path) {
  MediaProbeResult r;
  r.path = path;
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    r.error = "cannot open";
    return r;
  }
  char magic[12] = {};
  in.read(magic, 12);
  auto has = [&](const char* s, size_t n) {
    return std::memcmp(magic, s, n) == 0;
  };
  // Very light container sniff — real build uses FFmpeg avformat
  if (has("\x00\x00\x00", 3) || has("ftyp", 4) || (magic[4]=='f'&&magic[5]=='t'&&magic[6]=='y'&&magic[7]=='p')) {
    r.container = "mp4/mov";
    r.ok = true;
    MediaStreamInfo v;
    v.type = "video"; v.codec = "unknown"; v.width = 1920; v.height = 1080; v.fps = 24;
    v.duration_ms = 10000;
    r.streams.push_back(v);
    MediaStreamInfo a;
    a.type = "audio"; a.codec = "unknown"; a.sample_rate = 48000; a.channels = 2;
    r.streams.push_back(a);
    r.duration_ms = 10000;
  } else if (has("\x89PNG", 4)) {
    r.container = "png"; r.ok = true;
    MediaStreamInfo v; v.type = "video"; v.codec = "png"; v.width = 0; v.height = 0;
    r.streams.push_back(v);
  } else if ((uint8_t)magic[0]==0xFF && (uint8_t)magic[1]==0xD8) {
    r.container = "jpeg"; r.ok = true;
    MediaStreamInfo v; v.type = "video"; v.codec = "jpeg";
    r.streams.push_back(v);
  } else if (has("RIFF", 4)) {
    r.container = "wav/avi"; r.ok = true;
  } else {
    r.container = "unknown";
    r.ok = true; // still allow import as generic
    MediaStreamInfo v; v.type = "video"; v.codec = "raw"; v.width = 1920; v.height = 1080; v.fps = 30;
    r.streams.push_back(v);
    r.duration_ms = 5000;
  }
  return r;
}

Error SlateDecoder::open(const std::string& path) {
  info_ = MediaProbe::probe(path);
  if (!info_.ok && !info_.error.empty()) {
    return Error::make(ErrorCode::MediaOpenFailed, info_.error, path);
  }
  info_.path = path;
  if (info_.duration_ms <= 0) info_.duration_ms = 10000;
  if (info_.streams.empty()) {
    MediaStreamInfo v; v.type = "video"; v.width = 1920; v.height = 1080; v.fps = 30;
    info_.streams.push_back(v);
  }
  open_ = true;
  pos_ms_ = 0;
  return Error::success();
}

void SlateDecoder::close() { open_ = false; }

Error SlateDecoder::seek_ms(int64_t ms) {
  if (!open_) return Error::make(ErrorCode::NotInitialized, "decoder closed");
  pos_ms_ = std::max<int64_t>(0, ms);
  return Error::success();
}

Error SlateDecoder::read_video(ImageBuffer& out, int64_t& pts_ms) {
  if (!open_) return Error::make(ErrorCode::NotInitialized, "decoder closed");
  int w = 640, h = 360;
  for (const auto& s : info_.streams) {
    if (s.type == "video" && s.width > 0) { w = s.width; h = s.height; break; }
  }
  // Cap slate size for preview
  if (w > 1280) { h = h * 1280 / w; w = 1280; }
  if (!out.allocate(w, h, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "slate frame");
  // Time-based slate color
  float t = (pos_ms_ % 10000) / 10000.f;
  float r = 0.15f + 0.2f * t;
  float g = 0.18f;
  float b = 0.35f + 0.3f * (1.f - t);
  out.clear(r, g, b, 1.f);
  pts_ms = pos_ms_;
  pos_ms_ += 33; // ~30fps
  return Error::success();
}

Error SlateDecoder::read_audio(AudioBuffer& out, int64_t& pts_ms) {
  out.sample_rate = 48000;
  out.channels = 2;
  out.samples.assign(48000 / 30 * 2, 0.f); // one frame silence
  pts_ms = pos_ms_;
  return Error::success();
}

} // namespace ms
