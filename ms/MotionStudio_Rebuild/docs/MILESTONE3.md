# Milestone 3 — Effects & transitions host (PASSED)

## Gate G3
Every built-in effect has an equivalent and applies; keyframes work; frei0r catalog discovered; OpenFX scan API present.

```
M3 PASSED
catalog=150  frei0r=136
all builtins apply + keyframe lerp + stack
```

## API
```cpp
EffectsHost::instance().init();
FxInstance blur;
blur.id = "gaussian_blur";
blur.set_key("radius", 0, 1);
blur.set_key("radius", 1, 12);
host.apply(blur, 0.5, in, out);
host.apply_stack(stack, t, in, out);
host.scan_openfx("/path/to/ofx");
```

## Backends
| Backend | Role |
|---------|------|
| BuiltinCpu | Full pixel path for all MS builtins |
| Frei0rMlt | 100+ plugins discovered; mapped apply |
| OpenFX | Discovery stub — full host later |

## Next: Milestone 4 — Export (MediaCodec / presets / lossless cut)
