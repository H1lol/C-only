#pragma once
/** Timeline markers + edit operations (Kdenlive / OVE / LibreCuts ideas) */
#include "ms_project.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>

namespace ms {

// TimelineMarker lives in ms_project.hpp so markers can be serialized with the
// session; this header only adds the edit-operation helpers that use it.
class MarkerTrack {
public:
  uint64_t add(TimeSec t, const std::string& name = "Marker");
  bool remove(uint64_t id);
  const std::vector<TimelineMarker>& all() const { return markers_; }
  std::vector<TimelineMarker> in_range(TimeSec a, TimeSec b) const;
private:
  std::vector<TimelineMarker> markers_;
  uint64_t next_ = 1;
};

/** Ripple delete: remove clip and pull following clips left on same track */
Error ripple_delete_clip(ProjectSession& session, TrackId track, ClipId clip);

/** Roll: trim boundary between two adjacent clips (shared cut moves) */
Error roll_edit(ProjectSession& session, TrackId track, ClipId left, ClipId right, TimeSec delta);

/** Slip: change source_in/out keeping timeline span */
Error slip_clip(ProjectSession& session, TrackId track, ClipId clip, TimeSec delta_source);

} // namespace ms
