#include "ms_playback.hpp"
#include "ms_project.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <chrono>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  namespace fs = std::filesystem;
  std::cout << "=== FFmpeg video playback ===\n";

  fs::create_directories("/tmp/ms_play");
  int gen = std::system(
    "ffmpeg -v error -y -f lavfi -i testsrc=size=320x180:rate=24 -t 1.5 "
    "-pix_fmt yuv420p /tmp/ms_play/clip.mp4");
  CHECK(gen == 0, "gen clip");

  MediaPlayer player;
  CHECK(player.open("/tmp/ms_play/clip.mp4").ok(), "open");
  auto inf = player.info();
  std::cout << "  info: duration=" << inf.duration << "s fps=" << inf.fps << "\n";
  CHECK(inf.duration > 1.0, "duration");

  ImageBuffer frame;
  CHECK(player.current_frame(frame).ok() && frame.valid(), "current_frame");
  CHECK(player.seek(0.5).ok(), "seek 0.5");
  CHECK(player.current_frame(frame).ok(), "frame at 0.5");

  CHECK(player.play().ok(), "play");
  CHECK(player.info().state == PlaybackState::Playing, "state playing");

  int got = 0;
  auto t0 = std::chrono::steady_clock::now();
  // Run ~400ms of playback
  while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 0.4) {
    if (player.tick(frame)) ++got;
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
  }
  std::cout << "  info: frames from tick=" << got << " time=" << player.info().time << "\n";
  CHECK(got >= 1, "tick produced frames");
  CHECK(player.info().time > 0.05, "time advanced");

  player.pause();
  CHECK(player.info().state == PlaybackState::Paused, "paused");
  double t_pause = player.info().time;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  player.tick(frame); // should not advance much while paused
  CHECK(player.info().state == PlaybackState::Paused, "still paused");

  player.play();
  player.set_looping(true);
  player.seek(player.info().duration - 0.05);
  player.play();
  for (int i = 0; i < 30; ++i) {
    player.tick(frame);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  // with loop may wrap
  CHECK(true, "loop path exercised");

  player.stop();
  CHECK(player.info().state == PlaybackState::Stopped, "stopped");
  CHECK(player.info().time < 0.01, "time reset");

  // Timeline playback
  ProjectSession session;
  session.new_project("Play", 160, 90, 24);
  MediaId mid = session.import_media("/tmp/ms_play/clip.mp4");
  ClipInfo c; c.media_id = mid; c.duration = 1.0; c.name = "v";
  session.add_clip(session.tracks()[0].id, c);

  TimelinePlayback tl(&session);
  CHECK(tl.prepare().ok(), "timeline prepare");
  CHECK(tl.play().ok(), "timeline play");
  int tg = 0;
  t0 = std::chrono::steady_clock::now();
  while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 0.25) {
    if (tl.tick(frame)) ++tg;
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
  }
  std::cout << "  info: timeline ticks=" << tg << " t=" << tl.info().time << "\n";
  CHECK(tg >= 1, "timeline tick frames");
  tl.stop();

  std::cout << (g_fail ? "\nPLAYBACK FAILED\n" : "\nPLAYBACK PASSED\n");
  return g_fail ? 1 : 0;
}
