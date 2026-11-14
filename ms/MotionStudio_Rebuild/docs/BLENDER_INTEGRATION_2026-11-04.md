# Blender 5.2.2 integration

## Selected upstream material

Motion Studio retains a small, attributed reference set from the supplied Blender 5.2.2 source archive:

- `ANIM_fcurve.hh` and `ANIM_keyframing.hh` — animation curves and keyframing API design.
- `COM_scheduler.hh` — compositor scheduling concepts.
- `NOD_geometry_exec.hh` and `NOD_composite.hh` — node execution/compositor API design.
- `GEO_mesh_boolean.hh` and `GEO_mesh_to_volume.hh` — geometry operation API design.

These headers are reference material only. They are not compiled into Motion Studio because Blender's modules depend on Blender's internal build system, generated headers, and many internal libraries. Blender remains an optional external DCC/render process.

## Motion Studio changes

- Scene JSON now serializes full translation, quaternion rotation, scale, parent ID, visibility, camera clipping/sensor data, light type/color/intensity, mesh asset path, and material properties.
- JSON strings are escaped, including quotes, slashes, and control characters.
- Generated Blender Python jobs support OBJ, FBX, glTF/GLB, STL, and USD asset import where the corresponding Blender operator is available.
- Mesh references receive a Principled BSDF material with base color, metallic, roughness, emission strength, and optional image texture.
- Cameras and lights carry their scene properties into Blender.
- Parent relationships are applied after object creation so forward references work.
- Missing or unsupported mesh assets use a clearly logged cube fallback; volume objects currently export bounds only and need a VDB asset integration for true volume rendering.
- Blender performs the offline Cycles render when the job is launched with Blender.

## Validation

- Native C++ Release build: passed.
- CTest: 10/10 passed.
- DCC test checks generated Blender script features and serialized scene properties.
- Blender itself was not installed in the build environment, so a real `.blend` session and Cycles render were not executed here.

## Licensing

Blender is GPL-3.0-or-later. The selected upstream headers and the upstream license texts are preserved in `third_party/upstream_reference_sources/blender-5.2.2/`. The reference headers are not compiled or linked. This is not legal advice; review the complete upstream license and notices before redistribution.
