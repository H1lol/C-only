# Milestone 4 — Export (PASSED)

## Gate G4
Exported file exists; probe shows duration matching segment.

```
M4 PASSED
re-encode · lossless cut/join · session export · queue
```

## Presets
yt_1080p, yt_720p, mobile_720p, proxy_540p, hevc_1080p, lossless_copy, master_prores

## API
```cpp
lossless_cut(in, out, t0, t1);
lossless_join({a,b}, out);
export_media_file(in, out, *find_preset("mobile_720p"), 0, 1.0);
export_session(session, out, preset);
ExportQueue q; q.enqueue(job); q.run_next(&session);
```

## Android
Swap encode path to MediaCodec / Media3 Transformer; keep preset table.

## Next: Milestone 5 — Auto-edit & captions
