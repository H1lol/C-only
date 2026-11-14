# Upstream project integration — 2026-11-04

This change set reviews the five uploaded source archives and adopts selected architecture, workflow, and interop ideas without importing whole applications.

## Delivered native changes

### Audio insert/master effects

- Added `native/cpp/include/ms_audio_fx.hpp` and `native/cpp/media/audio_fx.cpp`.
- `AudioEffectChain` supports gain, 3-band parametric EQ (low shelf / bell / high shelf), feed-forward compressor, tempo-independent delay, Schroeder-style comb reverb, and a sample-peak limiter.
- `AudioMixer::master_effects()` exposes an ordered master insert chain; `mix()` and `mix_timeline()` run the master chain after summing.
- `AudioPlayback` and `TimelineAudioPlayback` now expose their own ordered insert chains and process decoded PCM in `read_pcm()`.
- DSP state is preserved block-to-block and rebuilt when format or parameters change.
- The limiter is a sample-peak clamp, not a look-ahead true-peak limiter. EQ is three-band, not a full parametric channel strip. Metering, automation, sidechain routing, plugin hosting, and Flutter controls are not included in this pass.
- `AudioMixer` and `TimelineAudioPlayback` now process native audio through insert chains, but the current `Engine`/`TimelinePlayer` Flutter output route is not yet end-to-end connected to `TimelineAudioPlayback`. Do not describe the new effects as already audible in the app UI until that platform/output integration is completed.

### Compositor graph safety and operations

- `NodeGraph` now checks duplicate IDs, missing endpoints, invalid ports, duplicate inputs, and cycles.
- Connection creation rejects self-connections and cycles; validation performs a topological pass.
- Added parameter setting, disconnect support, stable port ordering, and evaluation recursion protection.
- Added `Saturation`, `Invert`, and straight-alpha `CompositeOver` operations.
- `Read` remains a placeholder generator until the media-provider/decoder is connected to graph evaluation.

### Offline 3D handoff

- Added an optional Appleseed target to the DCC bridge.
- The bridge writes a small `.appleseed` XML project plus a shell launcher calling `appleseed.cli`.
- Current `Scene` data stores mesh asset references but does not resolve/serialize real mesh geometry into the Appleseed scene. The generated XML therefore uses sphere proxies for mesh instances and records the original asset references in comments/job metadata. It is a handoff scaffold, not full geometry/material translation or a verified render.
- No Appleseed executable is bundled. Install Appleseed separately and set the binary path when submitting the job.

### Export profiles

Added delivery presets for UHD 4K, vertical 1080p and 4K, square 1080p, VP9/WebM, and ProRes 4K master. Codec-specific FFmpeg arguments are emitted by the export engine.

### Build and regression tests

- The CMake target now includes `media/export_engine.cpp`, `media/mlt_timeline.cpp`, and `media/audio_fx.cpp`.
- Native tests are registered with CTest. Run `scripts/test_native.sh` from the repository root.
- Audio tests generate short WAV files and verify that gain inserts change decoded `AudioPlayback` / `TimelineAudioPlayback` output when FFmpeg is available.

## Upstream sources and license handling

Selected upstream reference files and their license texts are retained in `third_party/upstream_reference_sources/`. These are reference/source-notes bundles; the upstream applications are not built as part of Motion Studio.

| Upstream | Supplied license | Selected material | Use in Motion Studio |
|---|---|---|---|
| Sprocket 0.1.101-alpha | MIT | License, architecture/features docs, selected C# model/audio/timeline files | Reference for non-destructive edit commands, animatable values, render plans, audio-effect organization. Native C++ DSP in Motion Studio is a new implementation; the C# files are not compiled into MS. |
| Gaffer 1.7.4.0 | BSD-style | License, GraphComponent and GraphGadget references, selected node implementations | Reference for graph ownership, connections, and node UI. Motion Studio's graph evaluator is its own C++ implementation. |
| Kdenlive master | GPL-3.0 | COPYING, AUTHORS, profile XML, selected timeline-model files | Reference for multi-track, grouping, snapping, and export-profile workflows. The GPL-3 source excerpts are retained with notices; they are not compiled into the native target. Motion Studio is GPL-3.0-or-later. |
| appleseed master-clean | MIT | License, README, sample project and CLI references | Optional renderer handoff format and command-line integration. No renderer source is embedded. |
| LMMS 1.2.2 | GPL-2.0-only | License and a note describing reviewed workflow ideas | Design reference only. No LMMS implementation source was copied because GPL-2.0-only code cannot be combined into this GPL-3.0-or-later project under the supplied license. |

No full upstream source archive is copied into the runtime. Original uploaded archives are not included in this repository; only the selected references listed above are retained.

## Validation performed

- C++17 CMake Release build succeeds.
- Nine native test executables are registered with CTest and pass in the current Linux environment.
- Graph safety, audio effects, and export profile tests cover the new additions.
- Appleseed generated project XML was parsed successfully with an XML parser during development; an Appleseed render was not run because the binary is not bundled/available in the test environment.
- Flutter/Dart SDK is unavailable in this environment. Flutter compilation, platform FFI wiring, and live UI playback/render behavior are not verified by this change set.
