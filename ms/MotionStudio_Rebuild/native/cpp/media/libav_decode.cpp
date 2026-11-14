#include "ms_libav_decode.hpp"
#include "ms_ffmpeg_io.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>

#if defined(MS_HAVE_LIBAV)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
#endif

namespace ms {

bool libav_available() {
#if defined(MS_HAVE_LIBAV)
  return true;
#else
  return false;
#endif
}

LibavDecoder::LibavDecoder() = default;

LibavDecoder::~LibavDecoder() {
  stop_prefetch();
  close();
}

void LibavDecoder::set_prefetch(int frames) {
  prefetch_n_ = std::clamp(frames, 0, 64);
}

PrefetchStats LibavDecoder::stats() const {
  std::lock_guard<std::mutex> lock(cache_mu_);
  PrefetchStats s = stats_;
  s.cache_size = static_cast<int>(cache_.size());
  return s;
}

#if defined(MS_HAVE_LIBAV)

Error LibavDecoder::open_internal(const std::string& path) {
  close_internal();
  AVFormatContext* fmt = nullptr;
  if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0)
    return Error::make(ErrorCode::MediaOpenFailed, "avformat_open_input", path);
  if (avformat_find_stream_info(fmt, nullptr) < 0) {
    avformat_close_input(&fmt);
    return Error::make(ErrorCode::MediaProbeFailed, "stream info", path);
  }
  int vstream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  if (vstream < 0) {
    avformat_close_input(&fmt);
    return Error::make(ErrorCode::MediaUnsupportedFormat, "no video", path);
  }
  AVStream* st = fmt->streams[vstream];
  const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
  if (!codec) {
    avformat_close_input(&fmt);
    return Error::make(ErrorCode::MediaUnsupportedFormat, "no decoder");
  }
  AVCodecContext* cctx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(cctx, st->codecpar);
  // Desktop software; on Android set cctx->get_format / hw_device_ctx for MediaCodec
  if (avcodec_open2(cctx, codec, nullptr) < 0) {
    avcodec_free_context(&cctx);
    avformat_close_input(&fmt);
    return Error::make(ErrorCode::MediaDecodeFailed, "avcodec_open2");
  }

  fmt_ctx_ = fmt;
  codec_ctx_ = cctx;
  video_stream_ = vstream;
  frame_ = av_frame_alloc();
  packet_ = av_packet_alloc();

  info_.ok = true;
  info_.path = path;
  if (fmt->duration > 0)
    info_.duration_ms = fmt->duration / (AV_TIME_BASE / 1000);
  MediaStreamInfo si;
  si.type = "video";
  si.codec = codec->name ? codec->name : "";
  si.width = cctx->width;
  si.height = cctx->height;
  si.index = vstream;
  if (st->avg_frame_rate.den)
    si.fps = av_q2d(st->avg_frame_rate);
  else if (st->r_frame_rate.den)
    si.fps = av_q2d(st->r_frame_rate);
  fps_ = si.fps > 1.0 ? si.fps : 24.0;
  si.duration_ms = info_.duration_ms;
  info_.streams.push_back(si);
  path_ = path;
  open_ = true;
  pos_ms_ = 0;
  return Error::success();
}

void LibavDecoder::close_internal() {
  if (sws_) { sws_freeContext(static_cast<SwsContext*>(sws_)); sws_ = nullptr; }
  if (frame_) { av_frame_free(reinterpret_cast<AVFrame**>(&frame_)); }
  if (packet_) { av_packet_free(reinterpret_cast<AVPacket**>(&packet_)); }
  if (codec_ctx_) {
    auto* c = static_cast<AVCodecContext*>(codec_ctx_);
    avcodec_free_context(&c);
    codec_ctx_ = nullptr;
  }
  if (fmt_ctx_) {
    auto* f = static_cast<AVFormatContext*>(fmt_ctx_);
    avformat_close_input(&f);
    fmt_ctx_ = nullptr;
  }
  video_stream_ = -1;
  open_ = false;
}

Error LibavDecoder::decode_at(int64_t ms, ImageBuffer& out, int64_t& pts_ms) {
  if (!open_) return Error::make(ErrorCode::NotInitialized, "closed");
  std::lock_guard<std::mutex> dlock(decode_mu_);
  auto* fmt = static_cast<AVFormatContext*>(fmt_ctx_);
  auto* cctx = static_cast<AVCodecContext*>(codec_ctx_);
  auto* frame = static_cast<AVFrame*>(frame_);
  auto* pkt = static_cast<AVPacket*>(packet_);

  AVStream* st = fmt->streams[video_stream_];
  int64_t ts = av_rescale_q(ms, {1, 1000}, st->time_base);
  if (av_seek_frame(fmt, video_stream_, ts, AVSEEK_FLAG_BACKWARD) < 0) {
    // try any stream
    av_seek_frame(fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
  }
  avcodec_flush_buffers(cctx);

  bool got = false;
  while (av_read_frame(fmt, pkt) >= 0) {
    if (pkt->stream_index != video_stream_) {
      av_packet_unref(pkt);
      continue;
    }
    if (avcodec_send_packet(cctx, pkt) < 0) {
      av_packet_unref(pkt);
      continue;
    }
    av_packet_unref(pkt);
    while (avcodec_receive_frame(cctx, frame) == 0) {
      int64_t fpts = 0;
      if (frame->best_effort_timestamp != AV_NOPTS_VALUE)
        fpts = av_rescale_q(frame->best_effort_timestamp, st->time_base, {1, 1000});
      else if (frame->pts != AV_NOPTS_VALUE)
        fpts = av_rescale_q(frame->pts, st->time_base, {1, 1000});
      if (fpts + 40 < ms) continue; // skip until near target

      int dw = out_w_ > 0 ? out_w_ : cctx->width;
      int dh = out_h_ > 0 ? out_h_ : cctx->height;
      if (dw <= 0) dw = 320;
      if (dh <= 0) dh = 180;

      SwsContext* sws = static_cast<SwsContext*>(sws_);
      sws = sws_getCachedContext(sws, cctx->width, cctx->height,
                                 static_cast<AVPixelFormat>(frame->format),
                                 dw, dh, AV_PIX_FMT_RGBA, SWS_BILINEAR,
                                 nullptr, nullptr, nullptr);
      sws_ = sws;
      if (!out.allocate(dw, dh, PixelFormat::RGBA8))
        return Error::make(ErrorCode::OutOfMemory, "frame");
      uint8_t* dst[4] = { out.data(), nullptr, nullptr, nullptr };
      int dst_stride[4] = { dw * 4, 0, 0, 0 };
      sws_scale(sws, frame->data, frame->linesize, 0, cctx->height, dst, dst_stride);
      pts_ms = fpts;
      pos_ms_ = fpts;
      got = true;
      stats_.decoded++;
      break;
    }
    if (got) break;
  }
  if (!got) return Error::make(ErrorCode::MediaDecodeFailed, "no frame");
  return Error::success();
}

#else

Error LibavDecoder::open_internal(const std::string& path) {
  // Fallback: probe via CLI path
  info_ = ffmpeg_probe(path);
  path_ = path;
  open_ = info_.ok || true;
  for (const auto& s : info_.streams)
    if (s.type == "video" && s.fps > 1) fps_ = s.fps;
  return Error::success();
}

void LibavDecoder::close_internal() { open_ = false; }

Error LibavDecoder::decode_at(int64_t ms, ImageBuffer& out, int64_t& pts_ms) {
  auto e = ffmpeg_decode_frame(path_, ms / 1000.0, out, out_w_, out_h_);
  pts_ms = ms;
  if (e.ok()) stats_.decoded++;
  return e;
}

#endif

Error LibavDecoder::open(const std::string& path) {
  stop_prefetch();
  Error e = open_internal(path);
  if (!e.ok()) return e;
  open_ = true;
  return Error::success();
}

void LibavDecoder::close() {
  stop_prefetch();
  close_internal();
  std::lock_guard<std::mutex> lock(cache_mu_);
  cache_.clear();
}

Error LibavDecoder::seek_ms(int64_t ms) {
  pos_ms_ = std::max<int64_t>(0, ms);
  target_ms_.store(pos_ms_);
  std::lock_guard<std::mutex> lock(cache_mu_);
  cache_.clear();
  return Error::success();
}

Error LibavDecoder::read_video(ImageBuffer& out, int64_t& pts_ms) {
  if (!open_) return Error::make(ErrorCode::NotInitialized, "closed");

  // Try cache
  {
    std::lock_guard<std::mutex> lock(cache_mu_);
    for (auto& c : cache_) {
      if (std::abs(c.pts_ms - pos_ms_) <= 40) {
        if (!out.allocate(c.image.width(), c.image.height(), PixelFormat::RGBA8))
          return Error::make(ErrorCode::OutOfMemory, "cache copy");
        std::memcpy(out.data(), c.image.data(),
                    static_cast<size_t>(c.image.width()) * c.image.height() * 4);
        pts_ms = c.pts_ms;
        stats_.hits++;
        pos_ms_ += static_cast<int64_t>(1000.0 / fps_);
        return Error::success();
      }
    }
    stats_.misses++;
  }

  Error e = decode_at(pos_ms_, out, pts_ms);
  if (e.ok()) {
    pos_ms_ = pts_ms + static_cast<int64_t>(1000.0 / fps_);
    target_ms_.store(pos_ms_);
  }
  return e;
}

Error LibavDecoder::read_audio(AudioBuffer& out, int64_t& pts_ms) {
  out.sample_rate = 48000;
  out.channels = 2;
  out.samples.assign(48000 / 30 * 2, 0.f);
  pts_ms = pos_ms_;
  return Error::success();
}

void LibavDecoder::start_prefetch() {
  if (prefetch_n_ <= 0 || run_) return;
  run_ = true;
  worker_ = std::thread([this] { prefetch_loop(); });
}

void LibavDecoder::stop_prefetch() {
  run_ = false;
  if (worker_.joinable()) worker_.join();
}

void LibavDecoder::prefetch_loop() {
  while (run_) {
    int64_t base = target_ms_.load();
    for (int i = 0; i < prefetch_n_ && run_; ++i) {
      int64_t want = base + static_cast<int64_t>(i * (1000.0 / fps_));
      bool have = false;
      {
        std::lock_guard<std::mutex> lock(cache_mu_);
        for (const auto& c : cache_)
          if (std::abs(c.pts_ms - want) <= 20) { have = true; break; }
      }
      if (have) continue;
      ImageBuffer img;
      int64_t pts = 0;
      if (decode_at(want, img, pts).ok()) {
        std::lock_guard<std::mutex> lock(cache_mu_);
        if (cache_.size() >= static_cast<size_t>(prefetch_n_ * 2))
          cache_.pop_front();
        CachedFrame cf;
        cf.pts_ms = pts;
        cf.image = std::move(img);
        cache_.push_back(std::move(cf));
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

Error LibavDecoder::decode_and_publish(double time_sec) {
  ImageBuffer img;
  int64_t pts = 0;
  Error e = decode_at(static_cast<int64_t>(time_sec * 1000.0), img, pts);
  if (!e.ok()) return e;
  return FrameBufferHub::instance().publish_rgba(
      img.width(), img.height(), img.data(), img.width() * 4, pts);
}

std::unique_ptr<IMediaDecoder> create_best_decoder(const std::string& path) {
  if (libav_available()) {
    auto d = std::make_unique<LibavDecoder>();
    if (d->open(path).ok()) return d;
  }
  return create_media_decoder(path);
}

// ---- LibavPlayback ----

Error LibavPlayback::open(const std::string& path, int max_w, int max_h) {
  close();
  dec_ = std::make_unique<LibavDecoder>();
  dec_->set_output_size(max_w, max_h);
  Error e = dec_->open(path);
  if (!e.ok()) return e;
  duration_ = dec_->info().duration_ms / 1000.0;
  if (duration_ <= 0) duration_ = 5.0;
  for (const auto& s : dec_->info().streams)
    if (s.type == "video" && s.fps > 1) fps_ = s.fps;
  dec_->set_prefetch(8);
  time_ = 0;
  return Error::success();
}

void LibavPlayback::close() {
  if (dec_) dec_->stop_prefetch();
  dec_.reset();
  playing_ = false;
}

Error LibavPlayback::play() {
  if (!dec_) return Error::make(ErrorCode::NotInitialized, "no media");
  playing_ = true;
  last_wall_ = std::chrono::steady_clock::now();
  clock_ = true;
  dec_->start_prefetch();
  return Error::success();
}

void LibavPlayback::pause() { playing_ = false; clock_ = false; }

void LibavPlayback::stop() {
  playing_ = false;
  time_ = 0;
  clock_ = false;
  if (dec_) dec_->seek_ms(0);
}

Error LibavPlayback::seek(double sec) {
  time_ = std::clamp(sec, 0.0, duration_);
  if (dec_) return dec_->seek_ms(static_cast<int64_t>(time_ * 1000.0));
  return Error::success();
}

bool LibavPlayback::tick() {
  if (!playing_ || !dec_) return false;
  auto now = std::chrono::steady_clock::now();
  if (!clock_) { last_wall_ = now; clock_ = true; }
  double dt = std::chrono::duration<double>(now - last_wall_).count();
  last_wall_ = now;
  if (dt > 0.25) dt = 1.0 / fps_;
  time_ += dt;
  if (time_ >= duration_) {
    time_ = duration_;
    playing_ = false;
  }
  Error e = dec_->decode_and_publish(time_);
  return e.ok();
}

PrefetchStats LibavPlayback::stats() const {
  return dec_ ? dec_->stats() : PrefetchStats{};
}

} // namespace ms
