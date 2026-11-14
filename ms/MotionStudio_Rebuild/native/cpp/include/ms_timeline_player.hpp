#pragma once
/**
 * Phase 2 — Timeline player
 * Seek time → sample active clips → decode/slate → composite to frame
 */
#include "ms_project.hpp"
#include "ms_media.hpp"
#include "ms_image.hpp"
#include "ms_error.hpp"
#include <memory>
#include <unordered_map>
#include <string>

namespace ms {

class TimelinePlayer {
public:
  explicit TimelinePlayer(ProjectSession* session);

  void set_session(ProjectSession* session);
  ProjectSession* session() const { return session_; }

  /** Open / cache decoders for all media in the project */
  Error prepare();

  /** Render one output frame at timeline time (seconds) */
  Error render_frame(TimeSec t, ImageBuffer& out);

  /** Current playhead helpers */
  Error scrub(TimeSec t, ImageBuffer& out) { return render_frame(t, out); }

  int output_width() const;
  int output_height() const;
  float output_fps() const;

private:
  ProjectSession* session_ = nullptr;
  std::unordered_map<MediaId, std::unique_ptr<IMediaDecoder>> decoders_;

  IMediaDecoder* decoder_for(MediaId id);
  static void blit_scaled(const ImageBuffer& src, ImageBuffer& dst,
                          float opacity, int dst_x, int dst_y);

  /** Opacity + keyframed translate/scale blit into the output frame. */
  static void blit_transformed(const ImageBuffer& src, ImageBuffer& dst, float opacity,
                               float dx, float dy, float scale);
};

/** Phase 2 — simple sequential export (frame sequence / stub mp4 path) */
class SequenceExporter {
public:
  struct Options {
    std::string out_dir = "/tmp/ms_export";
    TimeSec start = 0;
    TimeSec end = 0; // 0 = sequence_end
    int width = 0;    // 0 = project
    int height = 0;
    float fps = 0;
  };

  Error export_png_sequence(TimelinePlayer& player, const Options& opt);
};

} // namespace ms
