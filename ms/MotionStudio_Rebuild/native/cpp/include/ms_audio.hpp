#pragma once
/**
 * Pass 5 — Audio foundation (mixer)
 */
#include "ms_media.hpp"
#include "ms_audio_fx.hpp"
#include "ms_error.hpp"
#include "ms_ffmpeg_io.hpp"
#include "ms_project.hpp"
#include <unordered_map>
#include <memory>
#include "ms_types.hpp"
#include <string>
#include <vector>
#include <cmath>

namespace ms {

struct AudioClip {
  uint64_t id = 0;
  std::string name;
  MediaId media_id = 0;
  TimeSec start = 0;
  TimeSec duration = 1;
  TimeSec source_in = 0;
  float gain = 1.f;      // linear
  float pan = 0.f;       // -1 left .. 1 right
  bool mute = false;
};

struct AudioTrack {
  uint64_t id = 0;
  std::string name = "Audio";
  bool mute = false;
  bool solo = false;
  float gain = 1.f;
  std::vector<AudioClip> clips;
};

class AudioMixer {
public:
  uint64_t add_track(const std::string& name = "Audio");
  AudioTrack* find_track(uint64_t id);
  std::vector<AudioTrack>& tracks() { return tracks_; }
  AudioEffectChain& master_effects() { return master_effects_; }
  const AudioEffectChain& master_effects() const { return master_effects_; }

  uint64_t add_clip(uint64_t track_id, const AudioClip& clip);

  /**
   * Mix all tracks into out for a window [time, time+duration]
   * Generates silence/tone placeholders until real decode is linked.
   */
  Error mix(TimeSec time, TimeSec duration, AudioBuffer& out,
            int sample_rate = 48000, int channels = 2);

  // Decode and mix the project audio tracks for the requested window.
  Error mix_timeline(const ProjectSession& project, TimeSec time, TimeSec duration,
                     AudioBuffer& out, FFmpegPaths paths = {},
                     int sample_rate = 48000, int channels = 2);

private:
  std::vector<AudioTrack> tracks_;
  uint64_t next_track_ = 1;
  uint64_t next_clip_ = 1;
  AudioEffectChain master_effects_;
};

/** dB helpers */
inline float db_to_lin(float db) { return std::pow(10.f, db / 20.f); }
inline float lin_to_db(float lin) {
  if (lin < 1e-8f) return -120.f;
  return 20.f * std::log10(lin);
}

} // namespace ms

