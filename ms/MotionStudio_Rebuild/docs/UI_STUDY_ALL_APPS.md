# UI study — all reference apps (careful read, no redesign yet)

MotionStudio **visual language decision remains EffectCraft-only**.
This document studies **every** app’s UI so we understand patterns before any further UI code.

---

## 1. EffectCraft (desktop egui + Pocket)

### Pocket (from APK assets — studied in detail)
- **Theme:** dark `#1B1C1F`, text `#C9C9CC` / `#E3E3E3`, accent **`#2D8CEB`**
- **Layout:** almost full-bleed **canvas**; chrome is minimal
- **Loading:** centered status + substatus; calm, no clutter
- **Failure:** `#device-lost` overlay — short copy, two buttons (secondary + primary blue)
- **Plugins:** drawer / plugin_manager (CSS+JS); virtual **keyboard** for mobile typing
- **Shell:** Android WebView + floating control; one MainActivity

### Desktop (ui-egui crate structure)
Major UI modules (from source tree):
- `dock.rs` / `dock_ui.rs` — dockable panel system
- `header.rs`, `menus.rs`, `menu_keys.rs` — menu / shortcuts
- `frames.rs` — composition / frame viewer area
- `control.rs` — transport / controls
- `panels/` — specialized: anim tools, camera tracker, comp settings, dialogs, content fill…
- `color_picker.rs`, `audio.rs`, `automation.rs`
- AE-style mental model: project / comp / timeline / properties / effects

**UI takeaway:** dense pro panels on desktop; pure canvas + drawers on mobile. Same product, two densities.

---

## 2. Kdenlive (NLE — desktop)

### Structure
- **Menu bar** + **toolbars** + **status bar**
- **Workspace** of dockable widgets
- Default **Editing** layout:
  1. Project Bin / Library (media)
  2. Clip Monitor + Project Monitor (dual preview common in NLEs)
  3. Timeline (center of gravity)
  4. Timeline toolbar (select, razor, slip, ripple, spacer…)
  5. Audio mixer
  6. Effects stack / properties

### Timeline anatomy
- Track headers (V/A, lock, mute, hide, effects badge)
- Ruler with zone (I/O), preview-render zones
- Clip blocks on tracks; compositions between tracks

### Workspace modes
Logging | Editing | Audio | Effects | Color — **task-specific layouts**

**UI takeaway:** timeline is king; monitors dual; effects are a **stack panel**, not only a graph.

---

## 3. OpenShot (NLE — desktop)

### Simple View (default)
- Left: Project Files / Transitions / Effects / Emojis (**tabbed library**)
- Right: Preview
- Bottom: Timeline + timeline toolbar + zoom slider
- Properties appear when needed

### Timeline
- Tracks top→bottom (overlays → video → audio convention)
- Red playhead; clip rectangles; transitions as entities
- Dock system: Simple / Color / user views

**UI takeaway:** friendlier than Kdenlive; fewer permanent panels; library is tabbed not a deep bin tree.

---

## 4. Open Video Editor (mobile — Jetpack Compose)

### From `VideoEditorScreen.kt` + screenshots metadata
- Material 3, bottom sheets, sliders, progress
- Icons: play/pause, skip, layers, more
- **ModalBottomSheet** for secondary tools (mobile pattern)
- Single main editor screen; settings separate

**UI takeaway:** mobile NLE = preview top, controls mid, timeline/tools bottom or sheet — not dual monitors.

---

## 5. Natron (node compositor)

### Core regions
- **Node graph** (center) — place, wire, backdrop
- **Viewer** — output of selected/viewed node; proxy; wipe
- **Properties panels** — per-node params, tabs (controls / node / info), local undo
- **Curve editor** + **dope sheet** for animation
- Tab-to-create-node; multi-screen layouts; `.nl` layout files

**UI takeaway:** graph is primary, not timeline. Properties are **node-centric**. Viewer is sacred.

---

## 6. Gaffer (node pipeline — studio)

### Graph editor habits
- Tab / right-click node menu
- Alt-pan, wheel zoom, F frame selected
- Connect plugs by drag; D disable; Ctrl+L layout
- Boxes for subgraphs; Node Editor for plugs

**UI takeaway:** pure pipeline UI — almost no “consumer timeline.” MS would only borrow graph interaction rules for a future node mode.

---

## 7. Blender (3D + VSE)

### Editor / region model
- Every editor: **Header + Main + optional Toolbar/Sidebar**
- **Outliner** — hierarchy of data
- **Properties** — tabbed by data type (object, material, render…)
- **Timeline / Dope Sheet / Graph Editor** for animation
- Workspaces switch whole screen purpose (Layout, Modeling, Animation, Compositing…)

**UI takeaway:** extreme modularity; MS won’t copy Blender chrome, but “workspace = task” matches Kdenlive too.

---

## 8. LMMS (music)

### Windows, not one canvas
- Song Editor (arrangement tracks)
- Piano Roll (note detail)
- Beat+Bassline (patterns)
- Mixer (FX channels)
- Instrument rack / sidebar for plugins
- Automation editor

**UI takeaway:** audio tools are **separate focused windows**. For MS audio later: mixer strip + track lanes, not a piano roll as home.

---

## 9. StykAnimate / Sprocket / Miniter / LibreCuts

- **StykAnimate:** 2D cutout / bone-style animation — stage + timeline + puppet hierarchy (puppet apps: canvas center, layers/bones side)
- **Sprocket / Miniter / LibreCuts:** lighter mobile or experimental editors — fewer panels, faster path to cut/export
- Exact pixel UI varies; shared idea: **reduce chrome, prioritize preview + timeline**

---

## 10. MoonRay / Appleseed (renderers)

- **Not NLEs.** UI is either:
  - Hydra / DCC plugins inside Maya/Houdini/Katana-like hosts, or
  - CLI + simple progressive viewer (`moonray_gui`, appleseed.studio)
- **UI takeaway for MS:** offline render = **job panel / progress**, not a third full editor skin.

---

## Cross-app pattern matrix

| Concern | Mobile NLE | Desktop NLE | Node comp | Motion/AE-like | 3D | Audio | Offline render |
|--------|------------|-------------|-----------|----------------|----|-------|----------------|
| Primary focus | Preview + timeline | Timeline + monitors | Graph + viewer | Comp viewer + layers | 3D view | Song + roll | Progress / buckets |
| Media home | Import sheet | Project bin | Read nodes | Project panel | Outliner | Sidebar | Scene file |
| Params | Bottom sheet | Effect stack | Node properties | Properties panel | Properties tabs | Instrument window | Scene vars |
| Density | Low | High | High | High | Very high | Medium | Low |

---

## What this means for MotionStudio (still EffectCraft-only *look*)

| MS surface | Visual style | Behavioral lessons from others |
|------------|--------------|--------------------------------|
| Colors / type / panel chrome | **EffectCraft only** | — |
| Mobile layout | EC Pocket (canvas + drawers) | OVE bottom sheets; avoid dual monitors |
| Desktop layout | EC dock panels | Kdenlive/OpenShot: bin + preview + timeline |
| Timeline interaction | EC + own | Kdenlive tools names/roles; OpenShot simplicity |
| Effects | EC drawer + `.msp` | Kdenlive stack; Natron when node mode exists |
| Node mode (future) | EC styling | Natron/Gaffer graph rules |
| Audio (future) | EC styling | LMMS mixer + track lanes |
| 3D / path trace (future) | EC styling | Job UI, not Blender full UI |

**Do not** mix CapCut / random consumer skins.  
**Do** learn workflow regions from Kdenlive/Natron/OpenShot/OVE while painting them with EffectCraft’s visual system.

---

## Study status

| App | UI studied |
|-----|------------|
| EffectCraft Pocket | Source + assets (deep) |
| EffectCraft desktop | Module map from ui-egui |
| Kdenlive | Official UI docs |
| OpenShot | Official UI docs |
| Open Video Editor | Compose source + screenshot assets listed |
| Natron | Docs + product description |
| Gaffer | Official controls docs |
| Blender | Manual regions/properties/outliner |
| LMMS | Official interface docs |
| MoonRay / Appleseed | Role as non-editor UIs |
| StykAnimate, Sprocket, Miniter, LibreCuts | Category-level (lighter mobile/2D) |

No UI code changed in this pass — study only, as requested.
