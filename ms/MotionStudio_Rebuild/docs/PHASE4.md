# Phase 4 — Nodes / color **PASSED**

## Nodes
- Graph: Constant, Read, Blur, Grade, Merge, Transform, Viewer, Write
- Connect + evaluate to sink
- Cache per node during eval

## Color
- Working/display space list (Linear, sRGB, ACEScg, Rec.709, …)
- Grade: exposure, contrast, saturation

## Scopes
- RGB/Y histogram
- Luma waveform columns

## Test
```bash
g++ -std=c++17 -I native/cpp/include \
  native/cpp/graph/node_graph.cpp \
  native/cpp/color/color_pipeline.cpp \
  native/cpp/color/scopes.cpp \
  native/cpp/effects/cpu_effects.cpp \
  native/cpp/render/image_buffer.cpp \
  native/cpp/tools/ms_phase4_test.cpp \
  -o ms_phase4_test && ./ms_phase4_test
```

## Next
**Phase 5** — offline backends + polish (render queue, optional AI)
