#pragma once
#include "ui/ms_panel.hpp"
#include "ms_layer.hpp"
#include "ms_image.hpp"
#include <string>
#include <vector>

namespace ms {
namespace ui {

/** Project — what assets and comps exist */
class ProjectPanel : public IPanel {
public:
  const char* id() const override { return "project"; }
  const char* title() const override { return "PROJECT"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Edit) | workspace_bit(Workspace::Motion) |
           workspace_bit(Workspace::Nodes) | workspace_bit(Workspace::Color);
  }
  void on_project_changed(ProjectSession* session) override;
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override;

  const std::vector<std::string>& entries() const { return entries_; }

private:
  ProjectSession* session_ = nullptr;
  std::vector<std::string> entries_;
  std::string project_name_ = "Untitled";
};

/** Viewer — what does this frame look like */
class ViewerPanel : public IPanel {
public:
  const char* id() const override { return "viewer"; }
  const char* title() const override { return "COMPOSITION"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Edit) | workspace_bit(Workspace::Motion) |
           workspace_bit(Workspace::Nodes) | workspace_bit(Workspace::Color) |
           workspace_bit(Workspace::Render);
  }
  void on_project_changed(ProjectSession* session) override;
  void on_time_changed(const UiTime& t) override;
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override;

  /** Composite current layers into buffer (real pixels) */
  Error render_preview(LayerStack& layers, int w, int h);
  const ImageBuffer& frame() const { return frame_; }
  int width() const { return width_; }
  int height() const { return height_; }

private:
  ProjectSession* session_ = nullptr;
  UiTime time_;
  ImageBuffer frame_;
  int width_ = 1080;
  int height_ = 1920;
  std::string status_ = "idle";
};

/** Timeline — when does it happen */
class TimelinePanel : public IPanel {
public:
  const char* id() const override { return "timeline"; }
  const char* title() const override { return "TIMELINE"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Edit) | workspace_bit(Workspace::Audio);
  }
  void on_project_changed(ProjectSession* session) override;
  void on_time_changed(const UiTime& t) override;
  void on_selection_changed(const Selection& sel) override;
  std::string describe() const override;
  Error refresh() override;

  struct Row {
    std::string name;
    double start = 0, end = 0;
    bool selected = false;
  };
  const std::vector<Row>& rows() const { return rows_; }
  double playhead() const { return time_.seconds; }

private:
  ProjectSession* session_ = nullptr;
  UiTime time_;
  Selection selection_;
  std::vector<Row> rows_;
};

/** Inspector — what is selected, exactly */
class InspectorPanel : public IPanel {
public:
  const char* id() const override { return "inspector"; }
  const char* title() const override { return "PROPERTIES"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Edit) | workspace_bit(Workspace::Motion) |
           workspace_bit(Workspace::Nodes) | workspace_bit(Workspace::Color);
  }
  void on_project_changed(ProjectSession* session) override;
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection& sel) override;
  std::string describe() const override;
  Error refresh() override;

  struct Field {
    std::string label;
    std::string value;
  };
  const std::vector<Field>& fields() const { return fields_; }

private:
  ProjectSession* session_ = nullptr;
  Selection selection_;
  std::vector<Field> fields_;
};

} // namespace ui
} // namespace ms
