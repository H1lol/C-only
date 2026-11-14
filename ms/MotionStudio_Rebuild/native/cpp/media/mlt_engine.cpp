#include "ms_mlt_engine.hpp"
#include "ms_ffmpeg_io.hpp"
#include <vector>
#include <cstring>
#include <cmath>

#if defined(MS_HAVE_MLT)
#include <framework/mlt.h>
#endif

namespace ms {

bool mlt_available() {
#if defined(MS_HAVE_MLT)
  return true;
#else
  return false;
#endif
}

MltSpikeResult mlt_publish_solid(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  MltSpikeResult res;
  res.backend = "solid";
  if (w <= 0 || h <= 0) {
    res.detail = "bad size";
    return res;
  }
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
  for (int i = 0; i < w * h; ++i) {
    rgba[i * 4 + 0] = r;
    rgba[i * 4 + 1] = g;
    rgba[i * 4 + 2] = b;
    rgba[i * 4 + 3] = 255;
  }
  auto e = FrameBufferHub::instance().publish_rgba(w, h, rgba.data(), w * 4, 0);
  res.ok = e.ok();
  res.width = w;
  res.height = h;
  res.detail = e.ok() ? "solid published" : e.message;
  return res;
}

#if defined(MS_HAVE_MLT)

MltSpikeResult mlt_render_one_frame(const std::string& source, double time_sec,
                                    int out_w, int out_h) {
  MltSpikeResult res;
  res.backend = "mlt";
  static bool factory_ok = false;
  if (!factory_ok) {
    if (mlt_factory_init(nullptr) == nullptr) {
      res.detail = "mlt_factory_init failed";
      return res;
    }
    factory_ok = true;
  }

  mlt_profile profile = mlt_profile_init(nullptr);
  if (!profile) {
    res.detail = "no profile";
    return res;
  }
  // Prefer 320x180-ish for spike
  if (out_w > 0) profile->width = out_w;
  if (out_h > 0) profile->height = out_h;
  profile->progressive = 1;

  std::string src = source.empty() ? "color:red" : source;
  mlt_producer producer = mlt_factory_producer(profile, nullptr, src.c_str());
  if (!producer) {
    // try colour: variant
    producer = mlt_factory_producer(profile, "color", "red");
  }
  if (!producer) {
    mlt_profile_close(profile);
    res.detail = "producer failed: " + src;
    // fallback solid
    auto s = mlt_publish_solid(out_w > 0 ? out_w : 320, out_h > 0 ? out_h : 180, 200, 40, 40);
    s.detail = res.detail + " → solid fallback";
    return s;
  }

  // Seek
  double fps = mlt_profile_fps(profile);
  if (fps < 1.0) fps = 25.0;
  mlt_position pos = static_cast<mlt_position>(time_sec * fps);
  mlt_producer_seek(producer, pos);

  mlt_service service = mlt_producer_service(producer);
  mlt_frame frame = nullptr;
  int err = mlt_service_get_frame(service, &frame, 0);
  if (err != 0 || !frame) {
    mlt_producer_close(producer);
    mlt_profile_close(profile);
    res.detail = "get_frame failed";
    return mlt_publish_solid(out_w, out_h, 40, 40, 200);
  }

  int w = out_w > 0 ? out_w : profile->width;
  int h = out_h > 0 ? out_h : profile->height;
  mlt_image_format fmt = mlt_image_rgba;
  uint8_t* image = nullptr;
  int error = mlt_frame_get_image(frame, &image, &fmt, &w, &h, 0);
  if (error != 0 || !image || w <= 0 || h <= 0) {
    mlt_frame_close(frame);
    mlt_producer_close(producer);
    mlt_profile_close(profile);
    res.detail = "get_image failed";
    return mlt_publish_solid(320, 180, 40, 200, 40);
  }

  int stride = w * 4;
  auto pub = FrameBufferHub::instance().publish_rgba(w, h, image, stride,
                                                     static_cast<int64_t>(time_sec * 1000.0));
  res.ok = pub.ok();
  res.width = w;
  res.height = h;
  res.pts_ms = static_cast<int64_t>(time_sec * 1000.0);
  res.detail = pub.ok() ? "mlt frame published" : pub.message;

  mlt_frame_close(frame);
  mlt_producer_close(producer);
  mlt_profile_close(profile);
  return res;
}

#else

MltSpikeResult mlt_render_one_frame(const std::string& source, double time_sec,
                                    int out_w, int out_h) {
  MltSpikeResult res;
  res.backend = "ffmpeg_cli_fallback";
  if (out_w <= 0) out_w = 320;
  if (out_h <= 0) out_h = 180;

  // Prefer FFmpeg decode if file exists
  if (!source.empty() && source.find(':') == std::string::npos) {
    ImageBuffer img;
    auto e = ffmpeg_decode_frame(source, time_sec, img, out_w, out_h);
    if (e.ok() && img.valid()) {
      auto p = FrameBufferHub::instance().publish_rgba(
          img.width(), img.height(), img.data(), img.width() * 4,
          static_cast<int64_t>(time_sec * 1000.0));
      res.ok = p.ok();
      res.width = img.width();
      res.height = img.height();
      res.pts_ms = static_cast<int64_t>(time_sec * 1000.0);
      res.detail = "ffmpeg_cli frame published";
      return res;
    }
    res.detail = "ffmpeg decode failed: " + e.message;
  }

  auto s = mlt_publish_solid(out_w, out_h, 80, 80, 200);
  s.backend = "solid";
  s.detail = (res.detail.empty() ? "no mlt" : res.detail) + " → solid";
  return s;
}

#endif

} // namespace ms
