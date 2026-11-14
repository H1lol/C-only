#pragma once
/**
 * Pass 6 — 3D / DCC bridge
 * MotionStudio owns the scene; Blender / MoonRay / Gaffer run as external binaries.
 * We write job files + optional launch — we do NOT embed their engines.
 */
#include "ms_scene.hpp"
#include "ms_error.hpp"
#include "ms_image.hpp"
#include <string>
#include <vector>

namespace ms {

enum class DccTarget : uint8_t {
  Blender = 0,
  MoonRay,
  Gaffer,
  Appleseed,
};

struct DccBinaryPaths {
  std::string blender = "blender";       // or full path
  std::string moonray = "moonray";
  std::string gaffer = "gaffer";
  std::string gaffer_execute = "gaffer execute";
  std::string appleseed = "appleseed.cli";
};

struct DccJobResult {
  bool launched = false;
  bool binary_found = false;
  int exit_code = -1;
  std::string job_dir;
  std::string primary_script;
  std::string log;
  std::string message;
};

class DccBridge {
public:
  explicit DccBridge(DccBinaryPaths paths = {});

  void set_paths(const DccBinaryPaths& p) { paths_ = p; }
  const DccBinaryPaths& paths() const { return paths_; }

  /** Which binaries exist on PATH / given path */
  bool has_blender() const;
  bool has_moonray() const;
  bool has_gaffer() const;
  bool has_appleseed() const;

  /** Write job package for target under job_dir; does not require binary */
  Error write_job(const Scene& scene, DccTarget target, const std::string& job_dir,
                  std::string& primary_script_out);

  /**
   * Write job + optionally execute binary.
   * dry_run=true → only write files (CI / no tools installed).
   */
  DccJobResult submit(const Scene& scene, DccTarget target,
                      const std::string& job_dir, bool dry_run = true);

  /** Load a rendered image if present (PNG/PPM) after external run */
  Error load_render_preview(const std::string& image_path, ImageBuffer& out);

private:
  DccBinaryPaths paths_;
  static bool command_exists(const std::string& cmd);
  Error write_blender(const Scene& scene, const std::string& job_dir, std::string& script);
  Error write_moonray(const Scene& scene, const std::string& job_dir, std::string& script);
  Error write_gaffer(const Scene& scene, const std::string& job_dir, std::string& script);
  Error write_appleseed(const Scene& scene, const std::string& job_dir, std::string& script);
  int run_process(const std::string& cmd, std::string& log_out);
};

/** Convenience: default studio scene (camera + key light + mesh ref) */
Scene make_default_studio_scene(int width = 1920, int height = 1080);

} // namespace ms
