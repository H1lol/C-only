#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
echo "MotionStudio release check — $ROOT"
test -f "$ROOT/LICENSE"
test -f "$ROOT/README.md"
test -f "$ROOT/CHANGELOG.md"
test -d "$ROOT/native/cpp/include"
test -f "$ROOT/assets/branding/ic_motion_studio_logo.png" || echo "WARN: logo missing"
echo "OK: package layout"
which ffmpeg >/dev/null && echo "OK: ffmpeg" || echo "WARN: no ffmpeg"
echo "Done."
