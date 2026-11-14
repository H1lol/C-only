#include "ms_playback.hpp"
#include <algorithm>
#include <cmath>

namespace ms {

MediaPlayer::MediaPlayer(FFmpegPaths paths) : paths_(std::move(paths)) {}

Error MediaPlayer::open(const std::string& path) {
  close();
  path_ = path;
  probe_ = ffmpeg_probe(path, paths_);
  decoder_ = create_media_decoder(path, paths_);
  if (!decoder_)
    return Error::make(ErrorCode::MediaOpenFailed, "no decoder", path);

  duration_ = probe_.duration_ms > 0 ? probe_.duration_ms / 1000.0 : 5.0;
  fps_ = 24.f;
  for (const auto& s : probe_.streams)
    if (s.type == "video" && s.fps > 1.0) { fps_ = static_cast<float>(s.fps); break; }
  if (fps_ < 1.f) fps_ = 24.f;
  time_ = 0;
  state_ = PlaybackState::Stopped;
  frame_index_ = 0;
  dropped_ = 0;
  clock_running_ = false;
  return Error::success();
}

void MediaPlayer::close() {
  if (decoder_) decoder_->close();
  decoder_.reset();
  state_ = PlaybackState::Stopped;
  clock_running_ = false;
}

Error MediaPlayer::play() {
  if (!decoder_) return Error::make(ErrorCode::NotInitialized, "no media");
  state_ = PlaybackState::Playing;
  last_wall_ = std::chrono::steady_clock::now();
  clock_running_ = true;
  return Error::success();
}

void MediaPlayer::pause() {
  if (state_ == PlaybackState::Playing) state_ = PlaybackState::Paused;
  clock_running_ = false;
}

void MediaPlayer::stop() {
  state_ = PlaybackState::Stopped;
  time_ = 0;
  frame_index_ = 0;
  clock_running_ = false;
  if (decoder_) decoder_->seek_ms(0);
}

Error MediaPlayer::seek(TimeSec t) {
  if (!decoder_) return Error::make(ErrorCode::NotInitialized, "no media");
  time_ = std::clamp(t, 0.0, duration_);
  frame_index_ = static_cast<int>(time_ * fps_);
  return decoder_->seek_ms(static_cast<int64_t>(time_ * 1000.0));
}

Error MediaPlayer::current_frame(ImageBuffer& out) {
  if (!decoder_) return Error::make(ErrorCode::NotInitialized, "no media");
  decoder_->seek_ms(static_cast<int64_t>(time_ * 1000.0));
  int64_t pts = 0;
  return decoder_->read_video(out, pts);
}

bool MediaPlayer::tick(ImageBuffer& out) {
  if (state_ != PlaybackState::Playing || !decoder_) return false;

  auto now = std::chrono::steady_clock::now();
  if (!clock_running_) {
    last_wall_ = now;
    clock_running_ = true;
  }
  double dt = std::chrono::duration<double>(now - last_wall_).count();
  last_wall_ = now;

  // Cap huge dt (debugger pause)
  if (dt > 0.25) dt = 1.0 / fps_;
  time_ += dt * rate_;

  if (time_ >= duration_) {
    if (looping_) {
      time_ = 0;
      frame_index_ = 0;
      decoder_->seek_ms(0);
    } else {
      time_ = duration_;
      state_ = PlaybackState::Stopped;
      clock_running_ = false;
    }
  }

  int target = static_cast<int>(time_ * fps_);
  if (target < frame_index_) frame_index_ = target;
  if (target > frame_index_ + 1) dropped_ += (target - frame_index_ - 1);
  frame_index_ = target;

  decoder_->seek_ms(static_cast<int64_t>(time_ * 1000.0));
  int64_t pts = 0;
  Error e = decoder_->read_video(out, pts);
  return e.ok() && out.valid();
}

PlaybackInfo MediaPlayer::info() const {
  PlaybackInfo i;
  i.state = state_;
  i.time = time_;
  i.duration = duration_;
  i.fps = fps_;
  i.looping = looping_;
  i.frame_index = frame_index_;
  i.dropped_frames = dropped_;
  return i;
}

// ---- TimelinePlayback ----

TimelinePlayback::TimelinePlayback(ProjectSession* session)
    : session_(session), player_(session) {}

void TimelinePlayback::set_session(ProjectSession* s) {
  session_ = s;
  player_.set_session(s);
}

Error TimelinePlayback::prepare() {
  if (!session_) return Error::make(ErrorCode::NotInitialized, "no session");
  fps_ = session_->meta().fps > 1 ? session_->meta().fps : 24.f;
  if (out_ <= in_) out_ = session_->sequence_end();
  if (out_ <= in_) out_ = 5.0;
  return player_.prepare();
}

Error TimelinePlayback::play() {
  Error e = prepare();
  if (!e.ok()) return e;
  state_ = PlaybackState::Playing;
  last_wall_ = std::chrono::steady_clock::now();
  clock_running_ = true;
  return Error::success();
}

void TimelinePlayback::pause() {
  if (state_ == PlaybackState::Playing) state_ = PlaybackState::Paused;
  clock_running_ = false;
}

void TimelinePlayback::stop() {
  state_ = PlaybackState::Stopped;
  time_ = in_;
  frame_index_ = 0;
  clock_running_ = false;
}

Error TimelinePlayback::seek(TimeSec t) {
  time_ = std::clamp(t, in_, out_ > in_ ? out_ : t);
  frame_index_ = static_cast<int>(time_ * fps_);
  return Error::success();
}

Error TimelinePlayback::current_frame(ImageBuffer& out) {
  return player_.render_frame(time_, out);
}

bool TimelinePlayback::tick(ImageBuffer& out) {
  if (state_ != PlaybackState::Playing || !session_) return false;
  auto now = std::chrono::steady_clock::now();
  if (!clock_running_) {
    last_wall_ = now;
    clock_running_ = true;
  }
  double dt = std::chrono::duration<double>(now - last_wall_).count();
  last_wall_ = now;
  if (dt > 0.25) dt = 1.0 / fps_;
  time_ += dt;

  TimeSec end = out_ > in_ ? out_ : session_->sequence_end();
  if (end <= in_) end = in_ + 1;
  if (time_ >= end) {
    if (looping_) time_ = in_;
    else {
      time_ = end;
      state_ = PlaybackState::Stopped;
      clock_running_ = false;
    }
  }
  frame_index_ = static_cast<int>(time_ * fps_);
  Error e = player_.render_frame(time_, out);
  return e.ok() && out.valid();
}

PlaybackInfo TimelinePlayback::info() const {
  PlaybackInfo i;
  i.state = state_;
  i.time = time_;
  i.duration = out_ > in_ ? out_ - in_ : (session_ ? session_->sequence_end() : 0);
  i.fps = fps_;
  i.looping = looping_;
  i.frame_index = frame_index_;
  return i;
}

} // namespace ms
