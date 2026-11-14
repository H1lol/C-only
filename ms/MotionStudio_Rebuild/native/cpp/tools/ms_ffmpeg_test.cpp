#include "ms_ffmpeg_io.hpp"
#include "ms_timeline_player.hpp"
#include "ms_project.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  namespace fs = std::filesystem;
  std::cout << "=== FFmpeg media I/O ===\n";

  CHECK(ffmpeg_available(), "ffmpeg available");
  CHECK(ffprobe_available(), "ffprobe available");

  // Generate 1s test pattern mp4
  fs::create_directories("/tmp/ms_ff");
  int gen = std::system(
    "ffmpeg -v error -y -f lavfi -i testsrc=size=320x180:rate=24 -t 1 "
    "-pix_fmt yuv420p /tmp/ms_ff/test.mp4");
  CHECK(gen == 0 && fs::exists("/tmp/ms_ff/test.mp4"), "generate test.mp4");

  auto probe = ffmpeg_probe("/tmp/ms_ff/test.mp4");
  CHECK(probe.ok, "ffprobe ok");
  CHECK(!probe.streams.empty(), "has streams");
  bool has_v = false;
  for (const auto& s : probe.streams)
    if (s.type == "video") { has_v = true; std::cout << "  info: video " << s.width << "x" << s.height
      << " " << s.codec << " fps=" << s.fps << "\n"; }
  CHECK(has_v, "video stream");
  std::cout << "  info: duration_ms=" << probe.duration_ms << "\n";

  ImageBuffer frame;
  CHECK(ffmpeg_decode_frame("/tmp/ms_ff/test.mp4", 0.2, frame, 320, 180).ok(), "decode frame");
  CHECK(frame.valid() && frame.width() > 0, "frame valid");
  std::cout << "  info: decoded " << frame.width() << "x" << frame.height() << "\n";

  FFmpegDecoder dec;
  CHECK(dec.open("/tmp/ms_ff/test.mp4").ok(), "FFmpegDecoder open");
  dec.set_max_size(160, 90);
  int64_t pts = 0;
  ImageBuffer f2;
  CHECK(dec.seek_ms(400).ok(), "seek");
  CHECK(dec.read_video(f2, pts).ok(), "read_video");
  CHECK(f2.valid(), "read frame valid");

  auto any = create_media_decoder("/tmp/ms_ff/test.mp4");
  CHECK(any != nullptr, "factory decoder");

  // Timeline with real media
  ProjectSession session;
  session.new_project("FF", 160, 90, 24);
  MediaId mid = session.import_media("/tmp/ms_ff/test.mp4");
  // Update media duration from probe
  ClipInfo c; c.media_id = mid; c.duration = 0.5; c.name = "testsrc";
  session.add_clip(session.tracks()[0].id, c);
  TimelinePlayer player(&session);
  CHECK(player.prepare().ok(), "player prepare");
  ImageBuffer prev;
  CHECK(player.render_frame(0.1, prev).ok(), "timeline render");
  CHECK(prev.valid(), "timeline frame");

  // Extract + encode roundtrip
  CHECK(ffmpeg_extract_sequence("/tmp/ms_ff/test.mp4", "/tmp/ms_ff/seq", 0, 0.3, 12, 160).ok(),
        "extract sequence");
  int ppm_count = 0;
  for (auto& e : fs::directory_iterator("/tmp/ms_ff/seq"))
    if (e.path().extension() == ".ppm") ++ppm_count;
  CHECK(ppm_count > 0, "ppm frames extracted");
  std::cout << "  info: extracted frames=" << ppm_count << "\n";

  CHECK(ffmpeg_encode_sequence("/tmp/ms_ff/seq", "/tmp/ms_ff/out.mp4", 12.f).ok(), "encode mp4");
  CHECK(fs::exists("/tmp/ms_ff/out.mp4"), "out.mp4 exists");

  std::cout << (g_fail ? "\nFFMPEG I/O FAILED\n" : "\nFFMPEG I/O PASSED\n");
  return g_fail ? 1 : 0;
}
