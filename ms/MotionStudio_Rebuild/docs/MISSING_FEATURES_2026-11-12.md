# What is genuinely missing (2026-11-12, after the completion pass)

Scope: the shipping app is Kotlin/Compose + the C++ engine over the C ABI in
`native/cpp/bridge/ms_bridge.h`. "Missing" means **missing from the app**, not
from the engine source — that distinction was the original finding.

Legend: ✅ done in this pass · ❌ still missing · ⚠️ partial / untested on device

## Closed in this pass

| Item | What landed |
|---|---|
| ✅ Clip editing | `remove_clip`, `move_clip`, `trim_clip`, `ripple_delete` bridged; timeline drag moves, edge-drag trims, long-press deletes, drag snapping to playhead and clip edges. |
| ✅ Track management | `add_track`, `remove_track`, `set_track_flags` bridged; `+` adds a track, tap toggles mute, long-press removes, mute affects colour and label. |
| ✅ Effect stack UI | `add_effect` / `remove_effect` / `set_effect_param` / `set_effect_enabled` bridged; `EffectsSheet` lists eight CPU effects with live parameter sliders, per-effect enable switch and remove. Clip chips show `FX n`. |
| ✅ Effect persistence | Clip effects are now serialized in `.msproj` (`"effects": [{id, enabled, params:[{name,value}]}]`) and restored on load; previously they were memory-only and silently lost on save. |
| ✅ Frame-cache invalidation | Every edit and effect change clears the frame cache; before this, retimed or re-effected clips could show stale pixels. |
| ✅ Video export (video-only) | `VideoExporter` renders frames from the engine and encodes H.264 with `MediaCodec` + `MediaMuxer` into an MP4 in `filesDir/exports`. No ffmpeg. ⚠️ Compiled only — never executed, because there is no device or emulator here. |
| ✅ Import durability | Imports now copy to `filesDir/imports` instead of `cacheDir`, which the OS can purge out from under a project. |
| ✅ Typography | Inter + JetBrains Mono (OFL) bundled and wired, mono for all numeric readouts. |
| ✅ LUT looks | `.cube` engine module, six baked looks, strength blend, applied after the cache. |

### Closed in the second pass

| Item | What landed |
|---|---|
| ✅ Master grade | Seven-parameter grade applied in the render path, with a live RGB/luma histogram from the engine's scopes. |
| ✅ Keyframes | Opacity, scale, pos_x, pos_y curves with add/clear, persisted in the project JSON. |
| ✅ Transform compositing | Scale and position curves now actually move the image (they were stored and ignored). |
| ✅ Transitions | The transitions module was never compiled; it is now built, blended centred on the cut, persisted, and exposed with five types in the UI. |
| ✅ Audio in export | AAC track muxed from the engine's PCM mix alongside H.264. |
| ✅ Text and titles | stb_truetype rasteriser, six bundled OFL faces, per-clip style, bridged and editable in `TextSheet`. |
| ✅ Waveform | `TimelineAudioPlayback::clip_peaks` reads the already-cached decoded PCM; audio clips draw their real peak envelope in the timeline. |
| ✅ Per-clip grade | `ClipInfo` carries its own seven-parameter grade, applied per clip before compositing; `ColorSheet` switches between Sequence and Selected clip. |
| ✅ Masks | `comp/mask.cpp` is bridged: rectangle/ellipse/full with feather and invert, applied before the clip's transform; `MaskSheet` edits it. |
| ✅ Markers | Session-level markers with names, serialized as `"markers"`, added/removed from the session sheet. |
| ✅ Project settings | Size and frame rate can be changed without discarding edits (`ms_project_settings`). |
| ✅ Android decode | MediaCodec/MediaExtractor decoder replaces the ffmpeg shell-out, plus PCM audio decode for playback, export and waveforms. ⚠️ Compiled and linked, never run on a device. |

## Still missing

### 1. Text editing depth (titles themselves now render)
Titles are real now: `native/cpp/text/text_render.cpp` rasterises glyphs with stb_truetype, six OFL display faces ship in `assets/text/`, the C ABI has `ms_text_*`, and `TextSheet` edits text, family, size, tracking, line spacing, outline, shadow, wrap width, fill/outline colour, align, anchor and uppercase. A title is an ordinary clip, so keyframed transform, effects, grade and transitions all apply.
What is still *not* there: per-character or per-word styling (one style per clip), animation presets (type-on, word-by-word), text on a path, curved/warped text, right-to-left shaping and complex-script shaping (stb_truetype has no HarfBuzz, so Arabic/Devanagari will not join correctly), and importing a user font — a SAF picker plus the licence rule that user fonts must never be redistributed.

### 2. User-supplied effect packages
The `.msp` format is specified (`docs/plugins/MSP_FORMAT.md`: zip + `manifest.json` + shaders + `icon.png`), and `MspRegistry` can scan/parse it — but:
- `install_msp` shells out to the system `unzip` binary, which does not exist on Android;
- `EffectStore::install_package` is still a stub;
- there is no import UI and no package validator;
- the format is **JSON, not XML**, and only the `composite` entry type is data-only safe — `shader`/`wasm`/`native` packages execute code.
Needed: a Kotlin zip extractor, manifest validation, checksum, a browser with thumbnails, and a decision on whether third-party code execution is in scope.

### 3. Keyframe curve editor
Keyframes exist end-to-end now, but authoring is value-at-playhead plus add/clear. There is no curve view, no per-key interpolation choice, and no drag-to-retime in the UI.

### 4. Mask depth
Shapes are there (rectangle, ellipse, full frame, feather, invert). Missing: more than one mask per clip, bezier/roto shapes, mask tracking to follow motion, and mask keyframes.

### 5. Grade controls
Both the master grade and per-clip grades work, each with seven parameters and a live histogram. Missing: curves, colour wheels, a highlight/shadow split, and per-clip LUTs (the `.cube` look is still display-wide).

### 6. Audio depth
Export carries AAC audio, and audio clips now draw a real waveform from decoded PCM. The master insert chain is now editable from the app (Gain, 3-band EQ, compressor, delay, reverb, limiter) with a MIX sheet carrying per-track faders, mute and solo; clip gain and pan are honoured by the mix. Still missing: insert chains per track (only the master bus has one), audio automation curves, sample-rate choice, and loudness metering. The chain lives in engine state, so it is not saved with the project and chain edits are not undoable.

### 6b. Effect catalogue size
23 CPU effects against Kdenlive's roughly 84 hand-written effect definitions plus 145 MLT avfilter and 118 frei0r entries. Most of that gap is provided by third-party GPL plugins we cannot link; adding them would relicense the app. Audio effects are still the thinnest area: the master bus has six inserts, but tracks cannot host their own chain and there is no audio keyframing.

### 7. Playback performance
Decoding now uses MediaCodec, but playback is still seek-per-frame rather than a streaming pipeline, so 4K will not play smoothly. Compositing remains CPU into an RGBA8 buffer (`gpu_mock`), and there are no proxies or cache-management UI.

### 7b. Comp network depth
The node graph evaluates (Constant, Blur, Grade, Saturate, Invert, Merge, Over, Viewer) and is saved
with the project, and `Read` decodes media or samples the edited timeline through the engine's
provider. `Transform` now does translate/scale/rotate/opacity, and knobs can be linked to other
nodes' numeric knobs (cycle-checked, persisted). It is not yet a full compositor: there are no
expressions (links only), no with wired parameters, no per-node persistent cache across frames,
viewer AOI/tiling or proxy playback, one decoder per media path with no prefetch, and node colour
for Constant is not editable from the UI.

### 8. Multicam and nested sequences
Markers are done. Multicam and nested sequences still have zero references in the engine — a nested sequence would need a clip type that renders another timeline.

### 9. Project settings depth
Size presets and 24/30/60 fps are editable in the session sheet. Missing: arbitrary width/height entry, colour space and bit-depth choice, and audio sample-rate selection.

### 10. Colour pipeline depth
8-bit RGBA8 preview path; no 10-bit, no HDR, no practical linear working space.

### 11. Verification on hardware
**No APK has ever been launched in this environment** (no device, `/dev/kvm` not accessible to this user). Every runtime claim — JNI, `MediaCodec` export, `AudioTrack` playback, gestures — is compile/link/test evidence only.

## Test coverage now

- `ctest` **13/13**: existing suites plus `ms_lut_test` (cube parsing, blend, rejection) and `ms_edit_test` (move, trim, ripple, tracks, effect add/param/enable, JSON round-trip, undo, delete, transition add/persist/remove, font resolution, title rasterisation, word wrap, title persistence).
- `:app:testDebugUnitTest` **6/6**: native JSON contract, timeline end, malformed JSON, aspect ratio, timecode, clip lookup.
- Android build: `:app:assembleDebug` green, both ABIs, all assets packaged (6 title fonts, 6 looks, 7 UI fonts).
- `ms_title_demo <dir>` renders four title cards to PPM for visual inspection without a device — this is how the rasteriser was confirmed, not just unit-tested.
