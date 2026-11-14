# Phase 5 & 6 — PASSED

## Phase 5 — Offline backends + render queue
- `RealtimeCpuBackend` — frames via TimelinePlayer
- `RealtimeGpuBackend` — mock/preview path
- `MoonRayBackend` / `AppleseedBackend` — optional CLI stubs (available() check)
- `RenderQueue` — enqueue / process_next / process_all / cancel
- Job writes optional PPM output

## Phase 6 — Polish (Copilot + terminal)
- `CopilotService::ask_local` — offline rules (split, blur, export, undo, save, nodes)
- `CopilotService::ask` — same for now (cloud later)
- `TerminalRunner::run` — capture shell output
- AI model (Ollama/Qwen) still **optional external** — not required for pass

## Test
```bash
g++ -std=c++17 -I native/cpp/include \
  native/cpp/engine/project_session.cpp \
  native/cpp/media/media_probe.cpp \
  native/cpp/media/timeline_player.cpp \
  native/cpp/render/image_buffer.cpp \
  native/cpp/render/render_backends.cpp \
  native/cpp/render/render_queue.cpp \
  native/cpp/tools/copilot_service.cpp \
  native/cpp/tools/ms_phase56_test.cpp \
  -o ms_phase56_test && ./ms_phase56_test
```

## Phase map (updated)
1 Project ✅  2 Media ✅  3 Motion ✅  4 Nodes/color ✅  5 Queue/backends ✅  6 Copilot/terminal ✅
