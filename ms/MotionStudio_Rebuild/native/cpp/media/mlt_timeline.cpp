#include "ms_mlt_timeline.hpp"
#include "ms_edit_ops.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>

#if defined(MS_HAVE_MLT)
#include <framework/mlt.h>
#endif

namespace ms {

MltTimeline::MltTimeline() = default;

MltTimeline::~MltTimeline() { close(); }

bool MltTimeline::available() const {
#if defined(MS_HAVE_MLT)
  return true;
#else
  return false;
#endif
}

void MltTimeline::close() {
  destroy_graph();
  info_ = {};
}

#if defined(MS_HAVE_MLT)

Error MltTimeline::ensure_factory() {
  static bool init = false;
  if (!init) {
    if (!mlt_factory_init(nullptr))
      return Error::make(ErrorCode::NotInitialized, "mlt_factory_init");
    init = true;
  }
  return Error::success();
}

void MltTimeline::destroy_graph() {
  for (void* p : playlists_) {
    if (p) mlt_playlist_close(static_cast<mlt_playlist>(p));
  }
  playlists_.clear();
  if (tractor_) {
    mlt_tractor_close(static_cast<mlt_tractor>(tractor_));
    tractor_ = nullptr;
  }
  if (profile_) {
    mlt_profile_close(static_cast<mlt_profile>(profile_));
    profile_ = nullptr;
  }
}

Error MltTimeline::build(const ProjectSession& session) {
  Error e = ensure_factory();
  if (!e.ok()) return e;
  destroy_graph();

  width_ = session.meta().width > 0 ? session.meta().width : 1280;
  height_ = session.meta().height > 0 ? session.meta().height : 720;
  fps_ = session.meta().fps > 1 ? session.meta().fps : 24.0;

  mlt_profile profile = mlt_profile_init(nullptr);
  if (!profile) return Error::make(ErrorCode::NotInitialized, "profile");
  profile->width = width_;
  profile->height = height_;
  profile->frame_rate_num = static_cast<int>(fps_ * 1000);
  profile->frame_rate_den = 1000;
  profile->progressive = 1;
  profile_ = profile;

  mlt_tractor tractor = mlt_tractor_new();
  if (!tractor) {
    destroy_graph();
    return Error::make(ErrorCode::OutOfMemory, "tractor");
  }
  tractor_ = tractor;

  int clip_count = 0;
  double max_end = 0;

  const auto& tracks = session.tracks();
  // MLT multitrack: track 0 is often background; we map MS tracks in order
  for (size_t ti = 0; ti < tracks.size(); ++ti) {
    const auto& tr = tracks[ti];
    mlt_playlist pl = mlt_playlist_new(profile);
    if (!pl) continue;
    playlists_.push_back(pl);

    // Sort clips by start
    std::vector<ClipInfo> clips = tr.clips;
    std::sort(clips.begin(), clips.end(),
              [](const ClipInfo& a, const ClipInfo& b) { return a.start < b.start; });

    double cursor = 0.0;
    for (const auto& c : clips) {
      // blank gap
      if (c.start > cursor + 1e-4) {
        double gap = c.start - cursor;
        mlt_position gap_len = static_cast<mlt_position>(gap * fps_);
        if (gap_len > 0) mlt_playlist_blank(pl, gap_len);
      }
      std::string path;
      if (const MediaInfo* mi = session.find_media(c.media_id))
        path = mi->path;
      mlt_producer prod = nullptr;
      if (!path.empty())
        prod = mlt_factory_producer(profile, nullptr, path.c_str());
      if (!prod)
        prod = mlt_factory_producer(profile, "color", "black");
      if (!prod) continue;

      // Length in frames from clip duration
      double dur = c.duration > 0 ? c.duration : 1.0;
      mlt_position len = static_cast<mlt_position>(dur * fps_);
      if (len < 1) len = 1;
      mlt_properties_set_position(mlt_producer_properties(prod), "length", len);
      mlt_properties_set_position(mlt_producer_properties(prod), "out", len - 1);
      // in-point from source_in
      if (c.source_in > 0) {
        mlt_position in = static_cast<mlt_position>(c.source_in * fps_);
        mlt_properties_set_position(mlt_producer_properties(prod), "in", in);
      }
      mlt_playlist_append(pl, prod);
      mlt_producer_close(prod);
      cursor = c.start + dur;
      if (cursor > max_end) max_end = cursor;
      clip_count++;
    }

    // Attach playlist to tractor track
    mlt_tractor_set_track(tractor, mlt_playlist_producer(pl), static_cast<int>(ti));
  }

  // If no tracks, one black playlist
  if (playlists_.empty()) {
    mlt_playlist pl = mlt_playlist_new(profile);
    mlt_playlist_blank(pl, static_cast<mlt_position>(fps_ * 2));
    playlists_.push_back(pl);
    mlt_tractor_set_track(tractor, mlt_playlist_producer(pl), 0);
  }

  info_.built = true;
  info_.track_count = static_cast<int>(playlists_.size());
  info_.clip_count = clip_count;
  info_.duration_sec = max_end > 0 ? max_end : 2.0;
  info_.detail = "tractor built";
  return Error::success();
}

Error MltTimeline::render_frame(double time_sec, int out_w, int out_h) {
  if (!tractor_ || !profile_)
    return Error::make(ErrorCode::NotInitialized, "build timeline first");
  mlt_profile profile = static_cast<mlt_profile>(profile_);
  mlt_tractor tractor = static_cast<mlt_tractor>(tractor_);
  mlt_producer prod = mlt_tractor_producer(tractor);
  if (!prod) return Error::make(ErrorCode::NotInitialized, "no tractor producer");

  mlt_position pos = static_cast<mlt_position>(time_sec * fps_);
  mlt_producer_seek(prod, pos);
  mlt_service svc = mlt_producer_service(prod);
  mlt_frame frame = nullptr;
  if (mlt_service_get_frame(svc, &frame, 0) != 0 || !frame)
    return Error::make(ErrorCode::MediaDecodeFailed, "mlt get_frame");

  int w = out_w > 0 ? out_w : width_;
  int h = out_h > 0 ? out_h : height_;
  mlt_image_format fmt = mlt_image_rgba;
  uint8_t* image = nullptr;
  if (mlt_frame_get_image(frame, &image, &fmt, &w, &h, 0) != 0 || !image) {
    mlt_frame_close(frame);
    return Error::make(ErrorCode::MediaDecodeFailed, "mlt get_image");
  }
  auto e = FrameBufferHub::instance().publish_rgba(
      w, h, image, w * 4, static_cast<int64_t>(time_sec * 1000.0));
  mlt_frame_close(frame);
  return e;
}

Error MltTimeline::save_xml(const std::string& path) const {
  if (!tractor_ || !profile_)
    return Error::make(ErrorCode::NotInitialized, "no tractor");
  mlt_consumer consumer = mlt_factory_consumer(static_cast<mlt_profile>(profile_),
                                               "xml", path.c_str());
  if (!consumer)
    return Error::make(ErrorCode::ProjectSaveFailed, "xml consumer");
  mlt_consumer_connect(consumer, mlt_tractor_service(static_cast<mlt_tractor>(tractor_)));
  mlt_consumer_start(consumer);
  // xml consumer writes on start
  mlt_consumer_stop(consumer);
  mlt_consumer_close(consumer);
  // Verify file
  std::ifstream in(path);
  if (!in) return Error::make(ErrorCode::ProjectSaveFailed, "xml not written", path);
  return Error::success();
}

Error MltTimeline::load_xml(const std::string& path) {
  Error e = ensure_factory();
  if (!e.ok()) return e;
  destroy_graph();
  mlt_profile profile = mlt_profile_init(nullptr);
  profile_ = profile;
  mlt_producer prod = mlt_factory_producer(profile, "xml", path.c_str());
  if (!prod) {
    destroy_graph();
    return Error::make(ErrorCode::ProjectLoadFailed, "xml producer", path);
  }
  // Wrap producer as single-track tractor for render
  mlt_tractor tractor = mlt_tractor_new();
  tractor_ = tractor;
  mlt_playlist pl = mlt_playlist_new(profile);
  mlt_playlist_append(pl, prod);
  mlt_producer_close(prod);
  playlists_.push_back(pl);
  mlt_tractor_set_track(tractor, mlt_playlist_producer(pl), 0);
  info_.built = true;
  info_.track_count = 1;
  info_.detail = "loaded xml";
  info_.duration_sec = 5.0;
  return Error::success();
}

#else

Error MltTimeline::ensure_factory() {
  return Error::make(ErrorCode::NotSupported, "MLT not built");
}
void MltTimeline::destroy_graph() {}
Error MltTimeline::build(const ProjectSession&) {
  return Error::make(ErrorCode::NotSupported, "MLT not built");
}
Error MltTimeline::render_frame(double, int, int) {
  return Error::make(ErrorCode::NotSupported, "MLT not built");
}
Error MltTimeline::save_xml(const std::string&) const {
  return Error::make(ErrorCode::NotSupported, "MLT not built");
}
Error MltTimeline::load_xml(const std::string&) {
  return Error::make(ErrorCode::NotSupported, "MLT not built");
}

#endif

Error MltTimeline::insert_clip(ProjectSession& session, TrackId track, const ClipInfo& clip) {
  // Kdenlive-style insert: push later clips
  auto* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  ClipInfo c = clip;
  TimeSec end = c.start + c.duration;
  for (auto& other : tr->clips) {
    if (other.start >= c.start)
      other.start += c.duration;
  }
  session.add_clip(track, c);
  return build(session);
}

Error MltTimeline::overwrite_clip(ProjectSession& session, TrackId track, const ClipInfo& clip) {
  auto* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  TimeSec a0 = clip.start, a1 = clip.start + clip.duration;
  // Trim/remove overlaps
  std::vector<ClipInfo> kept;
  for (auto& other : tr->clips) {
    TimeSec b0 = other.start, b1 = other.start + other.duration;
    if (b1 <= a0 || b0 >= a1) {
      kept.push_back(other);
      continue;
    }
    if (b0 < a0) {
      ClipInfo left = other;
      left.duration = a0 - b0;
      kept.push_back(left);
    }
    if (b1 > a1) {
      ClipInfo right = other;
      right.start = a1;
      right.duration = b1 - a1;
      right.source_in += (a1 - b0);
      kept.push_back(right);
    }
  }
  tr->clips = kept;
  session.add_clip(track, clip);
  return build(session);
}

Error MltTimeline::remove_clip(ProjectSession& session, TrackId track, ClipId clip) {
  session.remove_clip(track, clip);
  return build(session);
}

Error MltTimeline::split_clip(ProjectSession& session, TrackId track, ClipId clip, TimeSec at) {
  auto* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  for (size_t i = 0; i < tr->clips.size(); ++i) {
    auto& c = tr->clips[i];
    if (c.id != clip) continue;
    if (at <= c.start || at >= c.start + c.duration)
      return Error::make(ErrorCode::InvalidArgument, "split outside clip");
    ClipInfo right = c;
    right.start = at;
    right.duration = (c.start + c.duration) - at;
    right.source_in = c.source_in + (at - c.start);
    right.id = 0; // new id on add
    c.duration = at - c.start;
    session.add_clip(track, right);
    return build(session);
  }
  return Error::make(ErrorCode::InvalidArgument, "clip not found");
}

Error MltTimeline::ripple_delete(ProjectSession& session, TrackId track, ClipId clip) {
  auto* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  TimeSec start = 0, dur = 0;
  bool found = false;
  for (const auto& c : tr->clips) {
    if (c.id == clip) { start = c.start; dur = c.duration; found = true; break; }
  }
  if (!found) return Error::make(ErrorCode::InvalidArgument, "clip");
  session.remove_clip(track, clip);
  for (auto& c : tr->clips) {
    if (c.start >= start)
      c.start -= dur;
  }
  return build(session);
}

Error mlt_timeline_roundtrip_test(ProjectSession& session, const std::string& xml_path) {
  MltTimeline tl;
  if (!tl.available())
    return Error::make(ErrorCode::NotSupported, "MLT");
  Error e = tl.build(session);
  if (!e.ok()) return e;
  e = tl.save_xml(xml_path);
  if (!e.ok()) return e;
  e = tl.render_frame(0.1);
  return e;
}

} // namespace ms
