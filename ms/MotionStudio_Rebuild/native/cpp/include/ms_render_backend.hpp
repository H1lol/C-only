#pragma once
/**
 * Pluggable render backends.
 * Realtime GPU = default on device.
 * MoonRay = optional film path tracer (desktop/cloud), not shipped in mobile APK.
 */
#include "ms_error.hpp"
#include "ms_image.hpp"
#include <memory>
#include <string>
#include <functional>

namespace ms {

struct RenderJobDesc {
  int width = 1920;
  int height = 1080;
  float time_sec = 0.f;
  int samples = 1;           // path tracer samples; realtime ignores
  std::string scene_path;    // RDL/USD/ms internal snapshot path for offline backends
  bool progressive = true;
};

struct RenderProgress {
  float fraction = 0.f;      // 0..1
  int pass = 0;
  std::string message;
};

using ProgressFn = std::function<void(const RenderProgress&)>;

class IRenderBackend {
public:
  virtual ~IRenderBackend() = default;
  virtual const char* name() const = 0;
  virtual bool available() const = 0;
  virtual Error render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) = 0;
};

/** Default: uses existing RenderPipeline / GPU mock */
class RealtimeGpuBackend : public IRenderBackend {
public:
  const char* name() const override { return "realtime_gpu"; }
  bool available() const override { return true; }
  Error render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) override;
};

/**
 * MoonRay backend — optional.
 * Does not link libmoonray into mobile. Desktop/cloud:
 *  - write scene snapshot
 *  - exec moonray or call Arras
 *  - read EXR/PNG into ImageBuffer
 */
class MoonRayBackend : public IRenderBackend {
public:
  explicit MoonRayBackend(std::string moonray_cli_path = {});
  const char* name() const override { return "moonray"; }
  bool available() const override;
  Error render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) override;
  void set_cli_path(std::string path) { cli_ = std::move(path); }

private:
  std::string cli_;
};

/** Appleseed — MIT-licensed PBR/GI engine; optional offline backend like MoonRay */
class AppleseedBackend : public IRenderBackend {
public:
  explicit AppleseedBackend(std::string cli_path = {});
  const char* name() const override { return "appleseed"; }
  bool available() const override;
  Error render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) override;
  void set_cli_path(std::string path) { cli_ = std::move(path); }

private:
  std::string cli_;
};


/** Unreal Engine — not embedded. Optional remote/editor job (user has UE source separately). */
class UnrealRemoteBackend : public IRenderBackend {
public:
  const char* name() const override { return "unreal_remote"; }
  bool available() const override { return false; } // enable when bridge configured
  Error render(const RenderJobDesc& job, ImageBuffer& out, ProgressFn progress) override;
};

class RenderBackendRegistry {
public:
  static RenderBackendRegistry& instance();
  void register_backend(std::shared_ptr<IRenderBackend> b);
  std::shared_ptr<IRenderBackend> get(const std::string& name) const;
  std::vector<std::string> list_available() const;

private:
  std::vector<std::shared_ptr<IRenderBackend>> backends_;
};

} // namespace ms
