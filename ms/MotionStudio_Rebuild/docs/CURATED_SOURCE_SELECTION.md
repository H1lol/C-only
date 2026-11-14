# Motion Studio curated source selection

Only source files useful to Motion Studio editor/engine integration are included. Full upstream repositories are intentionally not vendored. Tests, build output, CI metadata, generated files, docs, and unrelated application code are excluded.

## Included slices

- LuxCore-main: 796 files, 13.1 MiB
- Natron-2.5.0: 1497 files, 18.4 MiB
- Sprocket-0.1.101-alpha: 4 files, 0.0 MiB
- StykAnimate-App-0.5.5: 39 files, 0.6 MiB
- appleseed-master-clean: 1816 files, 23.7 MiB
- blender-5.2.2: 1808 files, 22.3 MiB
- effectcraft-main: 9 files, 1.4 MiB
- enzo-0.6.0: 3740 files, 2.4 MiB
- gaffer-1.7.4.0: 2927 files, 24.7 MiB
- kdenlive-master: 1329 files, 10.6 MiB
- lmms_1.2.2: 3809 files, 64.9 MiB
- openshot-qt-4.0.1: 1773 files, 59.6 MiB

Each upstream license/notice found at repository root is preserved alongside its selected source slice. Review compatibility before compiling/linking any component.

## Explicit exclusions

- Motion Studio native Rust layer was removed; the active architecture remains Flutter + C++ + Python + HTML/CSS.
- EffectCraft Rust implementation is not vendored as engine code; only non-Rust assets/UI material was selected.
- Full upstream applications, binaries, tests, CI/build systems, generated output, and unrelated desktop UI were excluded.
