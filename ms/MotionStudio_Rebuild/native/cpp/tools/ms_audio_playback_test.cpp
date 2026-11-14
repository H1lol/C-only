#include "ms_audio_playback.hpp"
#include "ms_project.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <cmath>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  namespace fs = std::filesystem;
  std::cout << "=== FFmpeg audio playback ===\n";

  fs::create_directories("/tmp/ms_aud");
  // 1s sine wav via ffmpeg
  int gen = std::system(
    "ffmpeg -v error -y -f lavfi -i sine=frequency=440:sample_rate=48000:duration=1 "
    "-c:a pcm_s16le /tmp/ms_aud/tone.wav");
  CHECK(gen == 0 && fs::exists("/tmp/ms_aud/tone.wav"), "gen tone.wav");

  // also a/v file
  int gen2 = std::system(
    "ffmpeg -v error -y -f lavfi -i testsrc=size=160x90:rate=24 "
    "-f lavfi -i sine=frequency=880:duration=1 -t 1 "
    "-c:v libx264 -c:a aac -shortest /tmp/ms_aud/av.mp4");
  CHECK(gen2 == 0, "gen av.mp4");

  AudioDecoderFFmpeg dec;
  CHECK(dec.open("/tmp/ms_aud/tone.wav").ok(), "decoder open");
  CHECK(dec.decode_all(5).ok(), "decode_all");
  CHECK(dec.buffer().samples.size() > 1000, "pcm samples");
  std::cout << "  info: samples=" << dec.buffer().samples.size()
            << " rate=" << dec.info().sample_rate
            << " dur=" << dec.info().duration << "\n";

  // energy check - sine should not be all silence
  double energy = 0;
  for (float s : dec.buffer().samples) energy += s * s;
  CHECK(energy > 1.0, "non-silent pcm");

  AudioPlayback play;
  CHECK(play.open("/tmp/ms_aud/tone.wav").ok(), "playback open");
  CHECK(play.play().ok(), "play");
  std::vector<float> pcm(480 * 2); // 10ms
  CHECK(play.read_pcm(pcm.data(), 480, 2).ok(), "read_pcm");
  double e2 = 0;
  for (float s : pcm) e2 += s * s;
  CHECK(e2 > 0.01, "playing has energy");

  play.advance(0.5);
  CHECK(play.info().time > 0.4, "time advanced");
  play.seek(0.1);
  CHECK(std::abs(play.info().time - 0.1) < 0.001, "seek");
  play.pause();
  CHECK(!play.info().playing, "paused");
  play.stop();
  CHECK(play.info().time < 0.001, "stopped");

  // window decode on av.mp4
  AudioDecoderFFmpeg d2;
  CHECK(d2.open("/tmp/ms_aud/av.mp4").ok(), "open av");
  AudioBuffer win;
  CHECK(d2.decode_window(0.1, 0.2, win).ok(), "decode_window");
  CHECK(win.samples.size() > 100, "window samples");

  // Timeline audio
  ProjectSession session;
  session.new_project("Aud", 160, 90, 24);
  MediaId mid = session.import_media("/tmp/ms_aud/tone.wav");
  ClipInfo c; c.media_id = mid; c.duration = 0.8; c.name = "tone";
  session.add_clip(session.tracks()[0].id, c);

  TimelineAudioPlayback dry_tl(&session);
  CHECK(dry_tl.play().ok(), "dry timeline audio play");
  std::vector<float> dry_mix(960 * 2);
  CHECK(dry_tl.read_pcm(dry_mix.data(), 960, 2).ok(), "dry timeline read");
  double dry_energy = 0;
  for (float s : dry_mix) dry_energy += s * s;
  CHECK(dry_energy > 0.01, "dry timeline mix energy");

  TimelineAudioPlayback tl(&session);
  CHECK(tl.play().ok(), "timeline audio play");
  AudioFxParams insert_gain; insert_gain.kind = AudioFxKind::Gain; insert_gain.gain_db = -6.0f;
  tl.effects().add(insert_gain);
  std::vector<float> mix(960 * 2);
  CHECK(tl.read_pcm(mix.data(), 960, 2).ok(), "timeline read with insert chain");
  double e3 = 0;
  for (float s : mix) e3 += s * s;
  CHECK(e3 > 0.01, "timeline mix energy");
  CHECK(e3 < dry_energy * 0.35, "timeline insert chain changes playback level");
  tl.advance(0.2);
  CHECK(tl.info().time > 0.15, "timeline time");

  std::cout << (g_fail ? "\nAUDIO PLAYBACK FAILED\n" : "\nAUDIO PLAYBACK PASSED\n");
  return g_fail ? 1 : 0;
}
