# Milestone 2 — MLT timeline engine (PASSED)

## Gate G2
Edit tests on MLT tractor/playlist graph; MLT XML save/load; frame render; session JSON save/reopen.

```
M2 PASSED
insert / overwrite / split / ripple delete
MLT XML 5482 bytes · render into frame buffer
```

## API
```cpp
MltTimeline tl;
tl.build(session);
tl.insert_clip(session, track, clip);
tl.overwrite_clip(...);
tl.split_clip(...);
tl.ripple_delete(...);
tl.render_frame(0.2);
tl.save_xml("project.mlt");
tl.load_xml("project.mlt");
```

## Design
- ProjectSession remains edit authority
- MLT tractor rebuilds from tracks/clips for render + XML
- Kdenlive-style insert (ripple push) and overwrite (trim overlaps)

## Next: Milestone 3
frei0r + OpenFX hosting, keyframes, port built-in effects
