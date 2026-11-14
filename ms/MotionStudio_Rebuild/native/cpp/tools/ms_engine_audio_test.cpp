#include "bridge/ms_bridge.h"
#include "ms_engine.hpp"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

// End-to-end check for the Engine -> TimelineAudioPlayback -> C ABI route:
// the host pulls interleaved PCM for the same playhead video renders at.
static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while (0)

static double energy(const std::vector<float>& v) {
  double e = 0;
  for (float s : v) e += static_cast<double>(s) * s;
  return e;
}

int main() {
  using namespace ms;
  namespace fs = std::filesystem;
  std::cout << "=== Engine timeline audio (bridge) ===\n";

  fs::create_directories("/tmp/ms_engine_audio");
  const std::string tone = "/tmp/ms_engine_audio/tone.wav";
  int gen = std::system(
      "ffmpeg -v error -y -f lavfi -i sine=frequency=440:sample_rate=48000:duration=1 "
      "-c:a pcm_s16le /tmp/ms_engine_audio/tone.wav");
  if (gen != 0 || !fs::exists(tone)) {
    std::cout << "SKIP: ffmpeg unavailable, cannot generate audio fixture\n";
    return 0;
  }

  CHECK(ms_engine_init(160, 90) == 0, "bridge engine init");

  Engine& e = Engine::instance();
  MediaId mid = e.project().import_media(tone);
  CHECK(mid != 0, "import media");
  ClipInfo clip;
  clip.media_id = mid;
  clip.start = 0.0;
  clip.duration = 0.8;
  clip.source_in = 0.0;
  clip.source_out = 0.8;
  clip.name = "tone";
  clip.motion.ensure_defaults();
  ClipId cid = e.project().add_clip(e.project().tracks()[0].id, clip);
  CHECK(cid != 0, "add clip");

  ms_play();
  MSAudioState st = ms_audio_state();
  CHECK(st.is_playing == 1, "bridge reports audio playing");
  CHECK(st.sample_rate == 48000 && st.channels == 2, "audio format 48k stereo");
  const TimeSec seq_end = e.project().sequence_end();
  CHECK(seq_end >= 0.8, "sequence covers the clip");
  CHECK(std::abs(st.duration - seq_end) < 0.001, "audio duration follows sequence end");

  std::vector<float> pcm(960 * 2, 0.f);
  CHECK(ms_audio_read_pcm(pcm.data(), 960, 2) == 0, "bridge read pcm");
  double full = energy(pcm);
  CHECK(full > 0.01, "engine mix has energy");

  CHECK(ms_audio_set_master_gain(-20.f) == 0, "set master gain");
  std::vector<float> quiet(960 * 2, 0.f);
  CHECK(ms_audio_read_pcm(quiet.data(), 960, 2) == 0, "read pcm through master gain");
  CHECK(energy(quiet) < full * 0.05, "master gain attenuates the engine mix");
  CHECK(ms_audio_set_master_gain(0.f) == 0, "reset master gain");

  ms_seek(0.3);
  CHECK(std::abs(ms_audio_state().time - 0.3) < 0.001, "audio playhead follows bridge seek");
  CHECK(std::abs(e.state().time - 0.3) < 0.001, "video playhead follows bridge seek");

  ms_play();
  CHECK(e.render_frame(), "render frame while playing");
  CHECK(std::abs(ms_audio_state().time - e.state().time) < 0.05,
        "audio clock tracks the video render tick");

  ms_pause();
  CHECK(ms_audio_state().is_playing == 0, "bridge reports audio paused");
  std::vector<float> paused(960 * 2, 1.f);
  CHECK(ms_audio_read_pcm(paused.data(), 960, 2) == 0, "read pcm while paused");
  CHECK(energy(paused) == 0.0, "paused audio is silent");

  CHECK(ms_audio_read_pcm(nullptr, 960, 2) != 0, "null pcm rejected");

  e.shutdown();
  std::cout << (g_fail ? "\nENGINE AUDIO FAILED\n" : "\nENGINE AUDIO PASSED\n");
  return g_fail ? 1 : 0;
}
