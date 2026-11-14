# MotionStudio Plugin Format (`.msp`)

**Version:** 1.0  
**License recommendation:** Open source (MIT / Apache-2.0 / GPL-3.0 compatible with the app)

MotionStudio is fully open source. Official and featured plugins are open source.
Third-party `.msp` authors are **strongly recommended** to release source under an OSI license.
The store can prefer / badge open plugins.

---

## What is an `.msp`?

A single **zip** archive renamed to `.msp` (or kept as `.msp` with zip contents).

```
my_effect.msp          ← zip container
├── manifest.json      ← required
├── icon.png           ← optional 256×256
├── README.md          ← optional (shown in store)
├── LICENSE            ← recommended
├── shaders/           ← GLSL / MSL / WGSL
│   └── effect.frag
├── native/            ← optional platform binaries
│   ├── android-arm64/libeffect.so
│   ├── ios-arm64/libeffect.dylib
│   └── linux-x64/libeffect.so
├── wasm/              ← optional sandboxed effect
│   └── effect.wasm
└── assets/            ← LUTs, textures, etc.
```

---

## manifest.json (required)

```json
{
  "msp_version": 1,
  "id": "com.example.gaussian_blur",
  "name": "Gaussian Blur",
  "version": "1.0.0",
  "author": "Your Name",
  "license": "MIT",
  "license_url": "https://opensource.org/licenses/MIT",
  "source_url": "https://github.com/you/ms-gaussian-blur",
  "description": "Fast separable Gaussian blur",
  "category": "Blur & Sharpen",
  "tags": ["blur", "soft"],
  "min_app_version": "2.0.0",
  "entry": {
    "type": "shader",
    "shader": "shaders/effect.frag",
    "params_schema": "params.json"
  },
  "params": [
    {
      "id": "radius",
      "label": "Radius",
      "type": "float",
      "default": 4.0,
      "min": 0.0,
      "max": 64.0
    }
  ],
  "open_source": true
}
```

### Entry types

| `entry.type` | Meaning |
|--------------|---------|
| `shader` | Fragment shader path; app binds standard uniforms |
| `wasm` | WASM module with EffectCraft-compatible / MS ABI |
| `native` | Load `.so` / `.dylib` for current platform |
| `composite` | Chain of other effect ids (meta-plugin) |

---

## Standard shader uniforms (entry.type = shader)

```
uniform sampler2D uInput;
uniform vec2 uResolution;
uniform float uTime;
uniform float uParam_<id>;   // one per param
```

Output: `out vec4 fragColor;` (or write to binding 0).

---

## Install location (on device)

```
<app_docs>/plugins/<id>/
  manifest.json
  ...extracted files...
```

Catalog index: public repo `catalog.json` lists remote `.msp` URLs for on-demand download.

---

## Packaging

```bash
python native/python/tools/msp_pack.py ./examples/plugins/sample_blur -o gaussian_blur.msp
```

## Validation

```bash
python native/python/tools/msp_pack.py --validate gaussian_blur.msp
```
