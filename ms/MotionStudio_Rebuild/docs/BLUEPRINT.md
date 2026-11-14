# MotionStudio Blueprint (Official)

**Version:** 2.0  
**Date:** October 2026  
**Status:** Active development

## Phases

### 1. Project Foundation (Current Focus)
- Real project files (`.msproj` JSON + binary assets)
- Media import (video, image, audio)
- Timeline (multi-track)
- Playback (seek, play, pause, loop)
- Undo / Redo (command pattern)
- Autosave

### 2. Rendering Foundation
- Image buffers (CPU + GPU)
- Color handling (sRGB, linear, HDR later)
- GPU preview (OpenGL ES / Vulkan)
- Effect evaluation
- Caching (proxy + frame cache)

### 3. Compositing
- Layers
- Masks
- Keying
- Transforms
- Tracking
- Functional node graph

### 4. Animation & Motion Graphics
- Keyframes
- Graph editor
- Text
- Shapes
- Expressions
- Motion blur

### 5. Professional Finishing
- Color grading
- Audio
- Advanced effects
- Render queue
- Export

### 6. 3D & Procedural Systems
- Modeling
- Sculpting
- Rigging
- Cameras
- Lighting
- Simulations

### 7. Extensibility & Advanced Tools
- Plugin SDK
- Scripting
- AI assistance
- Interoperability
- Pipeline tools

## Stack Reminder
- **UI:** Flutter (Dart)
- **Engine (perf):** C++
- **Safe core:** Rust
- **Tools:** Python

## Current Target
Complete **Phase 1** solidly before moving to Phase 2.
