#include "ms_audio_fx.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ms {
namespace {
constexpr float kPi = 3.14159265358979323846f;
inline float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
}

float AudioEffectChain::db_to_linear(float db) { return std::pow(10.0f, db / 20.0f); }

AudioEffectChain::Coefficients AudioEffectChain::low_shelf(float db, float f, int rate) {
  const float A = std::pow(10.0f, db / 40.0f);
  const float w = 2.0f * kPi * clampf(f, 10.0f, rate * 0.45f) / static_cast<float>(rate);
  const float c = std::cos(w), s = std::sin(w), alpha = s * 0.5f * std::sqrt(2.0f);
  const float beta = 2.0f * std::sqrt(A) * alpha;
  const float a0 = (A + 1.0f) + (A - 1.0f) * c + beta;
  Coefficients q;
  q.b0 = A * ((A + 1.0f) - (A - 1.0f) * c + beta) / a0;
  q.b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * c) / a0;
  q.b2 = A * ((A + 1.0f) - (A - 1.0f) * c - beta) / a0;
  q.a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * c) / a0;
  q.a2 = ((A + 1.0f) + (A - 1.0f) * c - beta) / a0;
  return q;
}

AudioEffectChain::Coefficients AudioEffectChain::peak(float db, float f, float qv, int rate) {
  const float A = std::pow(10.0f, db / 40.0f);
  const float w = 2.0f * kPi * clampf(f, 10.0f, rate * 0.45f) / static_cast<float>(rate);
  const float alpha = std::sin(w) / (2.0f * clampf(qv, 0.05f, 20.0f));
  const float a0 = 1.0f + alpha / A;
  Coefficients c;
  c.b0 = (1.0f + alpha * A) / a0;
  c.b1 = (-2.0f * std::cos(w)) / a0;
  c.b2 = (1.0f - alpha * A) / a0;
  c.a1 = (-2.0f * std::cos(w)) / a0;
  c.a2 = (1.0f - alpha / A) / a0;
  return c;
}

AudioEffectChain::Coefficients AudioEffectChain::high_shelf(float db, float f, int rate) {
  const float A = std::pow(10.0f, db / 40.0f);
  const float w = 2.0f * kPi * clampf(f, 10.0f, rate * 0.45f) / static_cast<float>(rate);
  const float c = std::cos(w), s = std::sin(w), alpha = s * 0.5f * std::sqrt(2.0f);
  const float beta = 2.0f * std::sqrt(A) * alpha;
  const float a0 = (A + 1.0f) - (A - 1.0f) * c + beta;
  Coefficients q;
  q.b0 = A * ((A + 1.0f) + (A - 1.0f) * c + beta) / a0;
  q.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * c) / a0;
  q.b2 = A * ((A + 1.0f) + (A - 1.0f) * c - beta) / a0;
  q.a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * c) / a0;
  q.a2 = ((A + 1.0f) - (A - 1.0f) * c - beta) / a0;
  return q;
}

float AudioEffectChain::run_biquad(float x, const Coefficients& c, BiquadState& s) {
  const float y = c.b0 * x + s.z1;
  s.z1 = c.b1 * x - c.a1 * y + s.z2;
  s.z2 = c.b2 * x - c.a2 * y;
  return y;
}

std::size_t AudioEffectChain::add(const AudioFxParams& params) {
  Entry e; e.params = params; entries_.push_back(std::move(e)); return entries_.size() - 1;
}
bool AudioEffectChain::remove(std::size_t index) {
  if (index >= entries_.size()) return false;
  entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index)); return true;
}
bool AudioEffectChain::set(std::size_t index, const AudioFxParams& params) {
  if (index >= entries_.size()) return false;
  auto& entry = entries_[index];
  if (entry.params.kind != params.kind) {
    entry = Entry{};
  } else {
    // Parameter changes can invalidate derived DSP state (notably EQ
    // coefficients and delay/reverb buffers). Force prepare() to rebuild it
    // on the next block so the new settings take effect immediately.
    entry.rate = 0;
    entry.channels = 0;
  }
  entry.params = params;
  return true;
}
std::vector<AudioFxParams> AudioEffectChain::parameters() const {
  std::vector<AudioFxParams> out; out.reserve(entries_.size());
  for (const auto& e : entries_) out.push_back(e.params);
  return out;
}
void AudioEffectChain::clear() { entries_.clear(); }
void AudioEffectChain::reset() {
  for (auto& e : entries_) { e.envelope = 0; e.delay_pos = 0; std::fill(e.delay_line.begin(), e.delay_line.end(), 0.f); for (auto& c : e.combs) { c.pos = 0; c.filter = 0; std::fill(c.data.begin(), c.data.end(), 0.f); } std::fill(e.eq_state.begin(), e.eq_state.end(), BiquadState{}); }
}

void AudioEffectChain::prepare(Entry& e, int rate, int channels) {
  if (e.rate == rate && e.channels == channels) return;
  e.rate = rate; e.channels = channels; e.envelope = 0;
  e.eq_coeffs = {low_shelf(e.params.low_gain_db, e.params.low_freq_hz, rate),
                 peak(e.params.mid_gain_db, e.params.mid_freq_hz, e.params.mid_q, rate),
                 high_shelf(e.params.high_gain_db, e.params.high_freq_hz, rate)};
  e.eq_state.assign(static_cast<std::size_t>(channels) * 3, BiquadState{});
  e.delay_line.clear(); e.delay_pos = 0;
  e.combs.clear();
  if (e.params.kind == AudioFxKind::Delay) {
    const std::size_t cap = std::max<std::size_t>(2, static_cast<std::size_t>(rate * 2.0));
    e.delay_line.assign(cap * static_cast<std::size_t>(channels), 0.f);
  }
  if (e.params.kind == AudioFxKind::Reverb) {
    // Schroeder-style four-comb network; delays are scaled from 44.1 kHz tunings.
    const int base[4] = {1116, 1188, 1277, 1356};
    e.combs.resize(static_cast<std::size_t>(channels) * 4);
    for (int ch = 0; ch < channels; ++ch) for (int k = 0; k < 4; ++k) {
      auto& c = e.combs[static_cast<std::size_t>(ch) * 4 + k];
      const int spread = ch == 0 ? 0 : 23;
      const auto len = std::max<std::size_t>(1, static_cast<std::size_t>((base[k] + spread) * rate / 44100.0));
      c.data.assign(len, 0.f);
    }
  }
}

void AudioEffectChain::process_entry(Entry& e, float* data, std::size_t frames, int rate, int channels) {
  const auto& p = e.params;
  if (!p.enabled || frames == 0) return;
  prepare(e, rate, channels);
  const float wet = clampf(p.wet, 0.f, 1.f);
  switch (p.kind) {
    case AudioFxKind::Gain: {
      const float g = db_to_linear(clampf(p.gain_db, -96.f, 24.f));
      for (std::size_t i = 0; i < frames * static_cast<std::size_t>(channels); ++i) data[i] *= g;
      break;
    }
    case AudioFxKind::ParametricEq: {
      const bool active[3] = {std::abs(p.low_gain_db) > 1e-4f, std::abs(p.mid_gain_db) > 1e-4f, std::abs(p.high_gain_db) > 1e-4f};
      for (std::size_t f = 0; f < frames; ++f) for (int ch = 0; ch < channels; ++ch) {
        const std::size_t idx = f * static_cast<std::size_t>(channels) + ch;
        float x = data[idx], y = x;
        for (int b = 0; b < 3; ++b) if (active[b]) y = run_biquad(y, e.eq_coeffs[b], e.eq_state[static_cast<std::size_t>(b) * channels + ch]);
        data[idx] = x * (1.f - wet) + y * wet;
      }
      break;
    }
    case AudioFxKind::Compressor: {
      const float ratio = std::max(1.f, p.ratio);
      const float attack = std::exp(-1.f / (std::max(0.01f, p.attack_ms) * 0.001f * rate));
      const float release = std::exp(-1.f / (std::max(1.f, p.release_ms) * 0.001f * rate));
      const float makeup = db_to_linear(clampf(p.makeup_db, -24.f, 24.f));
      const float slope = 1.f - 1.f / ratio;
      for (std::size_t f = 0; f < frames; ++f) {
        const std::size_t base = f * static_cast<std::size_t>(channels);
        float peak = 0.f;
        for (int ch = 0; ch < channels; ++ch) peak = std::max(peak, std::abs(data[base + ch]));
        const float coeff = peak > e.envelope ? attack : release;
        e.envelope = coeff * e.envelope + (1.f - coeff) * peak;
        float gain = makeup;
        if (e.envelope > 1e-9f) {
          const float over = 20.f * std::log10(e.envelope) - p.threshold_db;
          if (over > 0.f) gain = db_to_linear(p.makeup_db - over * slope);
        }
        gain = 1.f + (gain - 1.f) * wet;
        for (int ch = 0; ch < channels; ++ch) data[base + ch] *= gain;
      }
      break;
    }
    case AudioFxKind::Delay: {
      const std::size_t cap = e.delay_line.size() / static_cast<std::size_t>(channels);
      const std::size_t delay = std::min(cap - 1, std::max<std::size_t>(1, static_cast<std::size_t>(clampf(p.delay_ms, 1.f, 2000.f) * 0.001f * rate)));
      const float fb = clampf(p.feedback, 0.f, 0.92f);
      for (std::size_t f = 0; f < frames; ++f) {
        const std::size_t read_pos = (e.delay_pos + cap - delay) % cap;
        for (int ch = 0; ch < channels; ++ch) {
          const std::size_t idx = f * static_cast<std::size_t>(channels) + ch;
          const std::size_t ri = read_pos * static_cast<std::size_t>(channels) + ch;
          const std::size_t wi = e.delay_pos * static_cast<std::size_t>(channels) + ch;
          const float dry = data[idx], delayed = e.delay_line[ri];
          e.delay_line[wi] = dry + delayed * fb;
          data[idx] = dry * (1.f - wet) + delayed * wet;
        }
        e.delay_pos = (e.delay_pos + 1) % cap;
      }
      break;
    }
    case AudioFxKind::Reverb: {
      const float feedback = clampf(0.35f + p.room_size * 0.58f, 0.35f, 0.93f);
      const float damp = clampf(p.damping, 0.f, 0.95f);
      for (std::size_t f = 0; f < frames; ++f) for (int ch = 0; ch < channels; ++ch) {
        const std::size_t idx = f * static_cast<std::size_t>(channels) + ch;
        const float dry = data[idx]; float sum = 0.f;
        for (int k = 0; k < 4; ++k) {
          auto& c = e.combs[static_cast<std::size_t>(ch) * 4 + k];
          const float delayed = c.data[c.pos];
          c.filter = delayed * (1.f - damp) + c.filter * damp;
          c.data[c.pos] = dry + c.filter * feedback;
          c.pos = (c.pos + 1) % c.data.size();
          sum += delayed;
        }
        const float wet_signal = sum * 0.22f;
        data[idx] = dry * (1.f - wet) + wet_signal * wet;
      }
      break;
    }
    case AudioFxKind::Limiter: {
      const float ceiling = db_to_linear(clampf(p.ceiling_db, -24.f, 0.f));
      for (std::size_t i = 0; i < frames * static_cast<std::size_t>(channels); ++i)
        data[i] = clampf(data[i], -ceiling, ceiling);
      break;
    }
  }
}

Error AudioEffectChain::process(AudioBuffer& buffer) {
  if (buffer.sample_rate <= 0 || buffer.channels <= 0 || buffer.samples.size() % static_cast<std::size_t>(buffer.channels) != 0)
    return Error::make(ErrorCode::InvalidArgument, "invalid audio buffer");
  return process(buffer.samples.data(), buffer.samples.size() / static_cast<std::size_t>(buffer.channels), buffer.sample_rate, buffer.channels);
}

Error AudioEffectChain::process(float* data, std::size_t frames, int rate, int channels) {
  if ((!data && frames) || rate <= 0 || channels <= 0 || channels > 32)
    return Error::make(ErrorCode::InvalidArgument, "invalid audio effect input");
  for (auto& e : entries_) process_entry(e, data, frames, rate, channels);
  return Error::success();
}

} // namespace ms
