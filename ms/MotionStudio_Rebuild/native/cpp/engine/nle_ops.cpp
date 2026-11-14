#include "ms_nle.hpp"
#include <algorithm>
#include <cmath>

namespace ms {

SequenceStats compute_stats(const ProjectSession& session, const MarkerTrack* markers) {
  SequenceStats st;
  st.track_count = static_cast<int>(session.tracks().size());
  st.duration = session.sequence_end();
  for (const auto& tr : session.tracks()) {
    if (tr.type == static_cast<int>(TrackType::Audio)) st.audio_tracks++;
    else st.video_tracks++;
    st.clip_count += static_cast<int>(tr.clips.size());
  }
  if (markers) st.marker_count = static_cast<int>(markers->all().size());
  return st;
}

void sort_track_clips(TrackInfo& track) {
  std::sort(track.clips.begin(), track.clips.end(),
            [](const ClipInfo& a, const ClipInfo& b) { return a.start < b.start; });
}

ClipInfo* clip_at(TrackInfo& track, TimeSec t) {
  for (auto& c : track.clips)
    if (t >= c.start && t < c.start + c.duration) return &c;
  return nullptr;
}

const ClipInfo* clip_at(const TrackInfo& track, TimeSec t) {
  for (const auto& c : track.clips)
    if (t >= c.start && t < c.start + c.duration) return &c;
  return nullptr;
}

int blade_all(ProjectSession& session, TimeSec t) {
  int n = 0;
  std::vector<TrackId> tids;
  for (const auto& tr : session.tracks()) tids.push_back(tr.id);
  for (TrackId tid : tids) {
    TrackInfo* tr = session.find_track(tid);
    if (!tr) continue;
    std::vector<ClipId> ids;
    for (const auto& c : tr->clips) {
      if (t > c.start + 1e-6 && t < c.start + c.duration - 1e-6)
        ids.push_back(c.id);
    }
    for (ClipId id : ids) {
      if (session.split_clip(tid, id, t)) ++n;
    }
  }
  return n;
}

Error close_gaps(ProjectSession& session, TrackId track) {
  TrackInfo* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  sort_track_clips(*tr);
  TimeSec cursor = 0;
  for (auto& c : tr->clips) {
    c.start = cursor;
    cursor += c.duration;
  }
  session.mark_dirty();
  return Error::success();
}

Error insert_gap(ProjectSession& session, TrackId track, TimeSec at, TimeSec duration) {
  if (duration <= 0) return Error::make(ErrorCode::InvalidArgument, "gap duration");
  TrackInfo* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  for (auto& c : tr->clips) {
    if (c.start >= at - 1e-9) c.start += duration;
  }
  session.mark_dirty();
  return Error::success();
}

ClipId overwrite_edit(ProjectSession& session, TrackId track, MediaId media,
                      TimeSec at, TimeSec duration, TimeSec source_in) {
  TrackInfo* tr = session.find_track(track);
  if (!tr || duration <= 0) return 0;
  TimeSec end = at + duration;
  // Trim/remove overlapping clips
  std::vector<ClipInfo> kept;
  for (auto& c : tr->clips) {
    TimeSec c0 = c.start, c1 = c.start + c.duration;
    if (c1 <= at + 1e-9 || c0 >= end - 1e-9) {
      kept.push_back(c);
      continue;
    }
    // left remnant
    if (c0 < at - 1e-9) {
      ClipInfo left = c;
      left.duration = at - c0;
      left.source_out = left.source_in + left.duration;
      kept.push_back(left);
    }
    // right remnant
    if (c1 > end + 1e-9) {
      ClipInfo right = c;
      TimeSec trim = end - c0;
      right.start = end;
      right.duration = c1 - end;
      right.source_in = c.source_in + trim;
      right.source_out = right.source_in + right.duration;
      right.id = 0; // new id on add
      kept.push_back(right);
    }
  }
  tr->clips = kept;
  ClipInfo neu;
  neu.media_id = media;
  neu.start = at;
  neu.duration = duration;
  neu.source_in = source_in;
  neu.source_out = source_in + duration;
  neu.name = "Overwrite";
  ClipId id = session.add_clip(track, neu);
  sort_track_clips(*tr);
  return id;
}

ClipId insert_edit(ProjectSession& session, TrackId track, MediaId media,
                   TimeSec at, TimeSec duration, TimeSec source_in) {
  if (duration <= 0) return 0;
  insert_gap(session, track, at, duration);
  ClipInfo neu;
  neu.media_id = media;
  neu.start = at;
  neu.duration = duration;
  neu.source_in = source_in;
  neu.source_out = source_in + duration;
  neu.name = "Insert";
  return session.add_clip(track, neu);
}

TimeSec snap_time(TimeSec t, const ProjectSession& session,
                  const MarkerTrack* markers, TimeSec tolerance) {
  TimeSec best = t;
  TimeSec best_d = tolerance + 1;
  auto consider = [&](TimeSec edge) {
    TimeSec d = std::abs(edge - t);
    if (d <= tolerance && d < best_d) { best_d = d; best = edge; }
  };
  consider(0);
  for (const auto& tr : session.tracks()) {
    for (const auto& c : tr.clips) {
      consider(c.start);
      consider(c.start + c.duration);
    }
  }
  if (markers) {
    for (const auto& m : markers->all()) consider(m.time);
  }
  return best;
}

Error add_transition_between(ProjectSession& session, TrackId track,
                             TransitionList& list, TransitionType type, TimeSec duration) {
  TrackInfo* tr = session.find_track(track);
  if (!tr || tr->clips.size() < 2)
    return Error::make(ErrorCode::InvalidArgument, "need 2 clips");
  sort_track_clips(*tr);
  // find first touching pair
  for (size_t i = 0; i + 1 < tr->clips.size(); ++i) {
    auto& L = tr->clips[i];
    auto& R = tr->clips[i + 1];
    TimeSec gap = R.start - (L.start + L.duration);
    if (std::abs(gap) < 0.05) {
      // ensure overlap region for transition: extend slightly
      TimeSec half = duration * 0.5;
      if (L.duration > half + 0.05 && R.duration > half + 0.05) {
        ClipTransition ct;
        ct.outgoing = L.id;
        ct.incoming = R.id;
        ct.type = type;
        ct.duration = duration;
        list.add(ct);
        return Error::success();
      }
    }
  }
  return Error::make(ErrorCode::InvalidArgument, "no adjacent pair");
}

} // namespace ms
