#pragma once
/**
 * Milestone 0 frame transport — RGBA ring published for UI / Texture
 */
#include "ms_error.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace ms {

struct FrameBufferSlot {
  int width = 0;
  int height = 0;
  int stride = 0; // bytes per row
  int64_t pts_ms = 0;
  std::vector<uint8_t> rgba; // size stride * height, format RGBA8
  bool valid() const {
    return width > 0 && height > 0 &&
           rgba.size() >= static_cast<size_t>(stride) * static_cast<size_t>(height);
  }
};

/**
 * Thread-safe latest-frame publisher (single consumer UI pattern).
 * C ABI wrapper: ms_frame_buffer_publish_rgba
 */
class FrameBufferHub {
public:
  static FrameBufferHub& instance();

  Error publish_rgba(int width, int height, const uint8_t* data, int stride,
                     int64_t pts_ms = 0);

  /** Copy latest frame into out; returns false if none yet */
  bool latest(FrameBufferSlot& out) const;

  uint64_t publish_count() const { return count_.load(); }
  void clear();

private:
  FrameBufferHub() = default;
  mutable std::mutex mu_;
  FrameBufferSlot slot_;
  std::atomic<uint64_t> count_{0};
};

} // namespace ms

extern "C" {
/** Returns 0 on success, non-zero on error */
int ms_frame_buffer_publish_rgba(int width, int height, const unsigned char* data,
                                 int stride, long long pts_ms);
int ms_frame_buffer_latest(int* width, int* height, long long* pts_ms,
                           unsigned char* out_rgba, int out_capacity);
uint64_t ms_frame_buffer_publish_count(void);
}
