#include "ms_render_backend.hpp"
#include <cstdlib>

namespace ms {

Error RealtimeGpuBackend::render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) {
  if (job.width <= 0 || job.height <= 0) {
    return Error::make(ErrorCode::InvalidArgument, "bad resolution");
  }
  if (!out.allocate(job.width, job.height, PixelFormat::RGBA8)) {
    return Error::make(ErrorCode::OutOfMemory, "realtime buffer");
  }
  out.clear(0.08f, 0.08f, 0.1f, 1.f);
  if (progress) progress({1.f, 1, "realtime complete"});
  return Error::success();
}

MoonRayBackend::MoonRayBackend(std::string moonray_cli_path) : cli_(std::move(moonray_cli_path)) {}

bool MoonRayBackend::available() const {
  if (!cli_.empty()) return true;
  return std::system("command -v moonray >/dev/null 2>&1") == 0;
}

Error MoonRayBackend::render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) {
  if (!available()) {
    return Error::make(ErrorCode::NotSupported, "MoonRay not installed or not configured");
  }
  (void)job;
  (void)out;
  if (progress) progress({0.f, 0, "MoonRay backend: wire CLI/Arras to write EXR then load"});
  return Error::make(ErrorCode::NotSupported, "MoonRayBackend::render not wired to CLI yet");
}

AppleseedBackend::AppleseedBackend(std::string cli_path) : cli_(std::move(cli_path)) {}

bool AppleseedBackend::available() const {
  if (!cli_.empty()) return true;
  return std::system("command -v appleseed.cli >/dev/null 2>&1") == 0
      || std::system("command -v appleseed.studio >/dev/null 2>&1") == 0;
}

Error AppleseedBackend::render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) {
  if (!available()) {
    return Error::make(ErrorCode::NotSupported, "Appleseed not installed or not configured");
  }
  (void)job;
  (void)out;
  if (progress) progress({0.f, 0, "Appleseed backend: wire appleseed.cli project → EXR"});
  return Error::make(ErrorCode::NotSupported, "AppleseedBackend::render not wired to CLI yet");
}

Error UnrealRemoteBackend::render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) {
  (void)job; (void)out;
  if (progress) progress({0.f, 0, "Unreal remote: export scene job JSON to UE; not in-process"});
  return Error::make(ErrorCode::NotSupported, "UnrealRemoteBackend not configured");
}

RenderBackendRegistry& RenderBackendRegistry::instance() {
  static RenderBackendRegistry r;
  return r;
}

void RenderBackendRegistry::register_backend(std::shared_ptr<IRenderBackend> b) {
  backends_.push_back(std::move(b));
}

std::shared_ptr<IRenderBackend> RenderBackendRegistry::get(const std::string& name) const {
  for (const auto& b : backends_) {
    if (b && name == b->name()) return b;
  }
  return nullptr;
}

std::vector<std::string> RenderBackendRegistry::list_available() const {
  std::vector<std::string> n;
  for (const auto& b : backends_) {
    if (b && b->available()) n.push_back(b->name());
  }
  return n;
}

} // namespace ms
