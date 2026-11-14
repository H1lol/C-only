# 7-pass blueprint — Passes 3–5 deepened

## Pass 3 — Compositing
- Layer stack + blend modes (existing, verified)
- **Masks**: rect/ellipse, feather, invert, luma/alpha modes
- **Transforms**: bilinear `blit_transformed` (pos/scale/rot/anchor)
- **Chromakey** simple keying

## Pass 4 — Animation & motion graphics
- **PropertySet** multi-curve animation
- transform_at from curves
- **Motion blur** multi-sample accumulate
- **Simple expressions** (`$prop`, + - * /)
- **Text placeholder** + **shape** rect/ellipse

## Pass 5 — Professional finishing
- **AudioMixer** tracks/clips/gain/pan (placeholder tone until FFmpeg)
- **GradeSettings** lift/gamma/gain/temperature + exposure path
- **export_graded_sequence**

## Still thin (honest)
- Tracking, true vector masks, real fonts, real audio decode
- Those stay for later passes / FFmpeg link
