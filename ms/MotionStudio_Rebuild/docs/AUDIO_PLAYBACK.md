# FFmpeg audio playback

## Classes
| Class | Role |
|-------|------|
| `AudioDecoderFFmpeg` | Decode file/window → float PCM via `ffmpeg -f f32le` |
| `AudioPlayback` | Play/pause/seek/advance + `read_pcm` for host output |
| `TimelineAudioPlayback` | Mix project clips’ audio at playhead |

## Flow
```text
ffmpeg → f32le PCM → AudioPlayback.read_pcm() → your audio device / SDL later
```

No OS audio device is opened here (portable). Host calls `read_pcm` on the audio callback / tick.

## Sync with video
Use the same `dt` for `MediaPlayer` clock and `AudioPlayback::advance(dt)`.

## Test
`ms_audio_playback_test` → **AUDIO PLAYBACK PASSED**
