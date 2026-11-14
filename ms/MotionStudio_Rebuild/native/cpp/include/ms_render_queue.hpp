#pragma once
/**
 * Phase 5 — Offline render queue
 */
#include "ms_render_backend.hpp"
#include "ms_timeline_player.hpp"
#include "ms_error.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

namespace ms {

enum class JobState : uint8_t { Queued, Running, Done, Failed, Cancelled };

struct QueueJob {
  uint64_t id = 0;
  std::string name;
  std::string backend = "realtime_cpu"; // realtime_cpu | realtime_gpu | moonray | appleseed
  RenderJobDesc desc;
  JobState state = JobState::Queued;
  std::string error;
  float progress = 0.f;
  std::string out_path; // optional sequence dir or single frame path
};

class RenderQueue {
public:
  explicit RenderQueue(TimelinePlayer* player = nullptr);
  ~RenderQueue();

  void set_player(TimelinePlayer* player) { player_ = player; }
  uint64_t enqueue(QueueJob job);
  bool cancel(uint64_t id);
  const std::vector<QueueJob>& jobs() const { return jobs_; }

  /** Process next queued job (sync for Phase 5) */
  Error process_next();

  /** Process all queued */
  Error process_all();

  // Background worker for mobile-friendly render queues.
  bool start_async();
  void wait();
  bool running() const { return running_.load(); }

  IRenderBackend* backend_named(const std::string& name);

private:
  TimelinePlayer* player_ = nullptr;
  std::vector<QueueJob> jobs_;
  uint64_t next_id_ = 1;
  RealtimeGpuBackend gpu_;
  MoonRayBackend moonray_;
  AppleseedBackend appleseed_;
  std::thread worker_;
  std::atomic<bool> running_{false};
  std::mutex jobs_mutex_;
};

/** CPU offline path using TimelinePlayer frames */
class RealtimeCpuBackend : public IRenderBackend {
public:
  explicit RealtimeCpuBackend(TimelinePlayer* player = nullptr) : player_(player) {}
  void set_player(TimelinePlayer* p) { player_ = p; }
  const char* name() const override { return "realtime_cpu"; }
  bool available() const override { return true; }
  Error render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) override;
private:
  TimelinePlayer* player_ = nullptr;
};

} // namespace ms
