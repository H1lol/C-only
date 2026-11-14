# FFmpeg media I/O

## What works
| API | Backend |
|-----|---------|
| `ffmpeg_probe` | `ffprobe` JSON |
| `ffmpeg_decode_frame` | `ffmpeg` raw RGBA pipe |
| `FFmpegDecoder` | `IMediaDecoder` implementation |
| `create_media_decoder` | FFmpeg if available, else Slate |
| `ffmpeg_extract_sequence` | PPM sequence |
| `ffmpeg_encode_sequence` | H.264 MP4 via libx264 |

## Design
Uses **FFmpeg CLI** (no `libav` linked). Works on any machine with `ffmpeg`/`ffprobe` on PATH.

Optional future: `-DMS_HAVE_LIBAV` for in-process decode.

## Timeline
`TimelinePlayer` uses `create_media_decoder()` so real media plays when FFmpeg exists.

## Test
`ms_ffmpeg_test` generates `testsrc`, probes, decodes, extracts, encodes → **FFMPEG I/O PASSED**
