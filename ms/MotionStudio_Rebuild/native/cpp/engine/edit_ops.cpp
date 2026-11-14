#include "ms_edit_ops.hpp"
#include <algorithm>

namespace ms {

uint64_t MarkerTrack::add(TimeSec t, const std::string& name) {
  TimelineMarker m;
  m.id = next_++;
  m.time = t;
  m.name = name;
  markers_.push_back(m);
  return m.id;
}

bool MarkerTrack::remove(uint64_t id) {
  auto it = std::remove_if(markers_.begin(), markers_.end(),
                           [id](const TimelineMarker& m) { return m.id == id; });
  if (it == markers_.end()) return false;
  markers_.erase(it, markers_.end());
  return true;
}

std::vector<TimelineMarker> MarkerTrack::in_range(TimeSec a, TimeSec b) const {
  std::vector<TimelineMarker> out;
  for (const auto& m : markers_)
    if (m.time >= a && m.time <= b) out.push_back(m);
  return out;
}

Error ripple_delete_clip(ProjectSession& session, TrackId track, ClipId clip) {
  TrackInfo* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  auto it = std::find_if(tr->clips.begin(), tr->clips.end(),
                         [clip](const ClipInfo& c) { return c.id == clip; });
  if (it == tr->clips.end()) return Error::make(ErrorCode::InvalidArgument, "clip");
  TimeSec start = it->start;
  TimeSec dur = it->duration;
  tr->clips.erase(it);
  for (auto& c : tr->clips) {
    if (c.start >= start - 1e-9)
      c.start = std::max(0.0, c.start - dur);
  }
  session.mark_dirty();
  return Error::success();
}

Error roll_edit(ProjectSession& session, TrackId track, ClipId left, ClipId right, TimeSec delta) {
  TrackInfo* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  ClipInfo *L = nullptr, *R = nullptr;
  for (auto& c : tr->clips) {
    if (c.id == left) L = &c;
    if (c.id == right) R = &c;
  }
  if (!L || !R) return Error::make(ErrorCode::InvalidArgument, "clips");
  // boundary at L->start + L->duration should meet R->start
  TimeSec new_dur = L->duration + delta;
  if (new_dur < 0.01) return Error::make(ErrorCode::InvalidArgument, "roll too far");
  TimeSec new_r_start = R->start + delta;
  TimeSec new_r_dur = R->duration - delta;
  if (new_r_dur < 0.01) return Error::make(ErrorCode::InvalidArgument, "roll too far");
  L->duration = new_dur;
  L->source_out = L->source_in + new_dur;
  R->start = new_r_start;
  R->duration = new_r_dur;
  R->source_in += delta;
  session.mark_dirty();
  return Error::success();
}

Error slip_clip(ProjectSession& session, TrackId track, ClipId clip, TimeSec delta_source) {
  TrackInfo* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  for (auto& c : tr->clips) {
    if (c.id != clip) continue;
    c.source_in += delta_source;
    c.source_out += delta_source;
    if (c.source_in < 0) {
      c.source_out -= c.source_in;
      c.source_in = 0;
    }
    session.mark_dirty();
    return Error::success();
  }
  return Error::make(ErrorCode::InvalidArgument, "clip");
}

} // namespace ms
