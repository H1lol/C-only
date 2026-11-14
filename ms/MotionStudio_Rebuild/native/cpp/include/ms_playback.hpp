#pragma once
/**
 * Video playback engine — FFmpeg-backed continuous play
 */
#include "ms_ffmpeg_io.hpp"
#include "ms_timeline_player.hpp"
#include "ms_project.hpp"
#include "ms_image.hpp"
#include "ms_error.hpp"
#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>

namespace ms {

enum class PlaybackState : uint8_t {
  Stopped = 0,
  Playing,
  Paused,
};

struct PlaybackInfo {
  PlaybackState state = PlaybackState::Stopped;
  TimeSec time = 0;
  TimeSec duration = 0;
  float fps = 24.f;
  bool looping = false;
  int frame_index = 0;
  int dropped_frames = 0;
};

/**
 * Single-file player: open media, play/pause, pull frames by wall clock
 */
class MediaPlayer {
public:
  explicit MediaPlayer(FFmpegPaths paths = {});

  Error open(const std::string& path);
  void close();

  Error play();
  void pause();
  void stop();
  Error seek(TimeSec t);

  /** Advance clock; if a new frame is due, decode into out and return true */
  bool tick(ImageBuffer& out);

  /** Force decode current time into out */
  Error current_frame(ImageBuffer& out);

  PlaybackInfo info() const;
  void set_looping(bool v) { looping_ = v; }
  void set_rate(float rate) { rate_ = rate <= 0 ? 1.f : rate; }

private:
  FFmpegPaths paths_;
  std::unique_ptr<IMediaDecoder> decoder_;
  MediaProbeResult probe_;
  std::string path_;
  PlaybackState state_ = PlaybackState::Stopped;
  TimeSec time_ = 0;
  TimeSec duration_ = 0;
  float fps_ = 24.f;
  float rate_ = 1.f;
  bool looping_ = false;
  int frame_index_ = 0;
  int dropped_ = 0;
  std::chrono::steady_clock::time_point last_wall_{};
  bool clock_running_ = false;
};

/**
 * Project timeline playback — drives TimelinePlayer with wall clock
 */
class TimelinePlayback {
public:
  explicit TimelinePlayback(ProjectSession* session = nullptr);

  void set_session(ProjectSession* s);
  Error prepare();

  Error play();
  void pause();
  void stop();
  Error seek(TimeSec t);

  bool tick(ImageBuffer& out);
  Error current_frame(ImageBuffer& out);

  PlaybackInfo info() const;
  void set_looping(bool v) { looping_ = v; }
  void set_in_out(TimeSec in, TimeSec out) { in_ = in; out_ = out; }

private:
  ProjectSession* session_ = nullptr;
  TimelinePlayer player_;
  PlaybackState state_ = PlaybackState::Stopped;
  TimeSec time_ = 0;
  TimeSec in_ = 0;
  TimeSec out_ = 0; // 0 = sequence end
  float fps_ = 24.f;
  bool looping_ = false;
  std::chrono::steady_clock::time_point last_wall_{};
  bool clock_running_ = false;
  int frame_index_ = 0;
};

} // namespace ms
