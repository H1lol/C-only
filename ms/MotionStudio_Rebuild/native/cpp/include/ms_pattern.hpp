#pragma once
/** LMMS-inspired pattern / note regions for audio timeline */
#include "ms_types.hpp"
#include "ms_audio.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>

namespace ms {

struct PatternNote {
  int step = 0;       // 0..steps-1
  int pitch = 60;     // MIDI
  float velocity = 0.8f;
  int length_steps = 1;
};

struct Pattern {
  std::string name = "Pattern";
  int steps = 16;
  float bpm = 120.f;
  std::vector<PatternNote> notes;

  TimeSec duration_sec() const {
    if (bpm < 1.f) return 1.0;
    return (steps / 4.0) * (60.0 / bpm); // 4 steps = 1 beat
  }
};

/** Render pattern as soft square blips into AudioBuffer */
Error render_pattern(const Pattern& pat, AudioBuffer& out, int sample_rate = 48000);

} // namespace ms
