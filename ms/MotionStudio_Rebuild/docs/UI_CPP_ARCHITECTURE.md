# MotionStudio C++ UI architecture
**Stack:** C++ (engine + UI) · little Python (scripting, catalog, copilot) · Linux-first  
**Look:** EffectCraft only (`#1B1C1F`, `#232326`, `#2D8CEB`)  
**Rule:** every panel has one job. If it doesn’t change a decision or a pixel, it doesn’t ship.

---

## 1. Design law

1. **One primary focus per workspace** — edit, composite, grade, audio, render, or code.
2. **Preview is sacred** — never shrink the viewer below usefulness to show vanity chrome.
3. **Panels answer questions** — not “look busy.”
4. **Density follows skill** — mobile/simple vs desktop/pro via *layout*, same widgets.
5. **Open source honesty** — terminal + copilot are first-class, not gimmicks.

---

## 2. Workspaces (fit “everything” without one overloaded screen)

| Workspace | User question | Panels that belong |
|-----------|---------------|--------------------|
| **Edit** | What is the cut? | Project · Viewer · Timeline · Inspector |
| **Motion** | What is the comp? | Project · Viewer · Layer stack · Effects · Inspector |
| **Nodes** | How is the graph wired? | Graph · Viewer · Node props |
| **Color** | How does it look? | Viewer · Scopes · Grade / OCIO |
| **Audio** | How does it sound? | Timeline (audio lanes) · Mixer · Meter |
| **Render** | Is the job done? | Queue · Log · Viewer (last frame) |
| **Dev** | What’s broken / what to run? | Terminal · Copilot · Problems |

Switching workspace **hides** panels that don’t serve that question.  
That’s how Kdenlive/Blender stay usable — we steal the *idea*, paint it EffectCraft.

---

## 3. Panel catalog — each one’s worth

### A. Project
- **Worth:** “What assets and comps exist?”
- **Shows:** bins, comps, footage, solids, `.msp` installed list
- **Actions:** import, new comp, search, open
- **Not:** effect parameters, graph wiring

### B. Viewer (Composition)
- **Worth:** “What does the frame look like *now*?”
- **Shows:** canvas, safe frames, resolution, color space label, playhead time
- **Actions:** fit / 1:1, channel view (RGB/A), compare wipe (later)
- **Not:** long property lists

### C. Timeline
- **Worth:** “When does it happen?”
- **Shows:** tracks, clips, layer bars, playhead, in/out
- **Actions:** trim, slip, razor, ripple, zoom, snap
- **Not:** shader code, full mixer EQ

### D. Layer stack (Motion workspace)
- **Worth:** “What is stacked in this comp?”
- **Shows:** ordered layers, visibility, lock, blend mode badge
- **Actions:** reorder, solo, mute, add adjustment layer
- **Not:** node graph (that’s Nodes workspace)

### E. Effects / Presets
- **Worth:** “What treatment can I add?”
- **Shows:** categories, search, installed vs downloadable `.msp`, open-source badge
- **Actions:** apply, download, open source link
- **Not:** timeline editing

### F. Inspector (Properties)
- **Worth:** “What is selected, exactly?”
- **Shows:** transform, opacity, blend, effect params for selection only
- **Actions:** scrub values, keyframe toggle, reset
- **Not:** global preferences

### G. Node graph (Nodes workspace only)
- **Worth:** “How do passes connect?”
- **Shows:** nodes, edges, backdrops, disabled state
- **Actions:** Tab-add, connect, disable (D), frame (F)
- **Not:** song arrangement

### H. Mixer (Audio workspace)
- **Worth:** “How loud / balanced?”
- **Shows:** track strips, peak meters, pan, mute/solo
- **Actions:** level, route (later)
- **Not:** video effects

### I. Render queue
- **Worth:** “What is exporting or path-tracing?”
- **Shows:** jobs, progress, backend (realtime / moonray / appleseed), errors
- **Actions:** enqueue, cancel, open output folder
- **Not:** creative grading tools

### J. Terminal (Dev / power)
- **Worth:** “Run real Linux commands in context of this project.”
- **Shows:** PTY shell, cwd = project root
- **Actions:** build tools, `ms-render`, git, python scripts
- **Not:** a fake console that can’t run anything

### K. Copilot (Dev / assist)
- **Worth:** “Explain error, propose patch, or run a safe fix.”
- **Shows:** chat, diff proposals, “apply” / “run in terminal”
- **Actions:** fix compile error, suggest `.msp` manifest, explain log line
- **Not:** unsupervised silent writes to the whole disk

### L. Problems / Log
- **Worth:** “What failed?”
- **Shows:** ErrorCode list, file:line if any, shader compile log
- **Actions:** jump to panel, copy, send to copilot
- **Not:** marketing tips

---

## 4. Shell layout (desktop Linux)

```
┌─ Menu (File Edit View Workspace Render Help) ─────────────────────┐
├─ Tool strip (workspace-specific, short) ──────────────────────────┤
├────────────┬──────────────────────────────┬───────────────────────┤
│  Project   │         Viewer               │  Inspector            │
│            │                              │                       │
│            ├──────────────────────────────┤                       │
│            │  Timeline  or  Graph  or     │                       │
│            │  Mixer     (by workspace)    │                       │
├────────────┴──────────────────────────────┴───────────────────────┤
│  optional: Terminal | Copilot | Problems   (Dev strip / drawer)   │
└───────────────────────────────────────────────────────────────────┘
```

**Motion workspace** swaps Timeline for Layer stack + keeps Effects drawer.  
**Nodes** swaps Timeline region for Graph.  
**Render** maximizes Queue + Log.  
**Dev** maximizes Terminal + Copilot.

Every region above maps to a C++ panel class with a single responsibility.

---

## 5. C++ panel model (meaningful code, not empty frames)

```text
IPanel
  name()
  workspace_mask()   // which workspaces show this panel
  build_ui(parent)
  on_selection_changed()
  on_time_changed()
  on_project_changed()
```

| Class | Owns |
|-------|------|
| `ProjectPanel` | asset list model |
| `ViewerPanel` | framebuffer present, time readout |
| `TimelinePanel` | track UI ↔ session tracks |
| `LayerPanel` | LayerStack binding |
| `EffectsPanel` | `.msp` catalog + apply |
| `InspectorPanel` | selection → params |
| `GraphPanel` | node document |
| `MixerPanel` | audio levels |
| `RenderQueuePanel` | IRenderBackend jobs |
| `TerminalPanel` | PTY |
| `CopilotPanel` | Python bridge |
| `ProblemsPanel` | Error bus |

If a class doesn’t implement real data binding, it doesn’t land in the default layout.

---

## 6. Fitting “all the apps” without Frankenstein chrome

| Capability | UI home | Not in UI as |
|------------|---------|--------------|
| Kdenlive-like edit | Edit workspace | Extra random toolbars |
| EffectCraft motion | Motion workspace | CapCut-style tabs |
| Natron/Gaffer nodes | Nodes workspace | Always-on graph |
| LMMS audio | Audio workspace | Piano roll as default |
| MoonRay/Appleseed | Render queue backend picker | Second full 3D suite skin |
| Mobile OVE simplicity | Future compact layout preset | Separate product UI language |

**One visual system (EffectCraft). Many workspaces. Shared inspector.**

---

## 7. Love criteria (panel QA)

Ship a panel only if you can answer yes:

- [ ] Does removing it break a real task?
- [ ] Does it update when time / selection / project changes?
- [ ] Can a new user say what it’s for in one sentence?
- [ ] Does it use EffectCraft colors/spacing, not a one-off style?
- [ ] Is there a keyboard path to the main action?

---

## 8. Implementation order (UI)

1. Shell + workspaces + Project + Viewer + Timeline + Inspector  
2. Effects panel + `.msp` apply  
3. Layer panel (Motion)  
4. Render queue + Problems  
5. Terminal  
6. Copilot bridge (Python)  
7. Graph (Nodes)  
8. Mixer (Audio)

---

## 9. What we are *not* doing

- Flutter / Kotlin / Compose / Java UI  
- CapCut clone chrome  
- Every panel visible at once  
- Decorative sidebars with no model behind them  

**Every line of C++ UI should bind to engine state or user intent.**  
That’s how a panel knows its worth.
