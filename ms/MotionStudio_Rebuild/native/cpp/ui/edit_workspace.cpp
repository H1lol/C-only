#include "ui/ms_panel.hpp"
#include "ui/ms_panels_edit.hpp"
#include "ms_project.hpp"
#include "ms_layer.hpp"
#include <memory>

namespace ms {
namespace ui {

/** Build Edit workspace: Project | Viewer | Timeline | Inspector */
void register_edit_workspace(PanelHost& host) {
  host.add(std::make_unique<ProjectPanel>());
  host.add(std::make_unique<ViewerPanel>());
  host.add(std::make_unique<TimelinePanel>());
  host.add(std::make_unique<InspectorPanel>());
  host.set_workspace(Workspace::Edit);
}

} // namespace ui
} // namespace ms
