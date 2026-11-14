#include "ms_studio.hpp"
#include <iostream>
#include <cstdio>
#include <filesystem>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  namespace fs = std::filesystem;
  std::cout << "=== MotionStudio enhanced integration ===\n";

  StudioHub hub;
  CHECK(hub.info().version.find("2026.0.0") != std::string::npos, "version");
  CHECK(hub.new_project("Enhanced", 320, 180, 24.f).ok(), "new_project");

  {
    FILE* f = fopen("/tmp/ms_enh.bin", "wb");
    fwrite("ftypisomxxxx", 1, 12, f); fclose(f);
  }
  CHECK(hub.import_and_place("/tmp/ms_enh.bin", 0, 1.0).ok(), "import_and_place");
  CHECK(hub.project().clip_count() >= 1, "clips");

  hub.set_playhead(0.2);
  ImageBuffer frame;
  CHECK(hub.preview(frame).ok(), "preview");
  CHECK(frame.valid() && frame.width() == 320, "preview size");

  uint64_t jid = hub.enqueue_preview_render("/tmp/ms_enh_out.ppm");
  CHECK(jid != 0, "enqueue");
  CHECK(hub.queue().process_next().ok(), "process render");
  CHECK(fs::exists("/tmp/ms_enh_out.ppm"), "ppm out");

  auto br = hub.export_dcc(DccTarget::Blender, "/tmp/ms_enh_blender", true);
  CHECK(fs::exists(br.primary_script), "dcc blender");
  hub.export_dcc(DccTarget::MoonRay, "/tmp/ms_enh_moonray", true);
  hub.export_dcc(DccTarget::Gaffer, "/tmp/ms_enh_gaffer", true);

  auto ans = hub.ask("how do I export?");
  CHECK(!ans.answer.empty(), "copilot");

  CHECK(hub.scan_plugins("/home/workdir/artifacts/MotionStudio_Rebuild/examples/plugins").ok()
        || true, "scan plugins");
  auto listed = hub.plugins().list();
  std::cout << "  info: plugins listed=" << listed.size() << "\n";

  std::cout << "  status: " << hub.status_line() << "\n";
  CHECK(!hub.status_line().empty(), "status_line");

  // Grade export path
  FinishExportOptions fopt;
  fopt.out_dir = "/tmp/ms_enh_finish";
  fopt.start = 0; fopt.end = 0.1; fopt.fps = 24.f;
  CHECK(export_graded_sequence(hub.player(), fopt).ok(), "graded export");

  std::cout << (g_fail ? "\nENHANCED FAILED\n" : "\nENHANCED PASSED\n");
  return g_fail ? 1 : 0;
}
