# Effects: offline + online (H1lol)

## Offline mode (default)
Built-in CPU effects (no network):

- gaussian_blur, brightness_contrast, opacity, invert  
- **grayscale, sepia, vignette, mirror** (new)  
- chromakey  

Transitions still available offline (dissolve, wipes, slides, …).

## Online mode
```cpp
OnlineEffectsCatalog cat;
cat.set_mode(EffectsMode::Online);
cat.refresh();                 // pulls catalog from GitHub
cat.download_effect("id");     // downloads .msp into plugins/online/
```

### Default GitHub target (your account)
```
https://raw.githubusercontent.com/H1lol/ms-effects/main/catalog.json
```

Fallback tried:
```
https://raw.githubusercontent.com/H1lol/Source-codes/main/ms-effects/catalog.json
```

### What you should create on GitHub
Repo: **`H1lol/ms-effects`** (public)

```
ms-effects/
  catalog.json          # same shape as MotionStudio_Rebuild/catalog/catalog.json
  effects/
    grayscale.msp
    sepia.msp
    ...
```

Ship `catalog.json` from this tree as a starting point.

## Switch
| Mode | Behavior |
|------|----------|
| Offline | Builtin only |
| Online | Catalog + on-demand download of every listed effect |
