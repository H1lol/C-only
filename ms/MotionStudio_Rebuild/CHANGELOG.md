# Changelog

## 2026.0.0-alpha — 2026-10-30

### Added
- Native C++17 core: project, tracks, clips, undo foundations
- FFmpeg CLI + in-process libav decode (MS_HAVE_LIBAV)
- Frame buffer C ABI (`ms_frame_buffer_publish_rgba`)
- MLT timeline spike: playlist/tractor, XML save/load, edit rules
- Effects host: builtins, keyframes, frei0r catalog, OpenFX scan API
- Export presets, lossless cut/join, export queue
- Scene/silence auto-edit, SRT/ASS captions, JSON templates
- Flutter mobile editor shell (EffectCraft-inspired palette)
- Online effects catalog hook (H1lol/ms-effects)

### Known limits (alpha)
- Android NDK / MediaCodec not verified in CI
- Flutter not fully bound to MLT/libav graph
- GPU path is not production
- OpenFX host is discovery-only

### Gates
M0–M5 desktop spikes: PASSED

## 2026-10-30 — Milestones 17–22
- Added native 3D materials/camera/light scene data and DCC job export.
- Added Blender background render submission path.
- Added named color transforms and grading/display transform.
- Added OpenFX host registry boundary with built-in validation plugin.
- Added asynchronous background render queue.
- Added `ms_17_22_test` verification suite.

## 23–30
- Added settings/theme controller and professional starter panel.
- Added touch/performance policy helpers and atomic media/project storage helpers.
- Added Android packaging scaffold.
- Added release-check target and 23–30 readiness/license documentation.

## 2026-11-04 — Desktop reference alignment pass

- Replaced the Flutter runtime branding asset with the exact user-supplied MS logo (same bytes as the top-level branding asset).
- Matched the desktop workspace order to the reference: Edit, Animation, Compositing, Nodes, 3D, Color, Audio, Export.
- Tuned wide-screen dock widths, desktop header, composition-tab strip, transport bar, timeline region, and Render Queue footer toward the 1648×923 reference.
- Made Inspector / Effects / Details tabs selectable, with separate content views.
- Kept the mobile editor layout separate and adaptive.
- Validation limitation: Flutter/Dart SDK is not installed in this environment; no Flutter compile or runtime screenshot is claimed.
- Updated the native integration test's stale version expectation (`0.6`) to the current `2026.0.0-alpha` project version.
- Native CMake build and all six native test executables pass in the working environment; Flutter build remains unverified because the SDK is unavailable.

## 2026-11-04 — Upstream-informed native integration

- Added stateful native audio insert chains: gain, 3-band EQ, compressor, delay, reverb, and sample-peak limiter; exposed by AudioMixer, AudioPlayback, and TimelineAudioPlayback and applied to native PCM output.
- Rebuilt node-graph validation with cycle/port/endpoint checks, disconnect and parameter update APIs, ordered input evaluation, and recursion protection.
- Added saturation, invert, and straight-alpha CompositeOver graph operations.
- Added an optional Appleseed offline-renderer handoff target with generated project XML and a launcher (sphere proxy geometry pending real mesh serialization).
- Expanded export profiles for UHD, vertical, square, WebM/VP9, and ProRes delivery.
- Added selected upstream reference source and license texts for Sprocket, Gaffer, Kdenlive, and appleseed; retained LMMS license/design notes only due GPL-2.0-only compatibility boundary.
- Added CTest registration and `scripts/test_native.sh` for repeatable native regression testing, including generated-WAV standalone and timeline playback insert tests.
- Native Release build and all ten registered test executables pass. Flutter/device build and end-to-end UI playback remain unverified in this environment.


## 2026-11-04 — Blender 5.2.2 DCC integration

- Expanded scene-job JSON to preserve transforms, hierarchy, visibility, camera/light properties, mesh paths, and material data.
- Hardened JSON string escaping.
- Upgraded generated Blender jobs with OBJ/FBX/glTF/STL/USD imports, Principled materials, image textures, camera settings, light settings, object parenting, and render output.
- Added a selected Blender 5.2.2 reference-header set and retained upstream GPL license texts.
- Added regression assertions for scene serialization and Blender script capabilities.
- Validation: C++ Release build passed; CTest 10/10 passed. Blender runtime rendering remains unverified because Blender is not installed in this environment.


## 2026-11-04 — Mobile editor workspace pass

- Reworked the phone editor into a touch-first vertical stack that preserves the desktop editor's order: composition viewer, transport, timeline controls, multi-layer timeline, and quick tools.
- Kept the same MS logo asset, dark charcoal surfaces, violet active states, workspace names, and export affordance.
- Added a dedicated mobile timeline action strip for Snap, Keyframe, and Add Layer.
- Replaced cramped equal-width phone actions with a horizontally scrollable touch dock for Media, Effects, Text, Shape, Audio, Mask, Keyframe, Inspector, and More.
- Made narrow phone headers adapt by switching Export to an icon action and reducing secondary toolbar controls.
- Kept phone landscape in the touch-first layout; the full docked desktop workspace activates at 900 logical pixels.
- Flutter/Dart SDK is unavailable in this environment, so no Flutter compile or Android runtime screenshot is claimed. Native C++ tests are unchanged from the passing 10/10 v4 baseline.

## 2026-11-04 — Mobile polish and autosave

- Added atomic Flutter-side `.msproj` save and 10-minute recovery snapshots.
- Added recovery restore when the autosave is newer than the manual save.
- Added lifecycle-triggered recovery saves and visible Saved / Autosaved / Unsaved status.
- Persisted Recent and Archived project lists so recovery remains reachable after an app restart.
- Added Android 10 (API 29) minimum target and documented device/storage guidance.
- Improved mobile header touch targets and added system requirements to Settings.


## Cache safety update
- Added automatic generated-cache enforcement: clear the dedicated cache directory when usage exceeds 75% of a 1 GiB budget.
- Checks at startup and every five minutes.
- Project files, recovery snapshots, autosaves, source media and exports remain outside the cleanup scope.

## 2026-11-15 — Fixed: black preview when opening a saved project
- `Engine::load_project_json` never set `impl_->initialized`, and `render_frame()` returns false immediately while that flag is clear. Creating a new project went through `new_project()`, which does set it — so a fresh project rendered and an **opened** one did not. The starter screen opens projects directly, so every existing project showed a black preview and exported black frames.
- Load now brings the engine up the same way `new_project()` does: playhead tick reset, `initialized = true`, frame cache cleared, backend registered.
- Regression test `ms_engine_load_test`: create → serialise → shutdown (assert cold) → load → assert initialized, `render_frame()` true, and size restored from the file.

## 2026-11-15 — Effect catalogue and pattern wipes
- **14 new CPU effects**, all original implementations of standard operations — no GPL code or XML copied from Kdenlive/MLT/frei0r (Kdenlive is GPL-3.0 and is only a capability reference here): saturation, hue shift, levels, white balance, posterize, sharpen, edge detect (Sobel), emboss, pixelate, glow (blur screened back), film grain, box blur (running sum, O(n) per pass), crop, threshold.
- **Two effects that were dead code are now reachable**: `cpu_threshold` and `cpu_chromakey_simple` existed in `cpu_effects_extra.cpp` but no dispatch branch called them.
- **23 effects exposed in the UI**, grouped into Basic / Colour / Stylise / Framing, replacing the flat 8-item list.
- **3 pattern wipes**: blinds horizontal, blinds vertical and checkerboard, appended to the enum so existing project files keep their integer ids. Transitions now total 23 implemented and 23 listed.
- **New test `ms_effects_catalogue_test`** applies all 15 catalogue entries through `cpu_effect_apply` and fails on a passthrough (mean per-channel delta < 0.5), then checks an unknown id reports `EffectNotFound`. Note the first run failed five cases because saturation/hue/levels/white balance/crop are deliberately identity at their defaults — the test now drives them with off-default parameters.
- Verified: `ctest` **14/14**, `:app:testDebugUnitTest` **6/6**, `:app:assembleDebug` green.

## 2026-11-13 — Real decoding on Android
- **MediaCodec decoder**: `media/android_decode.cpp` implements `IMediaDecoder` over `AMediaCodec` + `AMediaExtractor`, and `create_media_decoder` now prefers it on Android. Before this the only decoder was a shell-out to the `ffmpeg` CLI, which does not exist on Android, so every imported clip silently fell through to `SlateDecoder` and rendered as a flat placeholder colour. Handles H.264/HEVC/VP9 platform codecs, keyframe seeking, and NV12/I420/YV12/flexible YUV output with stride and slice-height from the reported format. Links `libmediandk`; no bundled binaries, no new licence obligations.
- **Audio on Android**: `Android_decode_audio` decodes the extractor's audio track to float PCM, and `AudioDecoderFFmpeg::decode_window` uses it when ffmpeg is absent. That makes timeline playback, the export AAC track and the waveform real on device instead of silent.
- **Honest import metadata**: `ProjectSession::import_media` asks a real decoder for duration/size/audio presence before falling back to the container sniff, which used to hardcode 10 s, 1920×1080 and "has audio" for every MP4.
- **Decoder backend is visible**: `ms_decoder_backend()` reports `mediacodec` / `ffmpeg` / `slate`; the session sheet shows it in red when it is `slate`, so a placeholder can never masquerade as footage.
- **All effects exposed**: chroma key was implemented in `cpu_effects.cpp` but missing from `EffectsSheet` — now listed, bringing the UI to all nine.
- **All transitions exposed**: `MotionSheet` listed 5 of the 20 implemented transitions; it now lists all 20 (dissolve, 3 fades, 9 wipes, 4 slides, iris, zoom, dip).
- Verified: `ctest` **13/13**, `:app:testDebugUnitTest` **6/6**, `:app:assembleDebug` green, both ABIs linked against `libmediandk.so` with 28 `AMediaCodec`/`AMediaExtractor`/`AMediaFormat` call sites. ⚠️ Still never executed on a device.

## 2026-11-13 — Waveforms, per-clip grade, masks, markers, project settings
- **Timeline waveform**: `TimelineAudioPlayback::clip_peaks` computes a `bins`-wide peak envelope from the decoded PCM it already caches for playback, so asking for a waveform also warms the audio. `ms_clip_peaks` → JNI → `EditorEngine.peaks` (cached per clip/range) → audio clips draw real bars behind their label. Clips with no decodable audio return empty rather than a fake shape.
- **Per-clip grade**: `ClipInfo` gained its own seven-parameter grade plus `grade_enabled`, applied in `render_clip` **after** the clip's effects and **before** compositing, so shots can be matched without touching the master grade. Serialized as `grade_enabled` + `grade: [...]`. `ColorSheet` now switches between **Sequence** and **Selected clip** with the same seven sliders.
- **Masks**: the long-dormant `comp/mask.cpp` is finally used — rectangle, ellipse or full frame, normalised centre/size, feather in pixels and invert, multiplied into the clip's alpha before its transform. `MaskSheet` drives it; persisted as `mask_enabled` + `mk_*` keys.
- **Markers**: session markers with names, `add_marker`/`remove_marker`, serialized as `"markers"`, listed and added/removed in the session sheet.
- **Project settings**: `Engine::set_project_settings` changes size and frame rate without discarding edits; size presets (1080×1920, 1920×1080, 1080×1080, 2160×3840) and 24/30/60 fps in the session sheet.
- Fixed a duplicated `ms::TimelineMarker` definition: it now lives in `ms_project.hpp` (so it can be serialized) and `ms_edit_ops.hpp` only declares the helpers that use it.
- Verified: `ctest` **13/13** (the edit suite now also covers mask rasterisation, mask/grade/marker persistence and the clean-failure paths of `clip_peaks`), `:app:testDebugUnitTest` **6/6**, `:app:assembleDebug` green.

## 2026-11-13 — Titles that actually render
- **Text engine**: `native/cpp/text/text_render.cpp` rasterises real glyphs with stb_truetype 1.21 (public domain). Supports UTF-8, kerning, tracking, multiple lines, greedy word wrap against a box width, align (left/centre/right), anchor (top/middle/bottom), fill colour, outline with configurable colour, drop shadow, and uppercase. Fonts are cached per path; resolution falls back through the host font dir, the repo asset tree and `MS_FONT_DIR`.
- **Bundled faces**: Anton, Bebas Neue, Archivo Black, Oswald, Playfair Display and Lobster (all SIL OFL 1.1, licences in `third_party/fonts/titles/`). `TextFonts` unpacks them from assets to `filesDir/fonts` on first launch and hands the path to the engine.
- **Text clips are first-class**: `ClipInfo` gained `is_text`/`text`/`TextStyle`; the clip renderer calls the rasteriser instead of a decoder, so titles inherit keyframed transform, opacity, effects, grade and transitions. Serialised as `is_text` + `tx_*` keys in `.msproj`.
- **C ABI / JNI**: `ms_text_set_font_dir`, `ms_text_add_clip`, `ms_text_update_clip`, `ms_text_measure` and their JNI mirrors.
- **UI**: `TextSheet` — text field, six typefaces, size/tracking/line-spacing/outline/shadow/wrap sliders, fill and outline swatches, align/anchor, uppercase, add-at-playhead and live re-render of the selected title. Reached from a new **Title** button on the tool rail.
- **`ms_title_demo`**: renders sample title cards to PPM for inspection without a device; used to confirm glyphs, outlines and shadows visually.
- Verified: `ctest` **13/13** (the edit suite now checks font resolution, rasterised coverage, wrapping and title persistence), `:app:testDebugUnitTest` **6/6**, `:app:assembleDebug` green, 62 MB APK with all six fonts in `assets/text/`.

## 2026-11-12 — Grade, scopes, keyframes, transitions, audio export
- **Master grade**: `ms_grade_set` / `ms_grade_clear` apply exposure, contrast, saturation, temperature, lift, gamma and gain through `apply_finishing_grade` after the frame cache. `ColorSheet` has all seven sliders plus a live RGB/luma histogram drawn from `Scopes::histogram` over the last rendered frame.
- **Keyframes**: `ms_keyframe_add` / `ms_keyframe_clear` for opacity, scale, pos_x and pos_y; curves persist in `.msproj` (`keys_opacity` etc.) and are evaluated per frame.
- **Transform compositing**: `TimelinePlayer::blit_transformed` honours keyframed scale and position, not just opacity — previously scale/position curves were stored and ignored.
- **Transitions**: `comp/transitions.cpp` was **not in the build**; it is now compiled, and `TimelineTransition` (id, outgoing, incoming, type, duration, softness) lives on `TrackInfo`, serializes to `.msproj`, and is blended by `apply_transition` centred on the cut. `add_transition` / `remove_transition` are bridged; five transition types have a UI.
- **Renderer rewrite**: each clip now renders into a transparent premultiplied buffer which is composited with an explicit over-operator, so transitions and per-clip alpha are correct. With no transition the output is unchanged (all 13 native suites still pass).
- **Audio in export**: `VideoExporter` now feeds an AAC encoder from `ms_audio_read_pcm` alongside the H.264 video, buffering samples until both output formats are known, then muxing both tracks.
- Toolchain note: the cloud runner was restored mid-task; the JDK/SDK/NDK/Gradle/ffmpeg were reinstalled under `~/tools` and `~/android-sdk`.
- Verified: `ctest` **13/13**, `:app:assembleDebug` + `:app:testDebugUnitTest` green. Still not runtime-verified on a device.

## 2026-11-12 — Editing, effects, tracks, export
- **Clip editing bridged and wired**: `ms_clip_remove/move/trim/ripple_delete`; timeline drag moves a clip, right-edge drag trims it, long-press deletes, and drags snap to the playhead and neighbouring clip edges (0.12 s tolerance).
- **Track management**: `ms_track_add/remove/flags`; `+` adds a track, tap toggles mute, long-press removes.
- **Effect stack**: `ms_effect_add/remove/set_param/set_enabled` plus `EffectsSheet` (eight CPU effects, live sliders, enable switch, remove). Previously nine engine effects were unreachable from the UI.
- **Effect persistence**: clip effects are serialized in `.msproj` and restored on load — they used to be memory-only.
- **Frame cache correctness**: every edit/effect change calls `invalidate_frames()`; stale frames could previously outlive a trim or an effect change.
- **Real video export**: `VideoExporter` renders from the engine and encodes H.264 via `MediaCodec` + `MediaMuxer` (RGBA→NV12 conversion in Kotlin). Video only; audio muxing is not wired.
- Imports now land in `filesDir/imports` rather than `cacheDir`, which Android may purge.
- New `ms_edit_test` (move/trim/test coverage of effects, JSON round-trip, undo, delete): native suite is now **13/13**.
- Honest status: no APK has ever been launched here, so export and gestures are compile-verified only.

## 2026-11-12 — Open-source assets and 3D LUT looks
- Bundled **Inter 4.1** and **JetBrains Mono 2.304** (both SIL OFL 1.1) as real font resources; `MsFonts.sans` drives the Compose typography and `MsFonts.mono` drives every numeric readout (timecode, ruler, track labels). Licenses travel with them in `third_party/fonts/`.
- New `native/cpp/color/lut.cpp` + `ms_lut.hpp`: an Adobe `.cube` reader (TITLE / LUT_3D_SIZE / DOMAIN handling, red-fastest ordering) with trilinear sampling and a strength blend, applied to RGBA8 frames.
- Engine: `load_look` / `clear_look` / `set_look_strength`. The look is applied **after** the frame cache, so changing a LUT or its strength never needs a cache flush and never poisons cached frames.
- Bridge + JNI: `ms_look_load`, `ms_look_clear`, `ms_look_set_strength`, `ms_look_strength` → `NativeEngine.nativeLook*`.
- Six looks baked by `scripts/gen_looks.py` into `android/app/src/main/assets/looks/` (Teal & Orange, Bleach Bypass, Warm Film, Cool Noir, Vivid Punch, Fade Matte), 17³ each, ~130 KB.
- Kotlin: `LooksSheet` (apply/strength/None), a “Look” rail button, and a `LOOK · NAME` badge in the viewer header.
- Verified: `ms_lut_test` added to the native suite (identity cube, channel swap, half-strength blend, alpha preservation, garbage/short/1D/non-unit-domain rejection) → **12/12 CTest pass**. `gradle :app:assembleDebug` → 60 MB APK containing the six cubes, seven TTFs and the new JNI symbols; `:app:testDebugUnitTest` → 6/6.
- Not verified: visual result of each look on device (no emulator/device in this environment).

## 2026-11-12 — Flutter removed: Kotlin + C++ app
- Deleted the entire `flutter_ui/` tree (Dart UI, FFI bindings, Flutter web scaffolding, Dart tests). No Dart remains in the repository.
- Replaced the Android scaffold with a real Gradle project: AGP 8.7.2, Kotlin 2.0.21, Jetpack Compose, `compileSdk 35`, `minSdk 29`, NDK 27 + CMake building `native/cpp` through `MS_BUILD_TESTS=OFF`.
- Added `android/app/src/main/cpp/ms_jni_bridge.cpp`: JNI over the existing C ABI, including `AndroidBitmap` frame copy for the preview and `FloatArray` PCM pull for audio.
- New Kotlin layers: `NativeEngine` (sole JNI surface), `EditorEngine` (session + Compose state), `PreviewView` (real `ms_render_frame` output with pinch-zoom), `TimelinePane` (one shared horizontal scroll for ruler + lanes, playhead inside the scrolled content), `NativeAudioPlayer` (`AudioTrack` consuming the engine mix), `ProjectStore`/`SettingsStore`, starter/settings screens.
- The C++ engine gained `Engine::new_project(name, width, height, fps)` and the bridge export `ms_project_new`, so Kotlin can create named projects without touching the document schema.
- Project documents stay owned by C++: Kotlin parses the engine's JSON for drawing only and saves through the engine.
- Verified: `gradle :app:assembleDebug` produces a 58 MB debug APK containing `libmotionstudio_jni.so` and `libmotionstudio_native.so` for `arm64-v8a` and `x86_64`; `gradle :app:testDebugUnitTest` passes 6/6; the desktop native suite still passes 11/11 with `MS_BUILD_TESTS=ON`.
- Not verified: installation or interaction on a physical device/emulator (no KVM access in the build environment). Hardware video decode and video encoding remain unwired; PNG frame export works.

## 2026-11-12 — Engine timeline audio output
- `Engine` now owns a `TimelineAudioPlayback` bound to the active session: `init`, project load and `shutdown` rebind/stop it, `seek` moves the audio playhead with the video playhead, `play`/`pause` drive the audio transport, and every `render_frame` tick keeps both clocks on the same time.
- Added `Engine::read_audio`, `Engine::audio_info` and `Engine::audio_effects` (master insert chain).
- New C ABI for hosts: `ms_audio_state`, `ms_audio_read_pcm`, `ms_audio_set_master_gain`, with matching FFI bindings (`MSAudioState`, `readAudioPcm`, `setMasterGain`) in `flutter_ui/lib/bridge/native_bridge.dart`.
- Added `ms_engine_audio_test` (CTest 11/11): imports a generated tone, proves the engine mix has energy, the -20 dB master gain stage attenuates it, the audio playhead follows bridge seek/render ticks, and paused playback emits silence.
- Platform audio device output (AAudio/OpenSL/desktop callback) remains host-side; the engine now supplies the PCM and chain for it.
