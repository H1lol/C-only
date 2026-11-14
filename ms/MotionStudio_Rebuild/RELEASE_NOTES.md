# MotionStudio 2026.0.0-alpha — Release notes

**Tag:** `v2026.0.0-alpha`  
**Date:** 2026-10-30  

## For users
First public **alpha**. Expect crashes, incomplete UI, and missing features.
Use for testing and contribution — not for final client delivery yet.

## For developers
1. Clone / unpack this tree  
2. Install `ffmpeg` + optional `libav*` + `libmlt-dev`  
3. Build native tests under `native/cpp`  
4. Flutter: `flutter_ui/` with Flutter 3.27+  
5. Publish `H1lol/ms-effects` for online effects  

## Recommend before “stable”
- [ ] One Android device: import → scrub → export  
- [ ] Wire Flutter Texture to `ms_frame_buffer_latest`  
- [ ] Proxy/cache for long 1080p clips  
- [ ] CI: Linux + Android arm64 artifact  

## Support
Issues on your GitHub repo. GPL-3.0 source must ship with binaries.

## Desktop reference alignment — 2026-11-04

- Updated the exact MS logo asset loaded by Flutter.
- Refined desktop workspace tab order, persistent tool rail, project/viewer/inspector dock proportions, timeline layer-column width, Graph Editor dock, and full-width Render Queue footer to follow the supplied desktop reference.
- Inspector / Effects / Details tabs now switch between separate content panels.
- Native C++ build and six native test executables pass. Flutter compilation and desktop runtime rendering remain unverified in this environment because Flutter/Dart are not installed.
- See `docs/UI_REFERENCE_MATCH.md` and `docs/VALIDATION_2026-11-04.md`.

## Upstream integration update — 2026-11-04

Native additions include an audio master insert chain, graph validation and new compositing operations, extended export presets, and an optional Appleseed handoff. See `docs/UPSTREAM_INTEGRATION_2026-11-04.md` for architecture, license handling, limitations, and validation detail. The C++ native build and ten CTest tests pass; Flutter/device validation remains outstanding.


## Mobile editor layout — 2026-11-04

The phone editor now uses a portrait-first workspace built from the same desktop visual language. The top workspace switcher, compact project header, composition viewer, time transport, timeline tool strip, layer timeline, and horizontally scrollable action dock have separate touch-sized zones. Narrow devices collapse the Export button to an icon. Phones in landscape remain on the touch layout; the desktop dock layout starts at 900 logical pixels.

This is a Flutter source update, not a tested Android APK. Flutter/Dart compilation and device interaction still require validation on a machine with Flutter and the Android SDK installed.

## Mobile polish v6

- Android 10+ target (API 29).
- 4 GB RAM and 4 GB free internal storage are the minimum device baseline; 6–8 GB RAM and 8 GB+ free storage are recommended for larger jobs.
- Timed project recovery every 10 minutes for dirty documents, plus app lifecycle recovery attempts.
- Manual save writes a project file; recovery restore prefers a newer valid autosave.
- Recent and archived project lists are retained locally across app restarts.
- The current Flutter-side schema is documented with its serialization limits.
