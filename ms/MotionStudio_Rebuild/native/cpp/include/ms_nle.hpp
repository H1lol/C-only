#pragma once
/**
 * Deeper NLE operations — insert/overwrite, blade, close gap, snap, sequence stats
 */
#include "ms_project.hpp"
#include "ms_edit_ops.hpp"
#include "ms_transitions.hpp"
#include "ms_error.hpp"
#include <vector>
#include <optional>

namespace ms {

struct SequenceStats {
  TimeSec duration = 0;
  int clip_count = 0;
  int track_count = 0;
  int video_tracks = 0;
  int audio_tracks = 0;
  int marker_count = 0;
};

SequenceStats compute_stats(const ProjectSession& session, const MarkerTrack* markers = nullptr);

/** Sort clips on track by start time */
void sort_track_clips(TrackInfo& track);

/** Find clip under playhead on track */
ClipInfo* clip_at(TrackInfo& track, TimeSec t);
const ClipInfo* clip_at(const TrackInfo& track, TimeSec t);

/** Blade/split all tracks at time (where a clip covers t) */
int blade_all(ProjectSession& session, TimeSec t);

/** Close gaps on a track (pack clips left) */
Error close_gaps(ProjectSession& session, TrackId track);

/** Insert empty gap at time on track (push following clips right) */
Error insert_gap(ProjectSession& session, TrackId track, TimeSec at, TimeSec duration);

/**
 * Overwrite: place media as clip at `at`, replacing overlapping region on track.
 * Returns new clip id.
 */
ClipId overwrite_edit(ProjectSession& session, TrackId track, MediaId media,
                      TimeSec at, TimeSec duration, TimeSec source_in = 0);

/**
 * Insert edit: place clip at `at` and push following clips right by duration.
 */
ClipId insert_edit(ProjectSession& session, TrackId track, MediaId media,
                   TimeSec at, TimeSec duration, TimeSec source_in = 0);

/** Snap time to nearest clip edge or marker within tolerance */
TimeSec snap_time(TimeSec t, const ProjectSession& session,
                  const MarkerTrack* markers = nullptr, TimeSec tolerance = 0.15);

/** Build transition between last two adjacent clips if they touch */
Error add_transition_between(ProjectSession& session, TrackId track,
                             TransitionList& list, TransitionType type, TimeSec duration);

} // namespace ms
