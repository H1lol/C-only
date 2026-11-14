# Components taken from donated open-source apps

| App | Components absorbed into MS |
|-----|----------------------------|
| **Kdenlive** | Multi-track timeline, markers, transitions (dissolve/wipe/fade), track lock/mute/solo patterns |
| **OpenShot** | Keyframe curves, project JSON shape, time remapping ideas |
| **Open Video Editor** | Mobile-friendly media import + clip placement workflow |
| **LibreCuts / Miniter** | Simple cut / split / export path for mobile NLE |
| **Sprocket** | Lightweight clip timeline model |
| **Natron** | Node graph ops: merge, grade, blur, constant, viewer |
| **EffectCraft** | Effect param bags, EffectCraft UI colors, AE-oriented effect ids |
| **StykAnimate** | Stick/bone hierarchy + joint angle curves |
| **LMMS** | Pattern clip / note-like audio region structure |
| **Gaffer** | External lookdev job export |
| **MoonRay / Appleseed** | Offline render backend stubs + job files |
| **Blender** | Scene (camera/light/mesh) + Blender job script |
| **Unreal** | Scene object / remote job pattern (not full engine) |

MS does **not** copy GPL sources verbatim into the binary; it reimplements **ideas and structures** in C++ under MS license, and shells to binaries where appropriate.
