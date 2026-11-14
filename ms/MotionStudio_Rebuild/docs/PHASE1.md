# Phase 1 — Project foundation **PASSED**

## Goals
- Project create / save / load (`.msproj` JSON)
- Media bin import (probe-based)
- Multi-track timeline: add / move / trim / split clips
- Playback state (seek / play / pause)
- Undo / redo (JSON snapshots)
- Autosave when dirty
- Image buffers

## Test
```bash
g++ -std=c++17 -I native/cpp/include \
  native/cpp/engine/project_session.cpp \
  native/cpp/media/media_probe.cpp \
  native/cpp/render/image_buffer.cpp \
  native/cpp/tools/ms_phase1_test.cpp \
  -o ms_phase1_test
./ms_phase1_test
```

## Done checklist
- [x] `ProjectSession::new_project`
- [x] `to_json` / `load_from_json` / file I/O
- [x] Default Video / Overlay / Audio tracks
- [x] `import_media` + `MediaProbe`
- [x] `add_clip` `move_clip` `trim_clip` `split_clip` `remove_clip`
- [x] Undo/redo stacks
- [x] Dirty + autosave
- [x] Acceptance test **PHASE 1 PASSED**

## Out of scope (later phases)
FFmpeg decode, export encode, GPU, effects graph, offline render

## Next
**Phase 2 — Media pipeline** (real decode frames into timeline scrub + export path)
