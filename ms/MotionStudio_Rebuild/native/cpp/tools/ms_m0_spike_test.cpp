#include "ms_mlt_engine.hpp"
#include "ms_frame_buffer.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  namespace fs = std::filesystem;
  std::cout << "=== Milestone 0 · MLT / frame buffer spike ===\n";
  std::cout << "  info: mlt_available=" << (mlt_available() ? "yes" : "no") << "\n";

  FrameBufferHub::instance().clear();

  // G0a: solid always
  auto solid = mlt_publish_solid(320, 180, 45, 140, 230);
  CHECK(solid.ok, "solid publish");
  CHECK(ms_frame_buffer_publish_count() >= 1, "C ABI publish_count");

  FrameBufferSlot slot;
  CHECK(FrameBufferHub::instance().latest(slot), "latest after solid");
  CHECK(slot.width == 320 && slot.height == 180, "solid size");
  CHECK(slot.rgba.size() >= 320 * 180 * 4u, "rgba bytes");

  // G0b: MLT colour producer or file
  fs::create_directories("/tmp/ms_m0");
  std::system("ffmpeg -v error -y -f lavfi -i testsrc=size=320x180:rate=24 -t 0.5 "
              "-pix_fmt yuv420p /tmp/ms_m0/clip.mp4");

  auto r1 = mlt_render_one_frame("color:blue", 0.0, 320, 180);
  std::cout << "  info: backend=" << r1.backend << " detail=" << r1.detail
            << " " << r1.width << "x" << r1.height << "\n";
  CHECK(r1.ok, "render colour/producer");
  CHECK(r1.width > 0 && r1.height > 0, "frame size");

  CHECK(FrameBufferHub::instance().latest(slot), "latest after mlt/ffmpeg");
  CHECK(slot.valid(), "slot valid");

  // G0c: media file path
  auto r2 = mlt_render_one_frame("/tmp/ms_m0/clip.mp4", 0.1, 320, 180);
  std::cout << "  info: file backend=" << r2.backend << " detail=" << r2.detail << "\n";
  CHECK(r2.ok, "render from file");

  // C ABI publish already used; latest via C
  int w = 0, h = 0;
  long long pts = 0;
  std::vector<unsigned char> buf(320 * 180 * 4);
  CHECK(ms_frame_buffer_latest(&w, &h, &pts, buf.data(), (int)buf.size()) == 0, "C ABI latest");
  CHECK(w > 0 && h > 0, "C ABI size");

  std::cout << "  G0: frame in ms_frame_buffer_publish_rgba — "
            << (g_fail ? "FAIL" : "PASS") << "\n";
  std::cout << (g_fail ? "\nM0 SPIKE FAILED\n" : "\nM0 SPIKE PASSED\n");
  return g_fail ? 1 : 0;
}
