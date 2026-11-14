# MotionStudio UI — EffectCraft-only redesign

**Reference:** EffectCraft (desktop + Pocket) only. No other app UI references.

## Visual language
- Background: `#1B1C1F`
- Panels: `#232326` / `#2A2A2E`
- Borders: `#3A3A3A`
- Accent: `#2D8CEB` (EffectCraft blue)
- Text: `#E3E3E3` / `#C9C9CC` / `#8C8C8C`

## Layout (EffectCraft panels)
1. **Menu bar** — project / properties / effects / undo / redo / save
2. **Project panel** — comps, footage, folders, search
3. **Composition viewer** — canvas, checkerboard, fit/1:1, time readout
4. **Properties panel** — Transform, Layer, Effects
5. **Transport** — play / pause / seek / fps
6. **Timeline** — layer names + clip bars + playhead
7. **Effects & Presets drawer** — categorized effects + “Download more”

## New Flutter files
```
lib/theme/ms_theme.dart          (EffectCraft palette)
lib/screens/editor_screen.dart
lib/widgets/ec_menu_bar.dart
lib/widgets/ec_project_panel.dart
lib/widgets/ec_composition_viewer.dart
lib/widgets/ec_timeline.dart
lib/widgets/ec_transport.dart
lib/widgets/ec_properties_panel.dart
lib/widgets/ec_effects_drawer.dart
lib/widgets/ec_loading_overlay.dart
```

## Useful additions
- Command-style UI actions (`ms_ui_commands.hpp`) for Flutter ↔ C++
- On-demand effects entry from Effects drawer
- Loading overlay matching EffectCraft Pocket
- Wide vs narrow adaptive layout (desktop panels / mobile canvas-first)

## Old CapCut-style widgets
`top_bar.dart`, `left_rail.dart`, `bottom_tools.dart`, `preview_area.dart`,
`timeline_panel.dart` are superseded by the `ec_*` widgets. Safe to delete later.
