# Enhancement 0.6 — peak integration

## Added
- **StudioHub** — one object for project, player, layers, queue, DCC, audio, scene, Copilot, MSP
- **catalog/catalog.json** — downloadable effects index
- **examples/plugins/demo_blur** — MSP-style folder
- **Full CMakeLists** listing all core units
- **Branding** logo path in StudioInfo
- **Preview** composites layer stack over timeline frame
- Integration test: `ms_studio_test` → **ENHANCED PASSED**

## Architecture

```
StudioHub
├── ProjectSession (NLE)
├── TimelinePlayer (media frames)
├── LayerStack + Masks + Transforms
├── RenderQueue + backends
├── Scene + DccBridge (Blender/MoonRay/Gaffer)
├── AudioMixer
├── CopilotService (local)
└── MspRegistry
```

## Honest limits
- Video still slate until FFmpeg linked
- MoonRay/Gaffer/Blender need install for live launch
- UI panels exist but desktop GUI framework not the focus of this pass
- AI cloud optional later
