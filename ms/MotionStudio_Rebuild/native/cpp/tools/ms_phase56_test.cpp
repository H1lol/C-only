#include "ms_project.hpp"
#include "ms_timeline_player.hpp"
#include "ms_render_queue.hpp"
#include "ms_copilot.hpp"
#include "ms_render_backend.hpp"
#include <iostream>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Phase 5 — Offline backends / queue ===\n";

  ProjectSession session;
  session.new_project("P56", 160, 90, 24.f);
  {
    FILE* f = fopen("/tmp/ms_p56.bin", "wb");
    fwrite("ftypisomxxxx", 1, 12, f);
    fclose(f);
  }
  MediaId mid = session.import_media("/tmp/ms_p56.bin");
  ClipInfo c; c.media_id = mid; c.duration = 0.5; c.name = "A";
  session.add_clip(session.tracks()[0].id, c);

  TimelinePlayer player(&session);
  player.prepare();

  RealtimeCpuBackend cpu(&player);
  CHECK(cpu.available(), "cpu backend available");
  ImageBuffer frame;
  RenderJobDesc desc;
  desc.width = 160; desc.height = 90; desc.time_sec = 0.1f;
  CHECK(cpu.render(desc, frame, nullptr).ok(), "cpu render frame");
  CHECK(frame.valid(), "frame valid");

  RealtimeGpuBackend gpu;
  CHECK(gpu.available(), "gpu mock available");
  CHECK(gpu.render(desc, frame, nullptr).ok(), "gpu mock render");

  MoonRayBackend mr;
  // may be unavailable — that's OK
  std::cout << "  info: moonray available=" << (mr.available() ? "yes" : "no") << "\n";
  AppleseedBackend as;
  std::cout << "  info: appleseed available=" << (as.available() ? "yes" : "no") << "\n";

  RenderQueue q(&player);
  QueueJob job;
  job.name = "preview_frame";
  job.backend = "realtime_cpu";
  job.desc = desc;
  job.out_path = "/tmp/ms_p56_out.ppm";
  uint64_t jid = q.enqueue(job);
  CHECK(jid != 0, "enqueue");
  CHECK(q.process_next().ok(), "process_next");
  CHECK(q.jobs().back().state == JobState::Done, "job done");
  {
    FILE* f = fopen("/tmp/ms_p56_out.ppm", "rb");
    CHECK(f != nullptr, "output ppm written");
    if (f) fclose(f);
  }

  QueueJob j2;
  j2.backend = "moonray";
  j2.desc = desc;
  q.enqueue(j2);
  q.process_next(); // expect fail if no moonray
  CHECK(true, "moonray job handled without crash");

  std::cout << "\n=== Phase 6 — Copilot / terminal polish ===\n";

  CopilotContext ctx;
  ctx.workspace = "Edit";
  ctx.clip_count = session.clip_count();
  ctx.dirty = session.is_dirty();

  auto a1 = CopilotService::ask_local("how do I split a clip?", ctx);
  CHECK(a1.answer.find("split") != std::string::npos, "copilot split");
  auto a2 = CopilotService::ask("export render please", ctx);
  CHECK(!a2.answer.empty(), "copilot export");
  auto a3 = CopilotService::ask_local("what about blur?", ctx);
  CHECK(a3.answer.find("blur") != std::string::npos || a3.answer.find("Blur") != std::string::npos, "copilot blur");

  std::string uname = TerminalRunner::run("uname -s");
  CHECK(!uname.empty(), "terminal uname");
  std::cout << "  info: uname=" << uname;

  std::cout << (g_fail ? "\nPHASE 5+6 FAILED\n" : "\nPHASE 5+6 PASSED\n");
  return g_fail ? 1 : 0;
}
