#include "ui/ms_panels_all.hpp"
#include "ms_error_ui.hpp"
#include "ms_project.hpp"
#include "ms_cpu_effects.hpp"
#include <sstream>
#include <cstdio>
#include <array>
#include <memory>
#include <algorithm>
#include <cmath>

namespace ms {
namespace ui {

static const char* blend_name(BlendMode m) {
  switch (m) {
    case BlendMode::Normal: return "Normal";
    case BlendMode::Multiply: return "Multiply";
    case BlendMode::Screen: return "Screen";
    case BlendMode::Overlay: return "Overlay";
    case BlendMode::Darken: return "Darken";
    case BlendMode::Lighten: return "Lighten";
    default: return "Other";
  }
}

// ─── Layers ────────────────────────────────────────────────

void LayerPanel::on_project_changed(ProjectSession* s) { session_ = s; refresh(); }
void LayerPanel::on_time_changed(const UiTime& t) { time_ = t; }
void LayerPanel::on_selection_changed(const Selection& sel) { selection_ = sel; refresh(); }

Error LayerPanel::refresh() {
  rows_.clear();
  if (!stack_) return Error::success();
  for (const auto& L : stack_->layers()) {
    Row r;
    r.id = L.id;
    r.name = L.name;
    r.visible = L.visible;
    r.locked = L.locked;
    r.blend = blend_name(L.blend);
    r.selected = (selection_.kind == Selection::Kind::Layer && selection_.id == L.id);
    rows_.push_back(r);
  }
  return Error::success();
}

std::string LayerPanel::describe() const {
  return std::to_string(rows_.size()) + " layers";
}

// ─── Effects ───────────────────────────────────────────────

void EffectsPanel::on_project_changed(ProjectSession* s) { session_ = s; refresh(); }
void EffectsPanel::on_selection_changed(const Selection& sel) { selection_ = sel; }

Error EffectsPanel::refresh() {
  items_.clear();
  // Builtins always present
  items_.push_back({"org.motionstudio.gaussian_blur", "Gaussian Blur", "Blur & Sharpen", true, true});
  items_.push_back({"brightness_contrast", "Brightness / Contrast", "Color Correction", true, true});
  items_.push_back({"opacity", "Opacity", "Channel", true, true});
  // Installed .msp from registry
  for (const auto& m : MspRegistry::instance().list()) {
    items_.push_back({m.id, m.name, m.category.empty() ? "User" : m.category, m.open_source, true});
  }
  return Error::success();
}

Error EffectsPanel::apply_to_selection(const std::string& effect_id, LayerStack* stack) {
  if (!stack) return Error::make(ErrorCode::InvalidArgument, "no layer stack");
  if (selection_.kind != Selection::Kind::Layer) {
    return Error::make(ErrorCode::InvalidArgument, "select a layer first");
  }
  Layer* L = stack->find(selection_.id);
  if (!L) return Error::make(ErrorCode::InvalidArgument, "layer not found");
  EffectInstance fx;
  fx.base_id = effect_id;
  fx.enabled = true;
  if (effect_id.find("blur") != std::string::npos) {
    EffectParam p; p.name = "radius"; p.value = 4.f; fx.params.push_back(p);
  }
  L->effects.push_back(fx);
  return Error::success();
}

std::string EffectsPanel::describe() const {
  return std::to_string(items_.size()) + " effects";
}

// ─── Graph ─────────────────────────────────────────────────

void GraphPanel::on_project_changed(ProjectSession* s) { session_ = s; }
void GraphPanel::on_selection_changed(const Selection& sel) { selection_ = sel; }

uint64_t GraphPanel::add_node(const std::string& type, const std::string& name) {
  Node n;
  n.id = next_id_++;
  n.type = type;
  n.name = name.empty() ? type : name;
  n.enabled = true;
  nodes_.push_back(n);
  return n.id;
}

bool GraphPanel::connect(uint64_t from, uint64_t to) {
  edges_.push_back({from, to});
  return true;
}

bool GraphPanel::set_enabled(uint64_t id, bool on) {
  for (auto& n : nodes_) if (n.id == id) { n.enabled = on; return true; }
  return false;
}

Error GraphPanel::refresh() { return Error::success(); }

std::string GraphPanel::describe() const {
  return std::to_string(nodes_.size()) + " nodes, " + std::to_string(edges_.size()) + " edges";
}

// ─── Scopes / Grade ────────────────────────────────────────

Error ScopesPanel::analyze(const ImageBuffer& frame) {
  if (!frame.valid() || frame.format() != PixelFormat::RGBA8) {
    has_data_ = false;
    return Error::make(ErrorCode::InvalidArgument, "bad frame");
  }
  const uint8_t* p = frame.data();
  const int n = frame.width() * frame.height();
  double sum = 0;
  float mn = 1.f, mx = 0.f;
  for (int i = 0; i < n; ++i) {
    float y = (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.f;
    sum += y;
    mn = std::min(mn, y);
    mx = std::max(mx, y);
    p += 4;
  }
  luma_avg_ = n ? static_cast<float>(sum / n) : 0.f;
  luma_min_ = mn;
  luma_max_ = mx;
  has_data_ = true;
  return Error::success();
}

Error ScopesPanel::refresh() { return Error::success(); }

std::string ScopesPanel::describe() const {
  if (!has_data_) return "no frame";
  std::ostringstream os;
  os.precision(3);
  os << "Y avg=" << luma_avg_ << " min=" << luma_min_ << " max=" << luma_max_;
  return os.str();
}

std::string GradePanel::describe() const {
  std::ostringstream os;
  os << "exp=" << exposure << " con=" << contrast << " sat=" << saturation
     << " view=" << ocio_view;
  return os.str();
}

// ─── Mixer ─────────────────────────────────────────────────

void MixerPanel::on_project_changed(ProjectSession* s) { session_ = s; refresh(); }

Error MixerPanel::refresh() {
  strips_.clear();
  if (!session_) {
    strips_.push_back({"Master", 0.f, 0.f, 0.f, false, false});
    return Error::success();
  }
  for (const auto& t : session_->tracks()) {
    if (t.type == static_cast<int>(TrackType::Audio) || t.name.find("Audio") != std::string::npos) {
      strips_.push_back({t.name, 0.f, 0.f, 0.f, t.muted, false});
    }
  }
  strips_.push_back({"Master", 0.f, 0.f, 0.f, false, false});
  if (strips_.size() == 1) {
    // placeholder lanes so mixer still has meaning
    strips_.insert(strips_.begin(), {"Audio 1", -6.f, 0.2f, 0.f, false, false});
    strips_.insert(strips_.begin() + 1, {"Audio 2", -3.f, 0.4f, -0.2f, false, false});
  }
  return Error::success();
}

std::string MixerPanel::describe() const {
  return std::to_string(strips_.size()) + " strips";
}

// ─── Render queue ──────────────────────────────────────────

void RenderQueuePanel::on_project_changed(ProjectSession* s) { session_ = s; }

uint64_t RenderQueuePanel::enqueue(const std::string& name, const std::string& backend) {
  Job j;
  j.id = next_id_++;
  j.name = name;
  j.backend = backend;
  j.progress = 0.f;
  j.status = "queued";
  jobs_.push_back(j);
  return j.id;
}

bool RenderQueuePanel::cancel(uint64_t id) {
  for (auto& j : jobs_) {
    if (j.id == id) {
      j.status = "cancelled";
      return true;
    }
  }
  return false;
}

std::vector<std::string> RenderQueuePanel::available_backends() const {
  std::vector<std::string> v = {"realtime_gpu"};
  MoonRayBackend mr;
  if (mr.available()) v.push_back("moonray");
  AppleseedBackend as;
  if (as.available()) v.push_back("appleseed");
  else v.push_back("appleseed"); // listed as optional target even if not installed
  return v;
}

Error RenderQueuePanel::refresh() { return Error::success(); }

std::string RenderQueuePanel::describe() const {
  return std::to_string(jobs_.size()) + " jobs";
}

// ─── Terminal ──────────────────────────────────────────────

void TerminalPanel::on_project_changed(ProjectSession* s) { session_ = s; }

Error TerminalPanel::run(const std::string& command) {
  history_.push_back("$ " + command);
  std::string cmd = "cd \"" + cwd_ + "\" && " + command + " 2>&1";
  std::array<char, 256> buf{};
  std::string result;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    last_output_ = "popen failed";
    history_.push_back(last_output_);
    return Error::make(ErrorCode::Unknown, "terminal popen failed");
  }
  while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
    result += buf.data();
  }
  int rc = pclose(pipe);
  last_output_ = result;
  if (last_output_.size() > 4000) last_output_.resize(4000);
  history_.push_back(last_output_.empty() ? "(exit " + std::to_string(rc) + ")" : last_output_);
  if (history_.size() > 50) history_.erase(history_.begin(), history_.begin() + static_cast<long>(history_.size() - 50));
  return Error::success();
}

std::string TerminalPanel::describe() const {
  return "cwd=" + cwd_ + " hist=" + std::to_string(history_.size());
}

// ─── Copilot ───────────────────────────────────────────────

void CopilotPanel::ask(const std::string& prompt) {
  messages_.push_back({true, prompt});
  // Local heuristic assistant (no network required) — real LLM can replace later
  std::string reply;
  std::string p = prompt;
  for (auto& c : p) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
  if (p.find("blur") != std::string::npos) {
    reply = "Apply org.motionstudio.gaussian_blur on the selected layer. "
            "In Motion workspace: effects apply <id>. Radius default 4.";
    last_suggestion_ = "effects apply org.motionstudio.gaussian_blur";
  } else if (p.find("error") != std::string::npos || p.find("fix") != std::string::npos) {
    reply = "Check Problems panel, then run the suggested command in Terminal. "
            "Common: missing select before effects apply; MoonRay not installed.";
    last_suggestion_ = "problems";
  } else if (p.find("render") != std::string::npos) {
    reply = "Switch to Render workspace, enqueue with backend realtime_gpu "
            "(or moonray/appleseed when installed).";
    last_suggestion_ = "render enqueue comp1 realtime_gpu";
  } else if (p.find("msp") != std::string::npos || p.find("plugin") != std::string::npos) {
    reply = "Pack with: python native/python/tools/msp_pack.py <folder> -o x.msp";
    last_suggestion_ = "python native/python/tools/msp_pack.py examples/plugins/sample_blur -o blur.msp";
  } else {
    reply = "Workspaces: Edit Motion Nodes Color Audio Render Dev. "
            "Type help in the shell for commands. Ask about blur, render, msp, or errors.";
    last_suggestion_.clear();
  }
  messages_.push_back({false, reply});
}

void CopilotPanel::ingest_error(const std::string& error_text) {
  messages_.push_back({false, std::string("Saw error: ") + error_text});
  last_suggestion_ = "Check Problems + Terminal";
}

std::string CopilotPanel::describe() const {
  return std::to_string(messages_.size()) + " messages";
}

// ─── Problems ──────────────────────────────────────────────

void ProblemsPanel::push(const Error& e, const std::string& context) {
  if (e.ok()) return;
  items_.push_back({e.code, e.message, context.empty() ? e.context : context});
  if (items_.size() > 100) items_.erase(items_.begin());
  ErrorUI::instance().show(e, context.empty() ? "Problems" : context);
}

void ProblemsPanel::clear() { items_.clear(); }

std::string ProblemsPanel::describe() const {
  return std::to_string(items_.size()) + " issues";
}

// ─── Register all ──────────────────────────────────────────

void register_all_panels(PanelHost& host) {
  host.add(std::make_unique<ProjectPanel>());
  host.add(std::make_unique<ViewerPanel>());
  host.add(std::make_unique<TimelinePanel>());
  host.add(std::make_unique<InspectorPanel>());
  host.add(std::make_unique<LayerPanel>());
  host.add(std::make_unique<EffectsPanel>());
  host.add(std::make_unique<GraphPanel>());
  host.add(std::make_unique<ScopesPanel>());
  host.add(std::make_unique<GradePanel>());
  host.add(std::make_unique<MixerPanel>());
  host.add(std::make_unique<RenderQueuePanel>());
  host.add(std::make_unique<TerminalPanel>());
  host.add(std::make_unique<CopilotPanel>());
  host.add(std::make_unique<ProblemsPanel>());
  host.set_workspace(Workspace::Edit);
}

} // namespace ui
} // namespace ms
