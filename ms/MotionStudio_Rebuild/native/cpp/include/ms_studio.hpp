#pragma once
/**
 * MotionStudio hub — single entry that wires project, timeline, layers,
 * effects, queue, DCC, copilot, MSP. Peak integration layer.
 */
#include "ms_project.hpp"
#include "ms_timeline_player.hpp"
#include "ms_layer.hpp"
#include "ms_render_queue.hpp"
#include "ms_dcc_bridge.hpp"
#include "ms_copilot.hpp"
#include "ms_msp.hpp"
#include "ms_finishing.hpp"
#include "ms_scene.hpp"
#include "ms_animation.hpp"
#include "ms_audio.hpp"
#include "ms_error.hpp"
#include <string>
#include <memory>

namespace ms {

struct StudioInfo {
  std::string name = "MotionStudio";
  std::string version = "2026.0.0-alpha";
  std::string logo_asset = "assets/branding/ic_motion_studio_logo.png";
};

class StudioHub {
public:
  StudioHub();

  const StudioInfo& info() const { return info_; }

  ProjectSession& project() { return project_; }
  TimelinePlayer& player() { return player_; }
  LayerStack& layers() { return layers_; }
  RenderQueue& queue() { return queue_; }
  DccBridge& dcc() { return dcc_; }
  Scene& scene() { return scene_; }
  AudioMixer& audio() { return audio_; }
  MspRegistry& plugins() { return MspRegistry::instance(); }

  /** New empty project at given resolution */
  Error new_project(const std::string& name, int w = 1920, int h = 1080, float fps = 24.f);

  /** Import media + place clip on first video track */
  Error import_and_place(const std::string& path, TimeSec at = 0, TimeSec duration = 5);

  /** Render preview frame at playhead into out */
  Error preview(ImageBuffer& out);

  /** Enqueue CPU offline frame */
  uint64_t enqueue_preview_render(const std::string& out_ppm);

  /** Write DCC jobs (dry-run safe) */
  DccJobResult export_dcc(DccTarget target, const std::string& job_dir, bool dry_run = true);

  /** Local Copilot with project context */
  CopilotSuggestion ask(const std::string& question);

  /** Scan plugins root */
  Error scan_plugins(const std::string& root);

  /** Status line for shell/UI */
  std::string status_line() const;

  TimeSec playhead() const { return playhead_; }
  void set_playhead(TimeSec t) { playhead_ = t; }

private:
  StudioInfo info_;
  ProjectSession project_;
  TimelinePlayer player_;
  LayerStack layers_;
  RenderQueue queue_;
  DccBridge dcc_;
  Scene scene_;
  AudioMixer audio_;
  TimeSec playhead_ = 0;
};

} // namespace ms
