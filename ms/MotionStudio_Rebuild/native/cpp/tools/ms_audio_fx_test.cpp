#include "ms_audio.hpp"
#include "ms_audio_playback.hpp"
#include <fstream>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <algorithm>

static int fails = 0;

static void write_u16(std::ofstream& out, std::uint16_t v) {
  const char b[2] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff)};
  out.write(b, 2);
}
static void write_u32(std::ofstream& out, std::uint32_t v) {
  const char b[4] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff),
                     static_cast<char>((v >> 16) & 0xff), static_cast<char>((v >> 24) & 0xff)};
  out.write(b, 4);
}
static bool make_test_wav(const std::string& path) {
  constexpr int rate = 48000, channels = 2, seconds = 1;
  constexpr std::uint32_t frames = rate * seconds;
  constexpr std::uint16_t bits = 16;
  constexpr std::uint32_t data_bytes = frames * channels * (bits / 8);
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  out.write("RIFF", 4); write_u32(out, 36 + data_bytes); out.write("WAVE", 4);
  out.write("fmt ", 4); write_u32(out, 16); write_u16(out, 1); write_u16(out, channels);
  write_u32(out, rate); write_u32(out, rate * channels * (bits / 8));
  write_u16(out, channels * (bits / 8)); write_u16(out, bits);
  out.write("data", 4); write_u32(out, data_bytes);
  for (std::uint32_t i = 0; i < frames; ++i) {
    const auto sample = static_cast<std::int16_t>(0.25 * 32767.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * i / rate));
    write_u16(out, static_cast<std::uint16_t>(sample));
    write_u16(out, static_cast<std::uint16_t>(sample));
  }
  return static_cast<bool>(out);
}
#define CHECK(x, name) do { if (!(x)) { std::cerr << "FAIL " << name << "\n"; ++fails; } else std::cout << "ok " << name << "\n"; } while (0)

int main() {
  using namespace ms;
  AudioBuffer b; b.sample_rate = 48000; b.channels = 2; b.samples.resize(4800 * 2, 0.0f);
  for (int i = 0; i < 4800; ++i) {
    const float s = 0.8f * std::sin(2.0 * 3.141592653589793 * 1000.0 * i / 48000.0);
    b.samples[i*2] = s; b.samples[i*2+1] = s;
  }
  AudioEffectChain pass; CHECK(pass.process(b).ok(), "empty chain passes valid buffer");
  float peak_before = 0; for (float v : b.samples) peak_before = std::max(peak_before, std::abs(v));

  AudioFxParams comp; comp.kind = AudioFxKind::Compressor; comp.threshold_db = -24; comp.ratio = 8; comp.attack_ms = 1; comp.release_ms = 80;
  AudioEffectChain dynamics; dynamics.add(comp);
  CHECK(dynamics.process(b).ok(), "compressor processes audio");
  float peak_after = 0; for (float v : b.samples) peak_after = std::max(peak_after, std::abs(v));
  CHECK(peak_after < peak_before, "compressor reduces peaks");

  AudioBuffer impulse; impulse.sample_rate = 48000; impulse.channels = 2; impulse.samples.assign(48000 * 2, 0.f); impulse.samples[0]=1.f; impulse.samples[1]=1.f;
  AudioFxParams delay; delay.kind = AudioFxKind::Delay; delay.delay_ms = 100; delay.feedback = 0; delay.wet = 1;
  AudioEffectChain echo; echo.add(delay); CHECK(echo.process(impulse).ok(), "delay processes audio");
  const std::size_t echo_frame = 4800 * 2;
  CHECK(impulse.samples[echo_frame] > 0.5f, "delay writes a 100ms echo");

  AudioBuffer reverb; reverb.sample_rate = 48000; reverb.channels = 2; reverb.samples.assign(48000 * 2, 0.f); reverb.samples[0]=1.f; reverb.samples[1]=1.f;
  AudioFxParams rv; rv.kind = AudioFxKind::Reverb; rv.wet = 1; rv.room_size = .8f;
  AudioEffectChain room; room.add(rv); CHECK(room.process(reverb).ok(), "reverb processes audio");
  bool tail = false; for (std::size_t i=2; i<reverb.samples.size(); ++i) if (std::abs(reverb.samples[i]) > 1e-5f) { tail=true; break; }
  CHECK(tail, "reverb produces a decaying tail");

  // Updating a live effect must rebuild its derived coefficients/state.
  AudioEffectChain editable_eq;
  AudioFxParams eq; eq.kind = AudioFxKind::ParametricEq;
  const auto eq_slot = editable_eq.add(eq);
  AudioBuffer eq_warmup; eq_warmup.sample_rate = 48000; eq_warmup.channels = 1;
  eq_warmup.samples.assign(6000, 0.0f);
  for (std::size_t i = 0; i < eq_warmup.samples.size(); ++i)
    eq_warmup.samples[i] = 0.2f * std::sin(2.0 * 3.141592653589793 * 50.0 * i / 48000.0);
  CHECK(editable_eq.process(eq_warmup).ok(), "EQ initializes before parameter edit");
  eq.low_gain_db = 12.0f;
  CHECK(editable_eq.set(eq_slot, eq), "EQ parameters can be updated in place");
  AudioBuffer eq_after; eq_after.sample_rate = 48000; eq_after.channels = 1;
  eq_after.samples.assign(6000, 0.0f);
  for (std::size_t i = 0; i < eq_after.samples.size(); ++i)
    eq_after.samples[i] = 0.2f * std::sin(2.0 * 3.141592653589793 * 50.0 * i / 48000.0);
  CHECK(editable_eq.process(eq_after).ok(), "updated EQ processes next block");
  double eq_rms = 0.0;
  for (std::size_t i = 1000; i < eq_after.samples.size(); ++i) eq_rms += eq_after.samples[i] * eq_after.samples[i];
  eq_rms = std::sqrt(eq_rms / static_cast<double>(eq_after.samples.size() - 1000));
  CHECK(eq_rms > 0.22, "updated low shelf coefficients take effect");

  AudioMixer mixer;
  AudioFxParams limiter; limiter.kind = AudioFxKind::Limiter; limiter.ceiling_db = -6;
  mixer.master_effects().add(limiter);
  AudioBuffer hot; hot.sample_rate=48000; hot.channels=2; hot.samples.assign(128, 1.0f);
  CHECK(mixer.master_effects().process(hot).ok(), "master insert chain is available on mixer");
  CHECK(std::abs(hot.samples[0]) < 0.51f, "master limiter applies ceiling");
  const auto track_id = mixer.add_track("Tone test");
  AudioClip tone; tone.start = 0; tone.duration = 0.1; tone.gain = 1.f;
  mixer.add_clip(track_id, tone);
  AudioBuffer mixed;
  CHECK(mixer.mix(0, 0.1, mixed, 48000, 2).ok(), "mixer runs its master insert chain");
  float mixed_peak = 0; for (float v : mixed.samples) mixed_peak = std::max(mixed_peak, std::abs(v));
  CHECK(mixed_peak <= 0.501f, "mixer master limiter constrains mixed output");

  // Exercise the same effect chain from the decoded timeline playback path.
  const std::string test_wav = "/tmp/ms_audio_fx_timeline_playback.wav";
  if (std::system("ffmpeg -version >/dev/null 2>&1") == 0 && make_test_wav(test_wav)) {
    ProjectSession session;
    CHECK(session.new_project("Audio FX Playback", 1920, 1080, 30).ok(), "audio playback test project created");
    const auto media_id = session.import_media(test_wav);
    const auto audio_track = session.add_track("Audio", static_cast<int>(TrackType::Audio));
    ClipInfo clip; clip.media_id = media_id; clip.start = 0; clip.duration = 0.8; clip.source_in = 0; clip.source_out = 0.8;
    CHECK(session.add_clip(audio_track, clip) != 0, "audio playback test clip inserted");
    TimelineAudioPlayback timeline_audio(&session);
    CHECK(timeline_audio.play().ok(), "timeline audio playback starts");
    AudioFxParams playback_gain; playback_gain.kind = AudioFxKind::Gain; playback_gain.gain_db = -12.0f;
    timeline_audio.effects().add(playback_gain);
    std::vector<float> pcm(4800 * 2, 0.f);
    CHECK(timeline_audio.read_pcm(pcm.data(), 4800, 2).ok(), "timeline audio playback processes PCM");
    float playback_peak = 0.f; for (float v : pcm) playback_peak = std::max(playback_peak, std::abs(v));
    CHECK(playback_peak > 0.01f && playback_peak < 0.1f, "timeline audio insert gain changes decoded playback");
    std::remove(test_wav.c_str());
  } else {
    std::cout << "SKIP timeline playback integration (FFmpeg unavailable)\n";
  }

  AudioBuffer invalid; invalid.sample_rate=0; invalid.channels=2;
  CHECK(!pass.process(invalid).ok(), "invalid sample rate rejected");
  std::cout << (fails ? "AUDIO FX TEST FAILED\n" : "AUDIO FX TEST PASSED\n");
  return fails ? 1 : 0;
}
