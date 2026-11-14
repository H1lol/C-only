# Phase 2 & 3 — PASSED

## Phase 2 — Media pipeline
- `MediaProbe` + `SlateDecoder` (FFmpeg-shaped; slate frames until linked)
- `TimelinePlayer` — prepare decoders, `render_frame(t)`, scrub
- Composite active clips onto project resolution buffer
- `SequenceExporter` — PPM frame sequence + `export_ok.txt`

## Phase 3 — Motion / effects
- `AnimCurve` / keyframes (linear, hold, ease in/out)
- `ClipMotion` — opacity / pos / scale curves
- CPU effects: solid, opacity, brightness/contrast, gaussian blur, invert
- `EffectStack::apply` — ordered effect ops

## Test
```bash
g++ -std=c++17 -I native/cpp/include \
  native/cpp/engine/project_session.cpp \
  native/cpp/media/media_probe.cpp \
  native/cpp/media/timeline_player.cpp \
  native/cpp/effects/cpu_effects.cpp \
  native/cpp/effects/effect_stack.cpp \
  native/cpp/render/image_buffer.cpp \
  native/cpp/tools/ms_phase23_test.cpp \
  -o ms_phase23_test && ./ms_phase23_test
```

## Next (Phase 4)
Nodes / color pipeline (node graph eval, grade, scopes)

## AI
Deferred until app phases solid (per plan).
