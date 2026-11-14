#include "ms_render_queue.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace ms {

Error RealtimeCpuBackend::render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) {
  if (player_) {
    Error e = player_->prepare();
    if (!e.ok()) return e;
    e = player_->render_frame(job.time_sec, out);
    if (progress) progress({1.f, 1, "cpu timeline frame"});
    return e;
  }
  if (!out.allocate(job.width, job.height, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "cpu frame");
  out.clear(0.12f, 0.12f, 0.15f, 1.f);
  if (progress) progress({1.f, 1, "cpu solid"});
  return Error::success();
}

RenderQueue::RenderQueue(TimelinePlayer* player) : player_(player) {}
RenderQueue::~RenderQueue() { wait(); }

uint64_t RenderQueue::enqueue(QueueJob job) {
  job.id = next_id_++;
  job.state = JobState::Queued;
  job.progress = 0.f;
  jobs_.push_back(std::move(job));
  return jobs_.back().id;
}

bool RenderQueue::cancel(uint64_t id) {
  for (auto& j : jobs_) {
    if (j.id == id && j.state == JobState::Queued) {
      j.state = JobState::Cancelled;
      return true;
    }
  }
  return false;
}

IRenderBackend* RenderQueue::backend_named(const std::string& name) {
  if (name == "realtime_gpu" || name == "gpu") return &gpu_;
  if (name == "moonray") return &moonray_;
  if (name == "appleseed") return &appleseed_;
  return nullptr; // cpu handled separately
}

Error RenderQueue::process_next() {
  QueueJob* job = nullptr;
  for (auto& j : jobs_) {
    if (j.state == JobState::Queued) { job = &j; break; }
  }
  if (!job) return Error::success();

  job->state = JobState::Running;
  ImageBuffer frame;
  ProgressFn prog = [job](const RenderProgress& p) {
    job->progress = p.fraction;
  };

  Error e = Error::success();
  if (job->backend == "realtime_cpu" || job->backend == "cpu" || job->backend.empty()) {
    RealtimeCpuBackend cpu(player_);
    e = cpu.render(job->desc, frame, prog);
  } else {
    IRenderBackend* be = backend_named(job->backend);
    if (!be || !be->available()) {
      job->state = JobState::Failed;
      job->error = "backend unavailable: " + job->backend;
      return Error::make(ErrorCode::NotSupported, job->error);
    }
    e = be->render(job->desc, frame, prog);
  }

  if (!e.ok()) {
    job->state = JobState::Failed;
    job->error = e.message.empty() ? "render failed" : e.message;
    return e;
  }

  if (!job->out_path.empty() && frame.valid()) {
    namespace fs = std::filesystem;
    fs::path path(job->out_path);
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    // write PPM single frame
    std::ofstream f(job->out_path, std::ios::binary);
    if (f) {
      f << "P6\n" << frame.width() << " " << frame.height() << "\n255\n";
      for (int y = 0; y < frame.height(); ++y) {
        const uint8_t* row = frame.data() + y * frame.stride();
        for (int x = 0; x < frame.width(); ++x) {
          f.put(static_cast<char>(row[x*4]));
          f.put(static_cast<char>(row[x*4+1]));
          f.put(static_cast<char>(row[x*4+2]));
        }
      }
    }
  }

  job->progress = 1.f;
  job->state = JobState::Done;
  return Error::success();
}

Error RenderQueue::process_all() {
  for (;;) {
    bool any = false;
    for (const auto& j : jobs_) if (j.state == JobState::Queued) { any = true; break; }
    if (!any) break;
    Error e = process_next();
    if (!e.ok()) return e;
  }
  return Error::success();
}

} // namespace ms

namespace ms { bool RenderQueue::start_async(){ if(running_.exchange(true)) return false; worker_=std::thread([this]{process_all();running_.store(false);}); return true;} void RenderQueue::wait(){if(worker_.joinable())worker_.join();running_.store(false);} }
