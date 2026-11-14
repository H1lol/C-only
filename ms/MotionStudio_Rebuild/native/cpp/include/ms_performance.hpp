#pragma once
#include "ms_gpu.hpp"
#include <chrono>
#include <string>

namespace ms {
struct PerformanceStats { double frame_ms=0; double fps=0; uint64_t rendered=0; uint64_t cache_hits=0; uint64_t cache_misses=0; size_t cache_bytes=0; std::string backend="unknown"; };
class PerformanceMonitor {
public:
  void begin_frame();
  void end_frame(bool cache_hit);
  void set_backend(const std::string& name);
  void set_cache_bytes(size_t bytes);
  PerformanceStats stats() const { return stats_; }
  bool within_budget(double budget_ms=33.333) const { return stats_.frame_ms <= budget_ms; }
private:
  std::chrono::steady_clock::time_point start_{}; PerformanceStats stats_{};
};
class RenderBudget {
public:
  explicit RenderBudget(double target_fps=30.0): target_fps_(target_fps) {}
  void set_target_fps(double fps); double target_fps() const { return target_fps_; }
  double frame_budget_ms() const { return target_fps_ > 0 ? 1000.0/target_fps_ : 33.333; }
  int preview_scale(double measured_ms) const;
private: double target_fps_ = 30.0;
};
} // namespace ms
