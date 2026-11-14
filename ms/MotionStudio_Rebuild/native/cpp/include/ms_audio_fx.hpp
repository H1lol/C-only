#pragma once

#include "ms_media.hpp"
#include "ms_error.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ms {

// Motion Studio's native, stateful audio insert/master effects.
// Parameters are deliberately plain data so Flutter/FFI can serialize them later.
enum class AudioFxKind : uint8_t { Gain, ParametricEq, Compressor, Delay, Reverb, Limiter };

struct AudioFxParams {
  AudioFxKind kind = AudioFxKind::Gain;
  bool enabled = true;
  float wet = 1.0f;
  float gain_db = 0.0f;

  // Three-band EQ (low shelf / mid bell / high shelf).
  float low_gain_db = 0.0f;
  float low_freq_hz = 100.0f;
  float mid_gain_db = 0.0f;
  float mid_freq_hz = 1000.0f;
  float mid_q = 1.0f;
  float high_gain_db = 0.0f;
  float high_freq_hz = 8000.0f;

  // Feed-forward compressor.
  float threshold_db = -18.0f;
  float ratio = 4.0f;
  float attack_ms = 10.0f;
  float release_ms = 100.0f;
  float makeup_db = 0.0f;

  // Delay / feedback reverb.
  float delay_ms = 280.0f;
  float feedback = 0.25f;
  float room_size = 0.72f;
  float damping = 0.35f;
  float ceiling_db = -1.0f;
};

class AudioEffectChain {
public:
  std::size_t add(const AudioFxParams& params);
  bool remove(std::size_t index);
  bool set(std::size_t index, const AudioFxParams& params);
  std::vector<AudioFxParams> parameters() const;
  void clear();
  void reset();
  Error process(AudioBuffer& buffer);
  Error process(float* interleaved, std::size_t frames, int sample_rate, int channels);

private:
  struct BiquadState { float z1 = 0.0f, z2 = 0.0f; };
  struct Coefficients { float b0=1, b1=0, b2=0, a1=0, a2=0; };
  struct CombState { std::vector<float> data; std::size_t pos=0; float filter=0; };
  struct Entry {
    AudioFxParams params;
    int rate=0, channels=0;
    float envelope=0.0f;
    std::vector<Coefficients> eq_coeffs; // 3 bands
    std::vector<BiquadState> eq_state;   // band-major, then channel
    std::vector<float> delay_line;
    std::size_t delay_pos=0;
    std::vector<CombState> combs; // channel-major, four combs per channel
  };

  static float db_to_linear(float db);
  static Coefficients low_shelf(float gain_db, float freq, int rate);
  static Coefficients peak(float gain_db, float freq, float q, int rate);
  static Coefficients high_shelf(float gain_db, float freq, int rate);
  static float run_biquad(float x, const Coefficients& c, BiquadState& s);
  void prepare(Entry& entry, int sample_rate, int channels);
  void process_entry(Entry& entry, float* data, std::size_t frames, int rate, int channels);

  std::vector<Entry> entries_;
};

} // namespace ms
