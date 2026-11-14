#pragma once
/**
 * Full panel set — every panel has one job (UI_CPP_ARCHITECTURE.md)
 */
#include "ui/ms_panels_edit.hpp"
#include "ms_msp.hpp"
#include "ms_render_backend.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>
#include <deque>
#include <mutex>

namespace ms {
namespace ui {

// ─── D. Layers (Motion) ────────────────────────────────────
class LayerPanel : public IPanel {
public:
  const char* id() const override { return "layers"; }
  const char* title() const override { return "LAYERS"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Motion);
  }
  void on_project_changed(ProjectSession* s) override;
  void on_time_changed(const UiTime& t) override;
  void on_selection_changed(const Selection& sel) override;
  std::string describe() const override;
  Error refresh() override;

  void bind_stack(LayerStack* stack) { stack_ = stack; refresh(); }
  LayerStack* stack() const { return stack_; }
  struct Row {
    uint64_t id = 0;
    std::string name;
    bool visible = true;
    bool locked = false;
    std::string blend;
    bool selected = false;
  };
  const std::vector<Row>& rows() const { return rows_; }

private:
  ProjectSession* session_ = nullptr;
  LayerStack* stack_ = nullptr;
  UiTime time_;
  Selection selection_;
  std::vector<Row> rows_;
};

// ─── E. Effects / Presets ──────────────────────────────────
class EffectsPanel : public IPanel {
public:
  const char* id() const override { return "effects"; }
  const char* title() const override { return "EFFECTS & PRESETS"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Motion) | workspace_bit(Workspace::Edit);
  }
  void on_project_changed(ProjectSession* s) override;
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection& sel) override;
  std::string describe() const override;
  Error refresh() override;

  struct Item {
    std::string id;
    std::string name;
    std::string category;
    bool open_source = false;
    bool installed = false;
  };
  const std::vector<Item>& items() const { return items_; }
  Error apply_to_selection(const std::string& effect_id, LayerStack* stack);

private:
  ProjectSession* session_ = nullptr;
  Selection selection_;
  std::vector<Item> items_;
};

// ─── G. Node graph ─────────────────────────────────────────
class GraphPanel : public IPanel {
public:
  const char* id() const override { return "graph"; }
  const char* title() const override { return "NODE GRAPH"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Nodes);
  }
  void on_project_changed(ProjectSession* s) override;
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection& sel) override;
  std::string describe() const override;
  Error refresh() override;

  struct Node {
    uint64_t id = 0;
    std::string name;
    std::string type;
    bool enabled = true;
  };
  struct Edge { uint64_t from = 0, to = 0; };
  uint64_t add_node(const std::string& type, const std::string& name);
  bool connect(uint64_t from, uint64_t to);
  bool set_enabled(uint64_t id, bool on);
  const std::vector<Node>& nodes() const { return nodes_; }
  const std::vector<Edge>& edges() const { return edges_; }

private:
  ProjectSession* session_ = nullptr;
  Selection selection_;
  std::vector<Node> nodes_;
  std::vector<Edge> edges_;
  uint64_t next_id_ = 1;
};

// ─── Color: Scopes + Grade ─────────────────────────────────
class ScopesPanel : public IPanel {
public:
  const char* id() const override { return "scopes"; }
  const char* title() const override { return "SCOPES"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Color);
  }
  void on_project_changed(ProjectSession*) override {}
  void on_time_changed(const UiTime& t) override { time_ = t; }
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override;
  /** Analyze viewer frame */
  Error analyze(const ImageBuffer& frame);
  float luma_avg() const { return luma_avg_; }
  float luma_min() const { return luma_min_; }
  float luma_max() const { return luma_max_; }

private:
  UiTime time_;
  float luma_avg_ = 0, luma_min_ = 0, luma_max_ = 1;
  bool has_data_ = false;
};

class GradePanel : public IPanel {
public:
  const char* id() const override { return "grade"; }
  const char* title() const override { return "GRADE"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Color);
  }
  void on_project_changed(ProjectSession*) override {}
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override { return Error::success(); }

  float exposure = 0.f;
  float contrast = 1.f;
  float saturation = 1.f;
  std::string ocio_view = "sRGB";
};

// ─── H. Mixer ──────────────────────────────────────────────
class MixerPanel : public IPanel {
public:
  const char* id() const override { return "mixer"; }
  const char* title() const override { return "MIXER"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Audio);
  }
  void on_project_changed(ProjectSession* s) override;
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override;

  struct Strip {
    std::string name;
    float level_db = 0.f;
    float peak = 0.f;
    float pan = 0.f;
    bool mute = false;
    bool solo = false;
  };
  std::vector<Strip>& strips() { return strips_; }
  const std::vector<Strip>& strips() const { return strips_; }

private:
  ProjectSession* session_ = nullptr;
  std::vector<Strip> strips_;
};

// ─── I. Render queue ───────────────────────────────────────
class RenderQueuePanel : public IPanel {
public:
  const char* id() const override { return "render_queue"; }
  const char* title() const override { return "RENDER QUEUE"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Render);
  }
  void on_project_changed(ProjectSession* s) override;
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override;

  struct Job {
    uint64_t id = 0;
    std::string name;
    std::string backend; // realtime_gpu | moonray | appleseed
    float progress = 0.f;
    std::string status;
  };
  uint64_t enqueue(const std::string& name, const std::string& backend);
  bool cancel(uint64_t id);
  const std::vector<Job>& jobs() const { return jobs_; }
  std::vector<std::string> available_backends() const;

private:
  ProjectSession* session_ = nullptr;
  std::vector<Job> jobs_;
  uint64_t next_id_ = 1;
};

// ─── J. Terminal ───────────────────────────────────────────
class TerminalPanel : public IPanel {
public:
  const char* id() const override { return "terminal"; }
  const char* title() const override { return "TERMINAL"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Dev);
  }
  void on_project_changed(ProjectSession* s) override;
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override { return Error::success(); }

  void set_cwd(const std::string& path) { cwd_ = path; }
  const std::string& cwd() const { return cwd_; }
  /** Run a shell command; capture last output lines */
  Error run(const std::string& command);
  const std::vector<std::string>& history() const { return history_; }
  const std::string& last_output() const { return last_output_; }

private:
  ProjectSession* session_ = nullptr;
  std::string cwd_ = ".";
  std::vector<std::string> history_;
  std::string last_output_;
};

// ─── K. Copilot ────────────────────────────────────────────
class CopilotPanel : public IPanel {
public:
  const char* id() const override { return "copilot"; }
  const char* title() const override { return "COPILOT"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Dev);
  }
  void on_project_changed(ProjectSession*) override {}
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override { return Error::success(); }

  struct Message {
    bool from_user = true;
    std::string text;
  };
  void ask(const std::string& prompt);
  void ingest_error(const std::string& error_text);
  const std::vector<Message>& messages() const { return messages_; }
  const std::string& last_suggestion() const { return last_suggestion_; }

private:
  std::vector<Message> messages_;
  std::string last_suggestion_;
};

// ─── L. Problems ───────────────────────────────────────────
class ProblemsPanel : public IPanel {
public:
  const char* id() const override { return "problems"; }
  const char* title() const override { return "PROBLEMS"; }
  uint32_t workspace_mask() const override {
    return workspace_bit(Workspace::Dev) | workspace_bit(Workspace::Render);
  }
  void on_project_changed(ProjectSession*) override {}
  void on_time_changed(const UiTime&) override {}
  void on_selection_changed(const Selection&) override {}
  std::string describe() const override;
  Error refresh() override { return Error::success(); }

  struct Item {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
    std::string context;
  };
  void push(const Error& e, const std::string& context = "");
  void clear();
  const std::vector<Item>& items() const { return items_; }

private:
  std::vector<Item> items_;
};

/** Register every panel into host */
void register_all_panels(PanelHost& host);

} // namespace ui
} // namespace ms
