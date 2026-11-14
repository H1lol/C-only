#pragma once
/**
 * Milestone 0 — MLT engine spike
 * Desktop: link libmlt. Android: same API after NDK build.
 */
#include "ms_error.hpp"
#include "ms_frame_buffer.hpp"
#include <string>

namespace ms {

struct MltSpikeResult {
  bool ok = false;
  int width = 0;
  int height = 0;
  int64_t pts_ms = 0;
  std::string backend; // "mlt" | "ffmpeg_cli_fallback" | "solid"
  std::string detail;
};

/** True if libmlt was linked at build time */
bool mlt_available();

/**
 * Render one frame at time_sec from a producer (file path or mlt colour:).
 * Publishes RGBA into FrameBufferHub via ms_frame_buffer_publish_rgba.
 */
MltSpikeResult mlt_render_one_frame(const std::string& source, double time_sec = 0.0,
                                    int out_w = 320, int out_h = 180);

/** Generate a solid-color frame and publish (always works) */
MltSpikeResult mlt_publish_solid(int w, int h, uint8_t r, uint8_t g, uint8_t b);

} // namespace ms
