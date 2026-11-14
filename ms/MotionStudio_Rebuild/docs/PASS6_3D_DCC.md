# Pass 6 — 3D & procedural (Blender / MoonRay / Gaffer)

## Design
MotionStudio **owns the scene graph**. Heavy 3D renderers stay **external binaries**:

| Tool | Role | MS writes |
|------|------|-----------|
| **Blender** | Modeling, Cycles/EEVEE render | `ms_blender_job.py` + `scene_job.json` |
| **MoonRay** | Film path tracer | `ms_moonray_job.rdla` + `run_moonray.sh` |
| **Gaffer** | Node-based lighting/lookdev | `ms_gaffer_job.py` + `run_gaffer.sh` |

`DccBridge::submit(..., dry_run=true)` always writes jobs.  
With binaries installed and `dry_run=false`, MS launches them.

## Scene
- Camera, lights (point/dir/spot/area), mesh refs, locators
- `make_default_studio_scene()`
- `Scene::to_job_json()` shared by all DCC targets

## Branding
- `assets/branding/ic_motion_studio_logo.png` — official MS logo

## Not in this pass (honest)
- Full RDL authoring, Blender bpy auto-render in CI without Blender
- Embedding MoonRay/Gaffer libraries into the MS binary
- Sculpt/rig/sim solvers (still future)

## Test
```bash
g++ -std=c++17 -I native/cpp/include \
  native/cpp/scene/scene.cpp native/cpp/dcc/dcc_bridge.cpp \
  native/cpp/render/image_buffer.cpp native/cpp/tools/ms_pass6_test.cpp \
  -o ms_pass6_test && ./ms_pass6_test
```

On a workstation with tools:
```bash
export PATH="/path/to/blender:/path/to/moonray:/path/to/gaffer:$PATH"
# then submit with dry_run=false from app/UI
```
