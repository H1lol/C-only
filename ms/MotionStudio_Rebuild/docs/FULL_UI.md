# Full C++ UI — shipped

Binary: `ms_app` (Linux command-driven front-end to the full panel set).

## Panels (14)

| Panel | Workspaces | Bound to |
|-------|------------|----------|
| PROJECT | Edit Motion Nodes Color | ProjectSession media/tracks |
| COMPOSITION | Edit Motion Nodes Color Render | ImageBuffer composite |
| TIMELINE | Edit Audio | tracks/clips + playhead |
| PROPERTIES | Edit Motion Nodes Color | Selection |
| LAYERS | Motion | LayerStack |
| EFFECTS & PRESETS | Motion Edit | MspRegistry + builtins |
| NODE GRAPH | Nodes | nodes/edges model |
| SCOPES | Color | luma analysis of viewer frame |
| GRADE | Color | exposure/contrast/sat/OCIO label |
| MIXER | Audio | strips |
| RENDER QUEUE | Render | jobs + backends |
| TERMINAL | Dev | real `popen` shell |
| COPILOT | Dev | local assist + suggestions |
| PROBLEMS | Dev Render | Error bus |

## Workspaces
Edit · Motion · Nodes · Color · Audio · Render · Dev

Visibility is enforced by `workspace_mask()` — empty panels are not “shown empty”; they are **hidden**.

## Build
```bash
g++ -std=c++17 -I native/cpp/include \
  native/cpp/ui/*.cpp native/cpp/ui/panels/*.cpp \
  native/cpp/effects/cpu_effects.cpp native/cpp/comp/layer_stack.cpp \
  native/cpp/effects/msp/msp_registry.cpp \
  native/cpp/render/image_buffer.cpp native/cpp/render/render_backends.cpp \
  native/cpp/engine/project_session.cpp \
  native/cpp/tools/ms_app.cpp \
  -o ms_app
```

Qt (or other) can later implement widgets that call the same `IPanel` methods — logic is already here.
