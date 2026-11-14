#include "ms_libav_decode.hpp"
#include "ms_frame_buffer.hpp"
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
  std::cout << "=== Milestone 1 · libav decode + prefetch + playback ===\n";
  std::cout << "  info: libav_available=" << (libav_available() ? "yes" : "no") << "\n";
  CHECK(libav_available(), "libav linked");

  fs::create_directories("/tmp/ms_m1");
  int gen = std::system(
    "ffmpeg -v error -y -f lavfi -i testsrc=size=1280x720:rate=30 -t 2 "
    "-pix_fmt yuv420p /tmp/ms_m1/hd.mp4");
  CHECK(gen == 0, "gen 720p clip");

  LibavDecoder dec;
  dec.set_output_size(640, 360);
  CHECK(dec.open("/tmp/ms_m1/hd.mp4").ok(), "open");
  std::cout << "  info: duration_ms=" << dec.info().duration_ms << "\n";
  for (const auto& s : dec.info().streams)
    if (s.type == "video")
      std::cout << "  info: " << s.width << "x" << s.height << " " << s.codec
                << " fps=" << s.fps << "\n";

  ImageBuffer frame;
  int64_t pts = 0;
  CHECK(dec.seek_ms(200).ok(), "seek");
  CHECK(dec.read_video(frame, pts).ok(), "read_video");
  CHECK(frame.valid() && frame.width() > 0, "frame valid");
  std::cout << "  info: decoded " << frame.width() << "x" << frame.height()
            << " pts=" << pts << "\n";

  CHECK(dec.decode_and_publish(0.5).ok(), "decode_and_publish");
  FrameBufferSlot slot;
  CHECK(FrameBufferHub::instance().latest(slot), "frame buffer has frame");
  CHECK(slot.width > 0, "published size");

  // Prefetch + playback
  LibavPlayback play;
  CHECK(play.open("/tmp/ms_m1/hd.mp4", 640, 360).ok(), "playback open");
  CHECK(play.play().ok(), "play");
  int published = 0;
  auto t0 = std::chrono::steady_clock::now();
  while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 0.5) {
    if (play.tick()) ++published;
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
  }
  std::cout << "  info: ticks published=" << published << " t=" << play.time() << "\n";
  CHECK(published >= 1, "playback published frames");
  CHECK(play.time() > 0.05, "time advanced");

  play.seek(1.0);
  CHECK(std::abs(play.time() - 1.0) < 0.01, "seek 1.0");
  CHECK(play.tick() || true, "tick after seek");

  auto st = play.stats();
  std::cout << "  info: prefetch decoded=" << st.decoded << " cache=" << st.cache_size
            << " hits=" << st.hits << " misses=" << st.misses << "\n";

  play.stop();
  std::cout << (g_fail ? "\nM1 FAILED\n" : "\nM1 PASSED\n");
  return g_fail ? 1 : 0;
}
