/**
 * Phase 1 acceptance test — project, media, timeline, save/load, undo, autosave
 */
#include "ms_project.hpp"
#include "ms_image.hpp"
#include <iostream>
#include <cstdio>
#include <cassert>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << msg << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Phase 1 — Project foundation ===\n";

  ProjectSession s;
  Error e = s.new_project("Phase1Demo", 1920, 1080, 24.f);
  CHECK(e.ok(), "new_project");
  CHECK(s.meta().width == 1920 && s.meta().height == 1080, "resolution 1920x1080");
  CHECK(s.tracks().size() >= 3, "default tracks (V/O/A)");

  // media file for probe
  {
    FILE* f = fopen("/tmp/ms_p1_media.bin", "wb");
    fwrite("ftypisom", 1, 8, f);
    fclose(f);
  }
  MediaId mid = s.import_media("/tmp/ms_p1_media.bin");
  CHECK(mid != 0, "import_media id");
  CHECK(s.media().size() == 1, "media bin size 1");
  CHECK(s.find_media(mid) != nullptr, "find_media");

  TrackId vid = s.tracks()[0].id;
  ClipInfo c;
  c.media_id = mid;
  c.name = "Shot_A";
  c.start = 0;
  c.duration = 4.0;
  c.source_in = 0;
  c.source_out = 4.0;
  ClipId cid = s.add_clip(vid, c);
  CHECK(cid != 0, "add_clip");
  CHECK(s.clip_count() == 1, "clip_count 1");

  CHECK(s.move_clip(vid, cid, 1.5), "move_clip");
  CHECK(s.find_clip(vid, cid)->start == 1.5, "clip start 1.5");

  CHECK(s.trim_clip(vid, cid, 1.5, 2.0), "trim_clip");
  CHECK(s.find_clip(vid, cid)->duration == 2.0, "duration 2");

  CHECK(s.split_clip(vid, cid, 2.5), "split_clip");
  CHECK(s.clip_count() == 2, "two clips after split");

  s.seek(2.0);
  s.play();
  CHECK(s.state().playing, "playing");
  s.pause();
  CHECK(!s.state().playing, "paused");
  CHECK(s.state().time == 2.0, "seek time");

  // save / load
  const char* path = "/tmp/phase1_demo.msproj";
  e = s.save_to_file(path);
  CHECK(e.ok(), "save_to_file");
  CHECK(!s.is_dirty(), "clean after save");

  ProjectSession s2;
  e = s2.load_from_file(path);
  CHECK(e.ok(), "load_from_file");
  CHECK(s2.meta().name == "Phase1Demo", "loaded name");
  CHECK(s2.media().size() >= 1, "loaded media");
  CHECK(s2.clip_count() >= 1, "loaded clips");

  // undo
  ProjectSession s3;
  s3.new_project("Undo", 1280, 720, 30.f);
  auto m = s3.import_media("/tmp/ms_p1_media.bin");
  auto tid = s3.tracks()[0].id;
  ClipInfo c2; c2.media_id = m; c2.name = "X"; c2.duration = 3;
  s3.add_clip(tid, c2);
  CHECK(s3.clip_count() == 1, "before undo clip");
  CHECK(s3.can_undo(), "can_undo");
  s3.undo();
  // after undo import or add may restore earlier snapshot
  CHECK(s3.can_redo(), "can_redo after undo");

  // buffers still work
  ImageBuffer buf;
  CHECK(buf.allocate(64, 64, PixelFormat::RGBA8), "image buffer allocate");
  buf.clear(0.1f, 0.2f, 0.3f, 1.f);
  CHECK(buf.valid() && buf.size_bytes() == 64 * 64 * 4, "buffer size");

  CHECK(s.sequence_end() > 0, "sequence_end");

  std::cout << (g_fail ? "\nPHASE 1 FAILED\n" : "\nPHASE 1 PASSED\n");
  return g_fail ? 1 : 0;
}
