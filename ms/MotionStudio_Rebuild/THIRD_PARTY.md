# Third-party components and upstream references

Motion Studio is licensed under **GPL-3.0-or-later** (see `LICENSE`). This table distinguishes references from code that is actually compiled or linked. A project being listed here does not mean its runtime has been embedded.

| Project | Upstream license in supplied archive | Retained material | Relationship to Motion Studio |
|---|---|---|---|
| Sprocket 0.1.101-alpha | MIT | Selected architecture, model, timeline, and audio-effect source references plus `LICENSE` | Reference only. The new native audio DSP is an independent C++ implementation; the C# files are not compiled. |
| Gaffer 1.7.4.0 | BSD-style | Selected graph-model/UI references plus `LICENSE` | Reference only. MS graph code is independently implemented. |
| Kdenlive master | GPL-3.0 | `COPYING`, `AUTHORS`, selected timeline-model source and profile data | Source references only, not built into MS. Compatible with MS's GPL-3.0-or-later licensing; preserve notices when redistributing those files. |
| appleseed master-clean | MIT | `LICENSE`, documentation and sample project/CLI references | Optional offline-renderer handoff. Renderer itself is not bundled. |
| LMMS 1.2.2 | GPL-2.0-only | License and design-reference note only | No implementation source imported; reviewed for mixer/effect-rack workflow ideas only. |
| Blender 5.2.2 | GPL-3.0-or-later (see upstream `COPYING` and `GPL-license.txt`) | Selected animation, compositor, node, and geometry API headers under `third_party/upstream_reference_sources/blender-5.2.2/selected/`; original license files retained | Reference-only headers; not compiled or linked. Motion Studio integrates with Blender as an external process through a generated Python scene job. |
| OpenMoonRay | Apache-2.0 | Renderer integration reference | Optional external renderer path; no renderer source is embedded by this change set. |
| MLT | LGPL/GPL components (varies by component/build) | Optional timeline integration | Check exact modules/build configuration and corresponding notices when distributing binaries. |
| FFmpeg | LGPL or GPL depending on build configuration | CLI and optional libav integration | Distribution obligations depend on the exact build and enabled libraries. |
| frei0r | LGPL | Effect discovery/plugin integration | Plugins remain external; check the individual plugin license. |
| OpenFX SDK | BSD-style (upstream SDK terms) | Host interface boundary | Check the exact SDK version and included notices in a redistributed build. |
| Inter 4.1 | SIL OFL 1.1 | Static TTFs (`inter_regular/medium/semibold/bold`) + license | **Bundled and used** by the Kotlin/Compose UI as the interface typeface. See `third_party/fonts/README.md`. |
| JetBrains Mono 2.304 | SIL OFL 1.1 | Static TTFs (`jbmono_regular/medium/bold`) + license | **Bundled and used** for timecode, ruler and numeric readouts. See `third_party/fonts/README.md`. |
| stb_truetype 1.21 | Public domain (MIT alternative) | `native/cpp/third_party/stb_truetype.h` | **Bundled and used** by `text/text_render.cpp` to rasterise titles. Copied from the vendored appleseed source tree, which only wraps it. |
| Anton, Bebas Neue, Archivo Black, Oswald, Playfair Display, Lobster | SIL OFL 1.1 | `android/app/src/main/assets/text/*.ttf` + licences in `third_party/fonts/titles/` | **Bundled and used** as the title typefaces. Variable files (Oswald, Playfair) are used at their default instance. No font data was modified. |
| `.cube` looks | GPL-3.0-or-later (Motion Studio) | Six 17³ LUTs baked by `scripts/gen_looks.py` | Project-authored: no third-party LUT data is redistributed. Applied by `native/cpp/color/lut.cpp`. |

## Reference file location

Selected upstream source excerpts and their original license files are kept under:

`third_party/upstream_reference_sources/`

See `docs/UPSTREAM_INTEGRATION_2026-11-04.md` for the file-level selection, implementation notes, and validation scope. Full uploaded upstream archives are not included in the Motion Studio repository.

`third_party/curated_sources/` used to hold complete copies of those archives (~292 MB,
19,611 files). Nothing built or read them, and they contradicted the paragraph above, so they were
dropped from the tree; the upstream projects and their licenses remain listed in the table
here, and the selected excerpts are still in `third_party/upstream_reference_sources/`.

## Important license boundary

The supplied LMMS 1.2.2 archive is GPL-2.0-only. Its implementation files were deliberately not copied into this GPL-3.0-or-later project. Do not add LMMS implementation source unless a compatible licensing arrangement is obtained or the project licensing strategy is changed after legal review.

This document is engineering attribution, not legal advice. Before binary redistribution, audit exact optional dependency builds, plugin licenses, bundled assets/fonts, and all required notices.
