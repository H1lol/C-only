# On-Demand Effects System

## Goal
Effects live in a **public repository**.  
The app only ships a small built-in set.  
Users download extra effects on demand.

## Flow

```
User opens Effect Browser
        ↓
App loads remote catalog (JSON)
        ↓
User taps "Download" on an effect
        ↓
App downloads effect package (.mseffect or folder)
        ↓
Package is verified + extracted to local cache
        ↓
Effect is registered in the runtime EffectRegistry
        ↓
User can now apply it like any built-in effect
```

## Public Repo Structure (example)

```
https://raw.githubusercontent.com/your-org/motionstudio-effects/main/
├── catalog.json                 # master list of all effects
├── effects/
│   ├── blur_gaussian/
│   │   ├── manifest.json
│   │   ├── blur.frag
│   │   ├── blur.vert
│   │   └── thumb.png
│   ├── glitch_rgb/
│   │   ├── manifest.json
│   │   ├── glitch.frag
│   │   └── ...
│   └── ...
└── README.md
```

## Effect Package (`manifest.json`)

```json
{
  "base_id": "glitch_rgb",
  "display_name": "RGB Glitch",
  "version": "1.0.0",
  "category": "Stylize",
  "description": "RGB channel split glitch",
  "author": "MotionStudio",
  "license": "MIT",
  "gpu_cost": "medium",
  "min_app_version": "2.0.0",
  "params": [
    { "name": "amount", "type": "float", "default": 0.1, "min": 0.0, "max": 1.0, "label": "Amount" },
    { "name": "speed",  "type": "float", "default": 1.0, "min": 0.0, "max": 5.0, "label": "Speed" }
  ],
  "shaders": {
    "vert": "glitch.vert",
    "frag": "glitch.frag"
  },
  "thumbnail": "thumb.png",
  "tags": ["glitch", "stylize"]
}
```

## Local Cache Layout (on device)

```
/data/.../effects_cache/
├── catalog_cache.json
├── glitch_rgb/
│   ├── manifest.json
│   ├── glitch.frag
│   ├── glitch.vert
│   └── thumb.png
└── ...
```

## Components

| Layer        | Responsibility                          |
|--------------|-----------------------------------------|
| Flutter      | UI (store browser, download button, progress) |
| Dart/HTTP    | Download catalog + packages             |
| C++ / Rust   | Register downloaded effects, load shaders |
| Python tools | Generate catalog, validate packages     |

## Security
- Only download from the configured official repo URL
- Verify package checksum (SHA-256 in catalog)
- Sandbox shader loading (no arbitrary code execution)
