# Motion Studio validation — 2026-11-04

## Passed

- `scripts/release_check.sh`
- CMake configure and native C++17 build (`ms_core`, shared bridge and test executables)
- `ms_studio_test`
- `ms_pass345_deep`
- `ms_pass6_test`
- `ms_11_16_test`
- `ms_17_22_test`
- `ms_23_30_test`
- `ms_audio_fx_test`
- `ms_audio_playback_test`
- `ms_graph_safety_test`
- `ms_export_profile_test`
- `ctest --test-dir /tmp/ms-build --output-on-failure` (10/10)
- `scripts/test_native.sh` with `MS_BUILD_DIR=/tmp/ms-build`
- `flutter_ui/pubspec.yaml` parses as YAML
- Both canonical branding assets have byte-for-byte equality with the user's uploaded logo
- Basic delimiter-balance check on the changed Dart files

## Not verified

- Flutter/Dart compilation and analyzer: Flutter and Dart SDKs are not installed in this environment.
- Desktop runtime rendering and pixel comparison against the 1648×923 reference.
- Android import / playback / export on a physical device.
- End-to-end bridge from Flutter / Engine to `TimelineAudioPlayback` audio output. **Resolved 2026-11-12:** `Engine` now owns a `TimelineAudioPlayback`, keeps its playhead in step with `seek`/`play`/`pause`/`render_frame`, and exposes the mixed PCM plus the master insert chain through `ms_audio_state`, `ms_audio_read_pcm` and `ms_audio_set_master_gain`. Covered by `ms_engine_audio_test`. Only the platform audio device/output callback is still host-side.

## Test fixture correction

`native/cpp/tools/ms_studio_test.cpp` used to assert that the version string contained `0.6`; the current project version is `2026.0.0-alpha`. The assertion was corrected to match the project metadata. The integration test then passed.
