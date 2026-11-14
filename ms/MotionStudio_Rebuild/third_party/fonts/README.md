# Bundled fonts

These are the only third-party *binary assets* shipped in the Android app, and
both are SIL Open Font License 1.1, which permits bundling in a GPL-3.0 work as
long as the license travels with them and the fonts are not sold on their own.

| Family | Version | Upstream | License | Files |
|---|---|---|---|---|
| Inter | 4.1 | https://github.com/rsms/inter | SIL OFL 1.1 (`inter/LICENSE.txt`) | `inter_regular`, `inter_medium`, `inter_semibold`, `inter_bold` (static TTFs from `extras/ttf/`) |
| JetBrains Mono | 2.304 | https://github.com/JetBrains/JetBrainsMono | SIL OFL 1.1 (`jetbrains-mono/OFL.txt`) | `jbmono_regular`, `jbmono_medium`, `jbmono_bold` (static TTFs from `fonts/ttf/`) |

Copied verbatim into `android/app/src/main/res/font/` (lowercase, underscore names
are required by Android resource packaging) and referenced from
`android/.../ui/theme/Theme.kt` as `MsFonts.sans` / `MsFonts.mono`.

Interface text uses Inter; every numeric readout (timecode, ruler labels, track
kind labels, gain, frame statistics) uses JetBrains Mono so digits stay aligned
and do not jitter while scrubbing.

No font is modified or renamed as a family: the OFL reservation on the names is
untouched, and the license files are kept next to this document.
