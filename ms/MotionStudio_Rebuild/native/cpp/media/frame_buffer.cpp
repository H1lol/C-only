#include "ms_frame_buffer.hpp"
#include <cstring>
#include <algorithm>

namespace ms {

FrameBufferHub& FrameBufferHub::instance() {
  static FrameBufferHub h;
  return h;
}

Error FrameBufferHub::publish_rgba(int width, int height, const uint8_t* data, int stride,
                                   int64_t pts_ms) {
  if (width <= 0 || height <= 0 || !data)
    return Error::make(ErrorCode::InvalidArgument, "publish_rgba args");
  if (stride < width * 4) stride = width * 4;
  std::lock_guard<std::mutex> lock(mu_);
  slot_.width = width;
  slot_.height = height;
  slot_.stride = stride;
  slot_.pts_ms = pts_ms;
  size_t need = static_cast<size_t>(stride) * static_cast<size_t>(height);
  slot_.rgba.resize(need);
  if (stride == width * 4) {
    std::memcpy(slot_.rgba.data(), data, need);
  } else {
    for (int y = 0; y < height; ++y)
      std::memcpy(slot_.rgba.data() + y * stride, data + y * stride, width * 4);
  }
  count_.fetch_add(1);
  return Error::success();
}

bool FrameBufferHub::latest(FrameBufferSlot& out) const {
  std::lock_guard<std::mutex> lock(mu_);
  if (!slot_.valid()) return false;
  out = slot_;
  return true;
}

void FrameBufferHub::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  slot_ = {};
  count_.store(0);
}

} // namespace ms

extern "C" int ms_frame_buffer_publish_rgba(int width, int height,
                                            const unsigned char* data, int stride,
                                            long long pts_ms) {
  auto e = ms::FrameBufferHub::instance().publish_rgba(width, height, data, stride, pts_ms);
  return e.ok() ? 0 : static_cast<int>(e.code);
}

extern "C" int ms_frame_buffer_latest(int* width, int* height, long long* pts_ms,
                                      unsigned char* out_rgba, int out_capacity) {
  ms::FrameBufferSlot s;
  if (!ms::FrameBufferHub::instance().latest(s)) return -1;
  if (width) *width = s.width;
  if (height) *height = s.height;
  if (pts_ms) *pts_ms = s.pts_ms;
  if (out_rgba && out_capacity > 0) {
    int n = std::min(out_capacity, static_cast<int>(s.rgba.size()));
    std::memcpy(out_rgba, s.rgba.data(), n);
  }
  return 0;
}

extern "C" uint64_t ms_frame_buffer_publish_count(void) {
  return ms::FrameBufferHub::instance().publish_count();
}
