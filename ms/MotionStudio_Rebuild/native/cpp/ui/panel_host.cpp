#include "ui/ms_panel.hpp"
#include <sstream>

namespace ms {
namespace ui {

void PanelHost::add(std::unique_ptr<IPanel> p) {
  panels_.push_back(std::move(p));
}

IPanel* PanelHost::get(const char* id) {
  for (auto& p : panels_) {
    if (p && std::string(p->id()) == id) return p.get();
  }
  return nullptr;
}

std::vector<IPanel*> PanelHost::visible(Workspace w) {
  const uint32_t bit = workspace_bit(w);
  std::vector<IPanel*> out;
  for (auto& p : panels_) {
    if (p && (p->workspace_mask() & bit)) out.push_back(p.get());
  }
  return out;
}

void PanelHost::set_workspace(Workspace w) {
  workspace_ = w;
}

void PanelHost::set_session(ProjectSession* s) {
  session_ = s;
  for (auto& p : panels_) if (p) p->on_project_changed(s);
}

void PanelHost::set_time(const UiTime& t) {
  time_ = t;
  for (auto& p : panels_) if (p) p->on_time_changed(t);
}

void PanelHost::set_selection(const Selection& sel) {
  selection_ = sel;
  for (auto& p : panels_) if (p) p->on_selection_changed(sel);
}

std::string PanelHost::dump_layout() const {
  std::ostringstream os;
  os << "workspace=" << workspace_name(workspace_) << "\n";
  const uint32_t bit = workspace_bit(workspace_);
  for (const auto& p : panels_) {
    if (!p) continue;
    const bool vis = (p->workspace_mask() & bit) != 0;
    os << (vis ? "[x] " : "[ ] ") << p->title() << " (" << p->id() << ")\n";
    if (vis) os << "    " << p->describe() << "\n";
  }
  return os.str();
}

} // namespace ui
} // namespace ms
