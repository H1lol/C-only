# MotionStudio Rebuild — Architecture (2026)

## Goal
Full mobile NLE + Motion Graphics + 2D Animation + Node Compositing app  
UI quality target: CapCut  
Engine power target: between Alight Motion and a light Kdenlive/Natron hybrid

## Technology Stack

### UI Layer
- **Flutter (Dart)** — primary UI
- Custom C++ rendered widgets where needed (timeline, node graph, preview) via Texture / PlatformView
- Design language: CapCut-inspired (dark cinematic, purple accent, fluid gestures)

### Native Engine
- **C++** — performance critical path
  - Video decode / encode (MediaCodec on Android, VideoToolbox on iOS)
  - OpenGL ES 3.0 / Vulkan renderer
  - Effect pipeline (GLSL / SPIR-V)
  - Real-time preview + export

- **Rust** — safe systems layer
  - Project model & serialization (`.msproj`)
  - Node graph evaluation
  - Effect registry & parameter system
  - Undo / Redo command stack
  - Multi-track timeline data model
  - Background task orchestration

### Language Bridge
```
Flutter (Dart)
    ↕  Platform Channels + FFI
C++ Engine (render + media)
    ↕  cxx / cbindgen
Rust Core (logic + safety)
```

## High-Level Modules

1. **Project & Timeline** (Rust)
2. **Node Graph / Compositor** (Rust + C++)
3. **Effect System** (Rust registry + C++ GLSL/Vulkan)
4. **Media Pipeline** (C++)
5. **Preview Surface** (C++ → Flutter Texture)
6. **Export Engine** (C++)
7. **UI Shell** (Flutter)

## Feature Mapping from References

| Feature Area              | Inspired From              | Implementation Plan          |
|--------------------------|----------------------------|------------------------------|
| Multi-track NLE          | Kdenlive, Sprocket, Miniter| Rust timeline + C++ playback |
| CapCut-style UI          | CapCut                     | Flutter + custom painters    |
| Node compositing         | Natron, EffectCraft, Gaffer| Rust node graph + C++ eval   |
| 2D Animation             | StykAnimate                | Keyframe system in Rust      |
| Motion Graphics          | EffectCraft, After Effects | Effect graph + expressions   |
| 3D (future)              | Blender                    | Optional glTF import later   |
| Audio                    | LMMS ideas                 | Basic multi-track audio first|

## Directory Structure

```
MotionStudio_Rebuild/
├── flutter_ui/                 # Flutter app (main UI)
├── native/
│   ├── cpp/                    # High-performance engine
│   │   ├── engine/
│   │   ├── render/
│   │   ├── codec/
│   │   ├── effects/
│   │   └── bridge/
│   └── rust/                   # Safe core logic
│       └── crates/
│           ├── ms_core
│           ├── ms_project
│           ├── ms_nodegraph
│           └── ms_effects
├── docs/
└── scripts/
```

## Build Targets
- Android (primary)
- iOS (secondary)
- Desktop (debug only)

## Version
MotionStudio 2.0 — Full Rewrite
