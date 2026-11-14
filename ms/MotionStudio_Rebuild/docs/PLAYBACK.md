# FFmpeg video playback

## MediaPlayer (single file)
- `open` / `close`
- `play` / `pause` / `stop` / `seek`
- `tick(ImageBuffer&)` — wall-clock advance + decode when playing
- `current_frame` — force decode at playhead
- looping, rate, dropped-frame counter

## TimelinePlayback (project)
- Same transport controls over `TimelinePlayer`
- In/out range optional
- Composites timeline clips while playing

## Usage
```cpp
MediaPlayer p;
p.open("clip.mp4");
p.play();
while (running) {
  ImageBuffer frame;
  if (p.tick(frame)) present(frame);
}
```

## Test
`ms_playback_test` → **PLAYBACK PASSED**
