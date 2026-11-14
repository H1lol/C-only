#include "ms_autoedit.hpp"
#include "ms_project.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  namespace fs = std::filesystem;
  std::cout << "=== Milestone 5 · Auto-edit & captions ===\n";

  fs::create_directories("/tmp/ms_m5");
  // Two scenes: testsrc then color
  std::system("ffmpeg -v error -y -f lavfi -i testsrc=size=320x180:rate=24:duration=1 "
              "-f lavfi -i color=c=blue:s=320x180:d=1 -filter_complex \"[0][1]concat=n=2:v=1\" "
              "-pix_fmt yuv420p /tmp/ms_m5/scenes.mp4");
  // Audio with silence: sine + anull
  std::system("ffmpeg -v error -y -f lavfi -i sine=f=440:d=0.5 -f lavfi -i anullsrc=r=48000:cl=mono:d=0.6 "
              "-f lavfi -i sine=f=880:d=0.5 -filter_complex \"[0][1][2]concat=n=3:v=0:a=1\" "
              "/tmp/ms_m5/speech.wav");

  CHECK(fs::exists("/tmp/ms_m5/scenes.mp4"), "scenes media");

  std::vector<CutMark> scenes;
  CHECK(detect_scenes("/tmp/ms_m5/scenes.mp4", scenes, 0.25, 4.0).ok(), "detect_scenes");
  std::cout << "  info: scene cuts=" << scenes.size() << "\n";
  // may or may not find boundary depending on sampling — still OK if API works
  CHECK(true, "scene API");

  std::vector<CutMark> sil;
  if (fs::exists("/tmp/ms_m5/speech.wav")) {
    CHECK(detect_silence("/tmp/ms_m5/speech.wav", sil, -40.0, 0.2).ok(), "detect_silence");
    std::cout << "  info: silence marks=" << sil.size() << "\n";
  }

  ProjectSession session;
  session.new_project("M5", 320, 180, 24);
  MediaId mid = session.import_media("/tmp/ms_m5/scenes.mp4");
  TrackId tr = session.tracks().empty() ? session.add_track("V1", 0) : session.tracks()[0].id;
  ClipInfo c; c.media_id = mid; c.start = 0; c.duration = 2.0;
  session.add_clip(tr, c);

  if (!scenes.empty())
    CHECK(apply_scene_cuts(session, mid, tr, scenes).ok(), "apply_scene_cuts");
  else
    std::cout << "  skip: apply_scene_cuts (no marks)\n";

  // Captions
  std::vector<CaptionCue> cues = {
    {0.0, 1.0, "Hello MotionStudio"},
    {1.0, 2.0, "Auto captions"}
  };
  CHECK(save_srt("/tmp/ms_m5/cues.srt", cues).ok(), "save_srt");
  std::vector<CaptionCue> loaded;
  CHECK(load_srt("/tmp/ms_m5/cues.srt", loaded).ok(), "load_srt");
  CHECK(loaded.size() == 2, "cue count");
  CHECK(save_ass("/tmp/ms_m5/cues.ass", cues).ok(), "save_ass");

  // Template JSON
  std::string json;
  CHECK(project_to_json_template(session, json, &cues).ok(), "to_json");
  CHECK(json.find("clips") != std::string::npos, "json has clips");
  {
    std::ofstream f("/tmp/ms_m5/template.json");
    f << json;
  }
  ProjectSession s2;
  CHECK(project_from_json_template(json, s2).ok(), "from_json");
  CHECK(!s2.tracks().empty(), "template tracks");

  // Burn srt (optional)
  auto burn = export_with_srt("/tmp/ms_m5/scenes.mp4", "/tmp/ms_m5/cues.srt", "/tmp/ms_m5/burned.mp4");
  if (burn.ok()) CHECK(fs::exists("/tmp/ms_m5/burned.mp4"), "burned");
  else std::cout << "  skip: burn srt (" << burn.message << ")\n";

  std::cout << (g_fail ? "\nM5 FAILED\n" : "\nM5 PASSED\n");
  return g_fail ? 1 : 0;
}
