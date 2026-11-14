#pragma once
/**
 * Pass 5 — Professional finishing helpers
 * Grade + multi-frame export + simple LUT-style curve
 */
#include "ms_color_pipeline.hpp"
#include "ms_timeline_player.hpp"
#include "ms_render_queue.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>

namespace ms {

struct GradeSettings {
  float exposure = 0.f;
  float contrast = 1.f;
  float saturation = 1.f;
  float temperature = 0.f; // -1..1 warm/cool approx
  float lift = 0.f;        // shadows
  float gamma = 1.f;
  float gain = 1.f;
};

Error apply_finishing_grade(ImageBuffer& img, const GradeSettings& g);

struct FinishExportOptions {
  std::string out_dir = "/tmp/ms_finish";
  TimeSec start = 0;
  TimeSec end = 1;
  float fps = 24.f;
  GradeSettings grade;
  bool apply_grade = true;
};

/** Export graded frame sequence via TimelinePlayer */
Error export_graded_sequence(TimelinePlayer& player, const FinishExportOptions& opt);

} // namespace ms
