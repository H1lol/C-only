#include "ms_pattern.hpp"
#include <cmath>
#include <algorithm>

namespace ms {

Error render_pattern(const Pattern& pat, AudioBuffer& out, int sample_rate) {
  if (sample_rate <= 0 || pat.steps <= 0)
    return Error::make(ErrorCode::InvalidArgument, "pattern");
  double dur = pat.duration_sec();
  int n = std::max(1, static_cast<int>(dur * sample_rate));
  out.sample_rate = sample_rate;
  out.channels = 2;
  out.samples.assign(static_cast<size_t>(n) * 2, 0.f);

  double step_dur = dur / pat.steps;
  for (const auto& note : pat.notes) {
    if (note.step < 0 || note.step >= pat.steps) continue;
    double freq = 440.0 * std::pow(2.0, (note.pitch - 69) / 12.0);
    int start = static_cast<int>(note.step * step_dur * sample_rate);
    int len = static_cast<int>(note.length_steps * step_dur * sample_rate);
    float amp = std::clamp(note.velocity, 0.f, 1.f) * 0.2f;
    for (int i = 0; i < len && start + i < n; ++i) {
      float env = 1.f - static_cast<float>(i) / std::max(1, len);
      float s = amp * env * ((std::sin(2 * 3.14159265 * freq * i / sample_rate) > 0) ? 1.f : -1.f);
      out.samples[static_cast<size_t>(start + i) * 2 + 0] += s;
      out.samples[static_cast<size_t>(start + i) * 2 + 1] += s;
    }
  }
  for (float& s : out.samples) s = std::clamp(s, -1.f, 1.f);
  return Error::success();
}

} // namespace ms
