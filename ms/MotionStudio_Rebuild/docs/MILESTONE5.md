# Milestone 5 — Auto-edit & captions (PASSED)

```
M5 PASSED
scene cuts · silence · SRT/ASS · JSON templates · burn-in
```

## API
```cpp
detect_scenes(path, cuts);
detect_silence(path, marks);
apply_scene_cuts(session, media, track, cuts);
load_srt / save_srt / save_ass
export_with_srt(video, srt, out);
project_from_json_template / project_to_json_template
```

## Roadmap complete
| M | Gate | Status |
|---|------|--------|
| 0 | Frame → buffer (MLT) | PASSED |
| 1 | libav decode + playback | PASSED |
| 2 | MLT timeline + XML | PASSED |
| 3 | Effects host + frei0r | PASSED |
| 4 | Export + lossless | PASSED |
| 5 | Auto-edit + captions | PASSED |
