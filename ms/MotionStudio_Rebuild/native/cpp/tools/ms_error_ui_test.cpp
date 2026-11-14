#include "ms_error_ui.hpp"
#include <iostream>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== Error screens ===\n";

  ErrorUI::instance().dismiss_all();
  ErrorUI::instance().clear_history();

  auto e1 = Error::make(ErrorCode::MediaOpenFailed, "cannot open", "/nope.mp4", "import");
  uint64_t id1 = report_error(e1, "Import Media");
  CHECK(id1 != 0, "show media error");
  CHECK(ErrorUI::instance().active().size() == 1, "1 active");

  ErrorScreen block;
  CHECK(ErrorUI::instance().try_blocking(block), "has blocking modal");
  CHECK(block.title.find("Cannot Open") != std::string::npos, "title");
  CHECK(!block.actions.empty(), "has actions");

  auto e2 = Error::make(ErrorCode::EffectNotFound, "unknown effect", "foo");
  ErrorUI::instance().show(e2, "Effects", ErrorScreenKind::Banner);
  CHECK(ErrorUI::instance().active().size() == 2, "2 active");

  ErrorUI::instance().show_message(ErrorSeverity::Info, "Saved", "Project saved",
                                   ErrorScreenKind::Toast);
  CHECK(ErrorUI::instance().active().size() == 3, "toast added");

  auto e3 = Error::make(ErrorCode::OutOfMemory, "frame buffer");
  ErrorUI::instance().show(e3, "Render", ErrorScreenKind::FullScreen);
  CHECK(ErrorUI::instance().try_blocking(block), "critical blocking");
  CHECK(block.kind == ErrorScreenKind::FullScreen || block.kind == ErrorScreenKind::Modal,
        "fullscreen or modal");

  std::cout << "--- active ---\n" << ErrorUI::instance().describe_active();
  CHECK(ErrorUI::instance().dismiss(id1), "dismiss");
  CHECK(ErrorUI::instance().history().size() >= 3, "history kept");

  ErrorUI::instance().dismiss_all();
  CHECK(ErrorUI::instance().active().empty(), "cleared active");

  std::cout << (g_fail ? "\nERROR UI FAILED\n" : "\nERROR UI PASSED\n");
  return g_fail ? 1 : 0;
}
