#include "ms_mlt_timeline.hpp"
#include "ms_project.hpp"
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
  std::cout << "=== Milestone 2 · MLT timeline engine ===\n";

  MltTimeline tl;
  CHECK(tl.available(), "MLT available");

  fs::create_directories("/tmp/ms_m2");
  std::system("ffmpeg -v error -y -f lavfi -i testsrc=size=320x180:rate=24 -t 2 "
              "-pix_fmt yuv420p /tmp/ms_m2/a.mp4");
  std::system("ffmpeg -v error -y -f lavfi -i testsrc=size=320x180:rate=24 -t 1 "
              "-pix_fmt yuv420p /tmp/ms_m2/b.mp4");

  ProjectSession session;
  session.new_project("M2", 320, 180, 24);
  MediaId ma = session.import_media("/tmp/ms_m2/a.mp4");
  MediaId mb = session.import_media("/tmp/ms_m2/b.mp4");
  CHECK(ma != 0 && mb != 0, "import media");

  TrackId track = session.tracks().empty() ? session.add_track("V1", 0) : session.tracks()[0].id;

  ClipInfo c1;
  c1.media_id = ma;
  c1.start = 0;
  c1.duration = 1.0;
  c1.name = "A";
  CHECK(tl.insert_clip(session, track, c1).ok(), "insert A");

  ClipInfo c2;
  c2.media_id = mb;
  c2.start = 0.5;
  c2.duration = 0.8;
  c2.name = "B";
  CHECK(tl.overwrite_clip(session, track, c2).ok(), "overwrite B");

  CHECK(tl.info().built, "tractor built");
  std::cout << "  info: tracks=" << tl.info().track_count
            << " clips=" << tl.info().clip_count
            << " dur=" << tl.info().duration_sec << "\n";

  CHECK(tl.render_frame(0.2, 320, 180).ok(), "render frame");
  FrameBufferSlot slot;
  CHECK(FrameBufferHub::instance().latest(slot) && slot.valid(), "frame in hub");

  std::string xml = "/tmp/ms_m2/project.mlt";
  CHECK(tl.save_xml(xml).ok(), "save MLT XML");
  CHECK(fs::exists(xml) && fs::file_size(xml) > 50, "xml file size");
  std::cout << "  info: xml bytes=" << fs::file_size(xml) << "\n";

  // Split first remaining clip
  ClipId first = 0;
  for (const auto& t : session.tracks())
    if (t.id == track && !t.clips.empty()) { first = t.clips[0].id; break; }
  if (first) {
    auto* tr = session.find_track(track);
    TimeSec mid = tr->clips[0].start + tr->clips[0].duration * 0.5;
    CHECK(tl.split_clip(session, track, first, mid).ok(), "split");
  } else {
    std::cout << "  skip: split (no clip id)\n";
  }

  // Ripple delete last clip if any
  auto* tr = session.find_track(track);
  if (tr && !tr->clips.empty()) {
    ClipId last = tr->clips.back().id;
    CHECK(tl.ripple_delete(session, track, last).ok(), "ripple delete");
  }

  // Reload XML into new timeline
  MltTimeline tl2;
  CHECK(tl2.load_xml(xml).ok(), "load XML");
  CHECK(tl2.render_frame(0.0, 320, 180).ok(), "render after load");

  // Project JSON still works
  CHECK(session.save_to_file("/tmp/ms_m2/session.msproj").ok(), "save msproj");
  ProjectSession s2;
  CHECK(s2.load_from_file("/tmp/ms_m2/session.msproj").ok(), "load msproj");

  std::cout << (g_fail ? "\nM2 FAILED\n" : "\nM2 PASSED\n");
  return g_fail ? 1 : 0;
}
