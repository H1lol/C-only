#include "ui/ms_panels_edit.hpp"
#include "ms_project.hpp"
#include "ms_cpu_effects.hpp"
#include <sstream>
#include <iomanip>

namespace ms {
namespace ui {

// ─── Project ───────────────────────────────────────────────

void ProjectPanel::on_project_changed(ProjectSession* session) {
  session_ = session;
  refresh();
}

Error ProjectPanel::refresh() {
  entries_.clear();
  if (!session_) {
    project_name_ = "(no project)";
    entries_.push_back("(empty)");
    return Error::success();
  }
  // Name from JSON stub — use first track presence as signal
  project_name_ = "Project";
  entries_.push_back("Composition 1");
  for (const auto& m : session_->media()) {
    entries_.push_back(std::string("  ") + m.name);
  }
  for (const auto& t : session_->tracks()) {
    entries_.push_back(std::string("Track: ") + t.name);
  }
  if (entries_.size() == 1) entries_.push_back("  (import media)");
  return Error::success();
}

std::string ProjectPanel::describe() const {
  std::ostringstream os;
  os << project_name_ << " | " << entries_.size() << " items";
  return os.str();
}

// ─── Viewer ────────────────────────────────────────────────

void ViewerPanel::on_project_changed(ProjectSession* session) {
  session_ = session;
  if (session_) {
    // default vertical mobile frame; desktop can resize
    width_ = 1080;
    height_ = 1920;
  }
  refresh();
}

void ViewerPanel::on_time_changed(const UiTime& t) {
  time_ = t;
  refresh();
}

Error ViewerPanel::render_preview(LayerStack& layers, int w, int h) {
  width_ = w;
  height_ = h;
  Error e = composite_layers(layers, time_.seconds, w, h, frame_);
  status_ = e.ok() ? "ok" : e.message;
  return e;
}

Error ViewerPanel::refresh() {
  if (!session_) {
    status_ = "no session";
    return Error::success();
  }
  auto st = session_->state();
  time_.seconds = st.time;
  time_.playing = st.playing;
  status_ = time_.playing ? "playing" : "paused";
  return Error::success();
}

std::string ViewerPanel::describe() const {
  std::ostringstream os;
  os << width_ << "x" << height_ << " t=" << std::fixed << std::setprecision(2)
     << time_.seconds << "s [" << status_ << "]";
  if (frame_.valid()) os << " frame=" << frame_.size_bytes() << "B";
  return os.str();
}

// ─── Timeline ──────────────────────────────────────────────

void TimelinePanel::on_project_changed(ProjectSession* session) {
  session_ = session;
  refresh();
}

void TimelinePanel::on_time_changed(const UiTime& t) {
  time_ = t;
}

void TimelinePanel::on_selection_changed(const Selection& sel) {
  selection_ = sel;
  refresh();
}

Error TimelinePanel::refresh() {
  rows_.clear();
  if (!session_) return Error::success();
  for (const auto& tr : session_->tracks()) {
    if (tr.clips.empty()) {
      Row r;
      r.name = tr.name;
      r.start = 0;
      r.end = 0;
      rows_.push_back(r);
      continue;
    }
    for (const auto& c : tr.clips) {
      Row r;
      r.name = c.name.empty() ? tr.name : c.name;
      r.start = c.start;
      r.end = c.start + c.duration;
      r.selected = (selection_.kind == Selection::Kind::Clip && selection_.id == c.id);
      rows_.push_back(r);
    }
  }
  return Error::success();
}

std::string TimelinePanel::describe() const {
  std::ostringstream os;
  os << rows_.size() << " rows @ " << std::fixed << std::setprecision(2)
     << time_.seconds << "s";
  return os.str();
}

// ─── Inspector ─────────────────────────────────────────────

void InspectorPanel::on_project_changed(ProjectSession* session) {
  session_ = session;
  refresh();
}

void InspectorPanel::on_selection_changed(const Selection& sel) {
  selection_ = sel;
  refresh();
}

Error InspectorPanel::refresh() {
  fields_.clear();
  if (selection_.kind == Selection::Kind::None) {
    fields_.push_back({"Selection", "(none)"});
    return Error::success();
  }
  fields_.push_back({"Name", selection_.name.empty() ? "-" : selection_.name});
  fields_.push_back({"Id", std::to_string(selection_.id)});
  switch (selection_.kind) {
    case Selection::Kind::Layer:
      fields_.push_back({"Type", "Layer"});
      fields_.push_back({"Opacity", "100%"});
      fields_.push_back({"Blend", "Normal"});
      break;
    case Selection::Kind::Clip:
      fields_.push_back({"Type", "Clip"});
      break;
    case Selection::Kind::Effect:
      fields_.push_back({"Type", "Effect"});
      break;
    case Selection::Kind::Asset:
      fields_.push_back({"Type", "Asset"});
      break;
    default: break;
  }
  return Error::success();
}

std::string InspectorPanel::describe() const {
  if (selection_.kind == Selection::Kind::None) return "nothing selected";
  return selection_.name.empty() ? ("id=" + std::to_string(selection_.id)) : selection_.name;
}

} // namespace ui
} // namespace ms
