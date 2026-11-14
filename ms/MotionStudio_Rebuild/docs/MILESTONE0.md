# Milestone 0 — Build spike (PASSED on desktop)

## Gate G0
One frame into `ms_frame_buffer_publish_rgba` via MLT.

## Result (this environment)
```
M0 SPIKE PASSED
mlt_available=yes
backend=mlt  320x180 colour + file
```

## API
```c
int ms_frame_buffer_publish_rgba(int w, int h, const unsigned char* rgba,
                                 int stride, long long pts_ms);
int ms_frame_buffer_latest(...);
```

```cpp
mlt_render_one_frame("color:blue", 0.0, 320, 180);
mlt_render_one_frame("/path/clip.mp4", 0.1, 320, 180);
```

## Build
```bash
g++ -std=c++17 -DMS_HAVE_MLT $(pkg-config --cflags --libs mlt-framework-7) \
  -I native/cpp/include \
  native/cpp/media/frame_buffer.cpp native/cpp/media/mlt_engine.cpp \
  ... -o ms_m0_spike
```

## Android (your machine)
- Cross-build FFmpeg + MLT for `arm64-v8a`
- Same sources with `-DMS_HAVE_MLT`
- If MLT fails → GStreamer Editing Services (plan fallback)

## Next: Milestone 1
In-process libav + MediaCodec, prefetch, Flutter Texture.
