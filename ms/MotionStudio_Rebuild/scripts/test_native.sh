#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${MS_BUILD_DIR:-${ROOT}/build/native}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "WARN: ffmpeg not found - audio tests need it (ms_audio_playback_test fails, ms_engine_audio_test skips)" >&2
fi
cmake -S "${ROOT}/native/cpp" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" --parallel "${MS_BUILD_JOBS:-2}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
