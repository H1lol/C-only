#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$ROOT/native/cpp" -B "$ROOT/native/cpp/build_23_30" >/dev/null
cmake --build "$ROOT/native/cpp/build_23_30" -j2 >/dev/null
(cd "$ROOT" && "$ROOT/native/cpp/build_23_30/ms_23_30_test")
