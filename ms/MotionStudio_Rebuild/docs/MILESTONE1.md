# Milestone 1 — Decode & playback (PASSED desktop)

## Gate G1 (desktop equivalent)
In-process **libav** decode, seek, continuous playback publishing into `ms_frame_buffer_publish_rgba`.

```
M1 PASSED
libav h264 1280x720 → 640x360
19 frames published in ~0.5s wall clock
```

## What landed
| Piece | Status |
|-------|--------|
| `LibavDecoder` (link libav, no popen) | Yes |
| Seek + `read_video` RGBA via swscale | Yes |
| Prefetch thread (mutex-safe) | Yes |
| `LibavPlayback` play/pause/seek/tick | Yes |
| Publish to frame buffer (Flutter Texture path) | Yes |
| MediaCodec hwaccel | API note only — wire on Android NDK |

## Android next
Set `AVCodecContext` hw_device_ctx to MediaCodec; same `LibavDecoder` API.

## Build
```bash
g++ -DMS_HAVE_LIBAV ... $(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale)
```
