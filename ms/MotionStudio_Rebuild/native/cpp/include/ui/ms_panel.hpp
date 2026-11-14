#pragma once
#include "ui/ms_theme.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace ms {

class ProjectSession; // fwd from engine

namespace ui {

/** Selection shared across panels — inspector listens, others publish */
struct Selection {
  enum class Kind { None, Layer, Clip, Effect, Asset } kind = Kind::None;
  uint64_t id = 0;
  std::string name;
};

struct UiTime {
  double seconds = 0.0;
  bool playing = false;
};

/**
 * Every panel: one job, bound to engine state.
 * No decorative panels without on_* handlers doing real work.
 */
class IPanel {
public:
  virtual ~IPanel() = default;
  virtual const char* id() const = 0;
  virtual const char* title() const = 0;
  /** Bitmask of workspaces where this panel is shown */
  virtual uint32_t workspace_mask() const = 0;
  virtual void on_project_changed(ProjectSession* session) = 0;
  virtual void on_time_changed(const UiTime& t) = 0;
  virtual void on_selection_changed(const Selection& sel) = 0;
  /** Human-readable snapshot for CLI / tests / future GUI bind */
  virtual std::string describe() const = 0;
  virtual Error refresh() = 0;
};

inline uint32_t workspace_bit(Workspace w) {
  return 1u << static_cast<int>(w);
}

class PanelHost {
public:
  void add(std::unique_ptr<IPanel> p);
  IPanel* get(const char* id);
  std::vector<IPanel*> visible(Workspace w);
  void set_workspace(Workspace w);
  Workspace workspace() const { return workspace_; }
  void set_session(ProjectSession* s);
  void set_time(const UiTime& t);
  void set_selection(const Selection& sel);
  const Selection& selection() const { return selection_; }
  const UiTime& time() const { return time_; }
  ProjectSession* session() const { return session_; }
  std::string dump_layout() const;

private:
  std::vector<std::unique_ptr<IPanel>> panels_;
  Workspace workspace_ = Workspace::Edit;
  ProjectSession* session_ = nullptr;
  UiTime time_;
  Selection selection_;
};

} // namespace ui
} // namespace ms
