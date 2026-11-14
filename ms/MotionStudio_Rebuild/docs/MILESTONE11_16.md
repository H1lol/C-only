# Motion Studio — Milestones 11–16

Implemented in the native C++ core and exposed through the Flutter FFI bridge.

## 11 — Caching + proxies
- In-memory `FrameCache` remains the hot-frame cache.
- `DiskFrameCache` stores reusable RGBA frames on disk with atomic commits.
- `ProxyManager` generates deterministic FFmpeg MP4 proxy paths and can build/remove proxies.
- Engine preview now checks the frame cache before rendering and records cache hits/misses.

## 12 — Audio timeline + mixer
- FFmpeg-backed `AudioDecoderFFmpeg` is linked into the core build.
- `AudioMixer::mix_timeline()` walks project audio tracks, decodes only overlapping source windows, applies track/clip gain, and clamps output.
- Existing transport-facing `AudioPlayback` / `TimelineAudioPlayback` remains available.

## 13 — Undo/redo + autosave
- Undo/redo history is preserved across state restoration instead of being erased by `load_from_json()`.
- `AutosaveController` supports interval-based and forced autosaves.
- Project saves use a temporary file + rename and keep a `.bak` copy.

## 14 — Project save/load
- `.msproj` loading now reconstructs media, every track, and clips on their original tracks.
- Audio gain/pan/mute and track gain/solo are serialized.
- File saves are atomic.

## 15 — Presets / effect browser
- Native `EffectPresetLibrary` provides searchable preset metadata and built-in presets.
- Flutter's effect catalog service remains the UI/download orchestration layer.

## 16 — Mobile performance
- `PerformanceMonitor` tracks frame time/FPS and cache hit/miss counts.
- `RenderBudget` selects an adaptive preview scale from measured frame time.
- Engine preview is cache-aware and exposes performance counters through FFI.

### Verification
`ms_11_16_test` passes all Milestone 11–16 checks.
