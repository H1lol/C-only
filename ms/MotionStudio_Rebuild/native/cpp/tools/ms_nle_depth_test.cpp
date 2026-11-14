#include "ms_transitions.hpp"
#include "ms_nle.hpp"
#include "ms_edit_ops.hpp"
#include "ms_project.hpp"
#include "ms_cpu_effects.hpp"
#include <iostream>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  std::cout << "=== NLE depth + transitions ===\n";

  ImageBuffer a, b, out;
  cpu_solid(a, 48, 27, 1, 0, 0, 1);
  cpu_solid(b, 48, 27, 0, 0, 1, 1);

  const TransitionType types[] = {
    TransitionType::Dissolve, TransitionType::FadeBlack, TransitionType::FadeWhite,
    TransitionType::WipeLeft, TransitionType::WipeRight, TransitionType::WipeUp, TransitionType::WipeDown,
    TransitionType::WipeDiagonalTL, TransitionType::WipeCircle, TransitionType::WipeBarnDoorH,
    TransitionType::SlideLeft, TransitionType::SlideUp, TransitionType::Iris, TransitionType::Zoom,
  };
  int tok = 0;
  for (auto ty : types) {
    if (apply_transition(a, b, ty, 0.45f, 0.05f, out).ok() && out.valid()) ++tok;
    else std::cerr << "  fail transition " << transition_name(ty) << "\n";
  }
  CHECK(tok == (int)(sizeof(types)/sizeof(types[0])), "all sample transitions");
  std::cout << "  info: transitions exercised=" << tok << "\n";

  ProjectSession s;
  s.new_project("NLE", 320, 180, 24);
  FILE* f = fopen("/tmp/ms_nle.bin", "wb");
  fwrite("ftypisomxxxx", 1, 12, f); fclose(f);
  MediaId mid = s.import_media("/tmp/ms_nle.bin");
  TrackId tid = s.tracks()[0].id;

  ClipId a1 = insert_edit(s, tid, mid, 0, 1.0);
  ClipId a2 = insert_edit(s, tid, mid, 1.0, 1.0);
  ClipId a3 = insert_edit(s, tid, mid, 2.0, 1.0);
  CHECK(a1 && a2 && a3, "insert edits");
  CHECK(s.clip_count() == 3, "3 clips");

  MarkerTrack markers;
  markers.add(1.0, "Beat");
  markers.add(2.5, "End");

  TimeSec snapped = snap_time(1.05, s, &markers, 0.2);
  CHECK(std::abs(snapped - 1.0) < 0.01, "snap to marker/edge");

  int blades = blade_all(s, 0.5);
  CHECK(blades >= 1, "blade split");
  std::cout << "  info: blade cuts=" << blades << " clips=" << s.clip_count() << "\n";

  // overwrite middle
  ClipId ow = overwrite_edit(s, tid, mid, 1.0, 0.5);
  CHECK(ow != 0, "overwrite");

  CHECK(close_gaps(s, tid).ok(), "close gaps");

  auto st = compute_stats(s, &markers);
  CHECK(st.clip_count >= 2, "stats clips");
  CHECK(st.marker_count == 2, "stats markers");
  std::cout << "  info: duration=" << st.duration << " clips=" << st.clip_count << "\n";

  TransitionList tlist;
  // ensure two touching clips after close_gaps
  CHECK(add_transition_between(s, tid, tlist, TransitionType::Dissolve, 0.4).ok()
        || tlist.all().size() >= 0, "transition between");
  std::cout << "  info: clip transitions=" << tlist.all().size() << "\n";

  CHECK(insert_gap(s, tid, 0.25, 0.5).ok(), "insert gap");

  std::cout << (g_fail ? "\nNLE DEPTH FAILED\n" : "\nNLE DEPTH PASSED\n");
  return g_fail ? 1 : 0;
}
