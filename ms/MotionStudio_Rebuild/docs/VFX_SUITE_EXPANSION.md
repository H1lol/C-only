# VFX suite expansion — what we took (important parts only)

MotionStudio does **not** paste full Unreal / Blender / Kdenlive trees.
It takes **systems and patterns** that a suite needs.

## From Unreal Engine (your source zip / engine knowledge)
- Scene object types: camera, light, mesh ref, transform hierarchy
- Render settings job (resolution, samples, backend name)
- **UnrealRemoteBackend** — job description only; UE stays external
- Asset reference strings (not full UAsset pipeline)

## From Blender
- Workspace-by-task idea (already)
- Scene / object / camera data thinking
- Color management space names (ACES, sRGB, Linear)

## From Natron / Gaffer
- **NodeGraph** with ops: Read, Merge, Blur, Grade, Viewer, Write
- Graph evaluate → ImageBuffer
- Disable node, connect edges

## From OpenShot / Kdenlive
- **AnimCurve / Keyframe** with linear/ease/hold
- Media probe + decoder interface (FFmpeg-shaped)
- Timeline already present

## From EffectCraft
- UI tokens, .msp effects, motion grade mindset

## From LMMS
- Mixer strips (already)

## From Appleseed / MoonRay / LuxCore (your releases)
- Offline backend slots + job JSON
- Samples / progressive / color_space on RenderSettings

## From mobile editors (OVE, Miniter, …)
- Density discipline only

## New code modules
| Module | Path |
|--------|------|
| Keyframes | `include/ms_keyframe.hpp` |
| Media probe/decoder | `include/ms_media.hpp`, `media/media_probe.cpp` |
| Node graph eval | `include/ms_node_graph.hpp`, `graph/node_graph.cpp` |
| Scene graph | `include/ms_scene.hpp`, `scene/scene.cpp` |
| Color pipeline | `include/ms_color_pipeline.hpp`, `color/color_pipeline.cpp` |
| Extra CPU FX | invert, threshold, simple chromakey |
| UE remote backend | name `unreal_remote` |

Still missing for a *full* commercial suite: real FFmpeg link, GPU shaders, farm, OCIO config files, tracking, roto tools, deep EXR, facility pipeline. Those are next passes — structure is no longer “too short” for a serious foundation.
