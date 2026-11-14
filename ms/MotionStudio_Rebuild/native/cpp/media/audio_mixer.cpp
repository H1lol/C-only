#include "ms_audio.hpp"
#include "ms_audio_playback.hpp"
#include <cmath>
#include <algorithm>

namespace ms {

uint64_t AudioMixer::add_track(const std::string& name) {
  AudioTrack t;
  t.id = next_track_++;
  t.name = name;
  tracks_.push_back(t);
  return tracks_.back().id;
}

AudioTrack* AudioMixer::find_track(uint64_t id) {
  for (auto& t : tracks_) if (t.id == id) return &t;
  return nullptr;
}

uint64_t AudioMixer::add_clip(uint64_t track_id, const AudioClip& clip_in) {
  AudioTrack* t = find_track(track_id);
  if (!t) return 0;
  AudioClip c = clip_in;
  if (c.id == 0) c.id = next_clip_++;
  t->clips.push_back(c);
  return c.id;
}

Error AudioMixer::mix(TimeSec time, TimeSec duration, AudioBuffer& out,
                      int sample_rate, int channels) {
  if (duration <= 0 || sample_rate <= 0 || channels <= 0)
    return Error::make(ErrorCode::InvalidArgument, "mix args");
  const int nframes = std::max(1, static_cast<int>(duration * sample_rate));
  out.sample_rate = sample_rate;
  out.channels = channels;
  out.samples.assign(static_cast<size_t>(nframes) * channels, 0.f);

  bool any_solo = false;
  for (const auto& t : tracks_) if (t.solo) any_solo = true;

  for (const auto& track : tracks_) {
    if (track.mute) continue;
    if (any_solo && !track.solo) continue;
    for (const auto& clip : track.clips) {
      if (clip.mute) continue;
      TimeSec c0 = clip.start;
      TimeSec c1 = clip.start + clip.duration;
      TimeSec w0 = time;
      TimeSec w1 = time + duration;
      if (c1 <= w0 || c0 >= w1) continue;

      // Placeholder: soft tone scaled by gain (real path: decode audio from media)
      float g = clip.gain * track.gain;
      float pan = std::clamp(clip.pan, -1.f, 1.f);
      float gl = g * (1.f - std::max(0.f, pan));
      float gr = g * (1.f + std::min(0.f, pan));
      if (channels == 1) { gl = gr = g; }

      for (int i = 0; i < nframes; ++i) {
        double tsec = time + (double)i / sample_rate;
        if (tsec < c0 || tsec >= c1) continue;
        double local = tsec - c0 + clip.source_in;
        float sample = 0.1f * std::sin(2.f * 3.14159265f * 440.f * static_cast<float>(local));
        out.samples[static_cast<size_t>(i) * channels + 0] += sample * gl;
        if (channels > 1)
          out.samples[static_cast<size_t>(i) * channels + 1] += sample * gr;
      }
    }
  }

  // Master inserts run after track summing, matching a conventional DAW master bus.
  auto fx_error = master_effects_.process(out);
  if (!fx_error.ok()) return fx_error;
  for (float& s : out.samples) s = std::clamp(s, -1.f, 1.f);
  return Error::success();
}

Error AudioMixer::mix_timeline(const ProjectSession& project, TimeSec time, TimeSec duration,
                               AudioBuffer& out, FFmpegPaths paths,
                               int sample_rate, int channels) {
  if (duration <= 0 || sample_rate <= 0 || channels <= 0)
    return Error::make(ErrorCode::InvalidArgument, "mix_timeline args");
  const int frames = std::max(1, static_cast<int>(duration * sample_rate));
  out.sample_rate = sample_rate; out.channels = channels;
  out.samples.assign(static_cast<size_t>(frames) * channels, 0.f);
  for (const auto& track : project.tracks()) {
    if (track.type != static_cast<int>(TrackType::Audio) || track.muted) continue;
    const float track_gain = track.gain;
    for (const auto& clip : track.clips) {
      if (clip.mute || clip.media_id == 0) continue;
      const auto* media = project.find_media(clip.media_id);
      if (!media || media->path.empty() || !media->has_audio) continue;
      const TimeSec clip_end = clip.start + clip.duration;
      const TimeSec win_end = time + duration;
      if (clip_end <= time || clip.start >= win_end) continue;
      const TimeSec overlap_start = std::max(time, clip.start);
      const TimeSec overlap_end = std::min(win_end, clip_end);
      const TimeSec source_start = clip.source_in + std::max<TimeSec>(0, overlap_start - clip.start) * std::max(0.001f, clip.speed);
      const TimeSec decode_len = overlap_end - overlap_start;
      if (decode_len <= 0) continue;
      AudioDecoderFFmpeg decoder(paths);
      auto e = decoder.open(media->path, sample_rate, channels);
      if (!e.ok()) return e;
      AudioBuffer src;
      e = decoder.decode_window(source_start, decode_len * std::max(0.001f, clip.speed), src);
      if (!e.ok()) return e;
      const int dst0 = std::max(0, static_cast<int>((overlap_start - time) * sample_rate));
      const float gain = std::clamp(clip.gain * track_gain, 0.f, 4.f);
      for (int f = 0; f < frames - dst0; ++f) {
        const size_t si = static_cast<size_t>(f) * src.channels;
        if (si >= src.samples.size()) break;
        const size_t di = static_cast<size_t>(dst0 + f) * channels;
        for (int c = 0; c < channels; ++c) {
          const int sc = std::min(c, src.channels - 1);
          out.samples[di + c] += src.samples[si + sc] * gain;
        }
      }
    }
  }
  auto fx_error = master_effects_.process(out);
  if (!fx_error.ok()) return fx_error;
  for (auto& v : out.samples) v = std::clamp(v, -1.f, 1.f);
  return Error::success();
}

} // namespace ms

