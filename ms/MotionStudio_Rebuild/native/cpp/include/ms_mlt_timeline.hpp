#pragma once
/**
 * Milestone 2 — MLT playlist/tractor timeline behind MS project model
 */
#include "ms_project.hpp"
#include "ms_error.hpp"
#include "ms_frame_buffer.hpp"
#include <string>
#include <vector>
#include <memory>

namespace ms {

struct MltTimelineInfo {
  bool built = false;
  int track_count = 0;
  int clip_count = 0;
  double duration_sec = 0;
  std::string detail;
};

/**
 * Owns an MLT tractor composed of playlists (one per MS track).
 * ProjectSession remains the edit authority; this rebuilds the graph for render/XML.
 */
class MltTimeline {
public:
  MltTimeline();
  ~MltTimeline();

  bool available() const;

  /** Build/rebuild tractor from session tracks & clips */
  Error build(const ProjectSession& session);

  /** Render one frame at time_sec into FrameBufferHub */
  Error render_frame(double time_sec, int out_w = 640, int out_h = 360);

  /** Save MLT XML (Shotcut/Kdenlive-compatible subset) */
  Error save_xml(const std::string& path) const;

  /** Load MLT XML into a bare tractor (does not fully rebuild ProjectSession) */
  Error load_xml(const std::string& path);

  /**
   * Apply Kdenlive-style edit helpers on the *session*, then rebuild MLT.
   * Returns false if session pointer missing.
   */
  Error insert_clip(ProjectSession& session, TrackId track, const ClipInfo& clip);
  Error overwrite_clip(ProjectSession& session, TrackId track, const ClipInfo& clip);
  Error remove_clip(ProjectSession& session, TrackId track, ClipId clip);
  Error split_clip(ProjectSession& session, TrackId track, ClipId clip, TimeSec at);
  Error ripple_delete(ProjectSession& session, TrackId track, ClipId clip);

  MltTimelineInfo info() const { return info_; }
  void close();

private:
  void* profile_ = nullptr;   // mlt_profile
  void* tractor_ = nullptr;   // mlt_tractor
  std::vector<void*> playlists_; // mlt_playlist*
  MltTimelineInfo info_;
  int width_ = 1280;
  int height_ = 720;
  double fps_ = 24.0;

  Error ensure_factory();
  void destroy_graph();
};

/** Round-trip: session → MLT XML → file exists + render */
Error mlt_timeline_roundtrip_test(ProjectSession& session, const std::string& xml_path);

} // namespace ms
