# Motion Studio Milestones 17–22

## 17 — 3D layers
Native scene objects now support cameras, lights, mesh references, parenting metadata, transforms and render settings.

## 18 — Cameras, lights and materials
Camera optics and light controls are represented in the scene model. Mesh layers carry a PBR-style material record (base color, metallic, roughness, emission and texture reference) and export to DCC job JSON.

## 19 — Blender render integration
Motion Studio writes a self-contained Blender job directory containing a scene snapshot and Python job. When Blender is installed, `DccBridge::submit(..., dry_run=false)` launches Blender in background mode and the job renders `render/frame_0001.png`. Blender remains an external DCC; it is not embedded in the mobile binary.

## 20 — Color management
`ColorPipeline` provides named working/display spaces, linear/sRGB-family transforms, exposure/contrast/saturation grading, and an explicit display-transform operation. This is the native baseline for a future OpenColorIO-backed implementation.

## 21 — OpenFX architecture
`OpenFxRegistry` provides plugin discovery/registration, capability checking, parameter passing and image processing through a stable Motion Studio host boundary. A built-in identity plugin validates the path without requiring an external SDK.

## 22 — Background render queue
`RenderQueue` supports queued jobs plus an asynchronous worker (`start_async`, `wait`, `running`) for non-blocking background rendering. Existing synchronous processing remains available for deterministic tests and command-line workflows.

### Verification
`ms_17_22_test` passes all 17–22 checks in the native C++ build.
