# Motion Studio mobile editor layout — 2026-11-04

## Design objective

Keep the desktop editor's visual identity and editing vocabulary on phones without trying to squeeze its three-column desktop layout into a narrow screen. The mobile layout is a deliberate vertical adaptation, not a different product.

## Shared visual language

- Canonical MS logo asset: `flutter_ui/assets/branding/ic_motion_studio_logo.png` (same bytes as the user-supplied logo in `assets/branding/`).
- Deep charcoal / near-black editor surfaces, thin graphite separators, lavender-violet active states, and the same typography hierarchy.
- The same top-level workspaces: Edit, Animation, Compositing, Nodes, 3D, Color, Audio, Export.

## Phone composition

1. **Compact project header:** back navigation, MS logo, project title and save state, undo, and export. On widths below 350 logical pixels, redo is removed from the header and export becomes an icon action to avoid overflow.
2. **Scrollable workspace selector:** all eight desktop workspaces remain available without wrapping or compressing labels.
3. **Composition viewer:** retains the desktop checker/canvas and workspace content. Mobile preview options use compact, touch-sized controls.
4. **Transport bar:** previous frame, play/pause, next frame, timecode, FPS, and loop.
5. **Timeline action strip:** snap state, keyframe creation, and add-layer action are separated from preview controls.
6. **Timeline dock:** the same multi-layer timeline and ruler are retained with a mobile-sized name column and horizontal track scrolling. Its height adapts to available screen height.
7. **Quick tool dock:** horizontally scrollable touch targets for Media, Effects, Text, Shape, Audio, Mask, Keyframe, Inspector, and More.

## Responsive breakpoints

- Below 900 logical pixels: touch-first stacked layout, including phone landscape.
- At or above 900 logical pixels: docked workspace layout.
- At available safe-area heights below 460 logical pixels, the horizontal workspace strip folds into the workspace picker in the preview header.
- Below 430 logical pixels of editor height, the secondary timeline strip is folded into the bottom tool dock and the timeline uses a short-height clamp.
- The timeline dock has compact-height and regular-height clamps so it remains usable on shorter screens.

## Interaction status and limits

The workspace selector, playback transport state, timeline scrubbing, layer visibility/lock, add-layer, add-keyframe, tool sheets, inspector sheet, and export settings UI use existing Flutter state and callbacks. Some native work remains: platform media picker, persistent project save/open, live frame/texture delivery, full native tool-mode bindings, and render job submission. The 3D/Nodes/Color/Audio canvases are not claimed to be full mobile-native implementations merely because their workspace entries are visible.

## Validation

- Confirmed canonical logo asset byte equality between runtime Flutter asset and top-level branding asset.
- Source diff reviewed for the mobile breakpoint, header, touch dock, timeline strip, and viewer controls.
- Native C++ suite remains at the v4 baseline: 10/10 tests passed.
- Flutter/Dart SDK is unavailable in this environment; no Dart analysis, Flutter build, Android APK, emulator test, or device screenshot is claimed.
