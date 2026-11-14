#include "ms_studio.hpp"
#include <algorithm>
#include <sstream>

namespace ms {

StudioHub::StudioHub()
    : player_(&project_), queue_(&player_) {
  scene_ = make_default_studio_scene(1920, 1080);
}

Error StudioHub::new_project(const std::string& name, int w, int h, float fps) {
  Error e = project_.new_project(name, w, h, fps);
  if (!e.ok()) return e;
  player_.set_session(&project_);
  queue_.set_player(&player_);
  playhead_ = 0;
  layers_ = LayerStack{};
  // Default solid backdrop layer
  Layer bg;
  bg.name = "Backdrop";
  bg.type = LayerType::Solid;
  bg.solid_r = 0.08f; bg.solid_g = 0.08f; bg.solid_b = 0.10f;
  bg.start = 0; bg.end = 3600;
  layers_.add_layer(bg);
  scene_ = make_default_studio_scene(w, h);
  return Error::success();
}

Error StudioHub::import_and_place(const std::string& path, TimeSec at, TimeSec duration) {
  MediaId mid = project_.import_media(path);
  if (mid == 0) return Error::make(ErrorCode::MediaOpenFailed, "import", path);
  if (project_.tracks().empty())
    return Error::make(ErrorCode::InvalidArgument, "no tracks");
  ClipInfo c;
  c.media_id = mid;
  c.name = path.substr(path.find_last_of("/\\") == std::string::npos
                           ? 0
                           : path.find_last_of("/\\") + 1);
  c.start = at;
  c.duration = duration;
  c.source_in = 0;
  c.source_out = duration;
  ClipId cid = project_.add_clip(project_.tracks()[0].id, c);
  if (cid == 0) return Error::make(ErrorCode::InvalidArgument, "add_clip failed");
  player_.prepare();
  return Error::success();
}

Error StudioHub::preview(ImageBuffer& out) {
  Error e = player_.prepare();
  if (!e.ok()) return e;
  e = player_.render_frame(playhead_, out);
  if (!e.ok()) return e;
  // Optional: composite layer stack on top at lower res skip for speed if huge
  if (!layers_.layers().empty() && out.valid()) {
    ImageBuffer layer_comp;
    Error ce = composite_layers(layers_, playhead_, out.width(), out.height(), layer_comp);
    if (ce.ok() && layer_comp.valid()) {
      // simple over
      const int n = out.width() * out.height();
      uint8_t* d = out.data();
      const uint8_t* s = layer_comp.data();
      for (int i = 0; i < n; ++i) {
        float sa = s[3] / 255.f;
        if (sa > 0.01f) {
          float ia = 1.f - sa;
          d[0] = static_cast<uint8_t>(s[0] * sa + d[0] * ia);
          d[1] = static_cast<uint8_t>(s[1] * sa + d[1] * ia);
          d[2] = static_cast<uint8_t>(s[2] * sa + d[2] * ia);
          d[3] = static_cast<uint8_t>(std::min(255.f, s[3] + d[3] * ia));
        }
        s += 4; d += 4;
      }
    }
  }
  return Error::success();
}

uint64_t StudioHub::enqueue_preview_render(const std::string& out_ppm) {
  QueueJob job;
  job.name = "preview";
  job.backend = "realtime_cpu";
  job.desc.width = project_.meta().width;
  job.desc.height = project_.meta().height;
  job.desc.time_sec = static_cast<float>(playhead_);
  job.out_path = out_ppm;
  return queue_.enqueue(job);
}

DccJobResult StudioHub::export_dcc(DccTarget target, const std::string& job_dir, bool dry_run) {
  scene_.render().width = project_.meta().width;
  scene_.render().height = project_.meta().height;
  return dcc_.submit(scene_, target, job_dir, dry_run);
}

CopilotSuggestion StudioHub::ask(const std::string& question) {
  CopilotContext ctx;
  ctx.workspace = "Studio";
  ctx.clip_count = project_.clip_count();
  ctx.dirty = project_.is_dirty();
  return CopilotService::ask(question, ctx);
}

Error StudioHub::scan_plugins(const std::string& root) {
  return plugins().scan_installed(root);
}

std::string StudioHub::status_line() const {
  std::ostringstream os;
  os << info_.name << " " << info_.version
     << " | " << project_.meta().name
     << " " << project_.meta().width << "x" << project_.meta().height
     << "@" << project_.meta().fps
     << " | t=" << playhead_
     << " | clips=" << project_.clip_count()
     << " | layers=" << layers_.layers().size()
     << (project_.is_dirty() ? " | dirty" : "");
  return os.str();
}

} // namespace ms
