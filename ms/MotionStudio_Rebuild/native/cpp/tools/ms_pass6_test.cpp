#include "ms_scene.hpp"
#include "ms_dcc_bridge.hpp"
#include "ms_image.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++g_fail; } \
  else std::cout << "  ok: " << m << "\n"; } while(0)

int main() {
  using namespace ms;
  namespace fs = std::filesystem;
  std::cout << "=== Pass 6 — 3D / DCC (Blender, MoonRay, Gaffer) ===\n";

  Scene scene = make_default_studio_scene(1280, 720);
  CHECK(scene.objects().size() >= 4, "default studio objects");
  CHECK(scene.find(1) != nullptr || !scene.objects().empty(), "scene has objects");

  std::string json = scene.to_job_json();
  CHECK(json.find("Camera") != std::string::npos, "job json has Camera");
  CHECK(json.find("backend") != std::string::npos, "job json meta");
  CHECK(json.find("\"rotation\"") != std::string::npos &&
        json.find("\"scale\"") != std::string::npos, "job json includes full transforms");
  CHECK(json.find("\"light_type\"") != std::string::npos, "job json includes light properties");

  DccBridge bridge;
  std::cout << "  info: blender=" << (bridge.has_blender() ? "yes" : "no")
            << " moonray=" << (bridge.has_moonray() ? "yes" : "no")
            << " gaffer=" << (bridge.has_gaffer() ? "yes" : "no") << "\n";

  // Blender job package
  auto r_b = bridge.submit(scene, DccTarget::Blender, "/tmp/ms_dcc_blender", true);
  CHECK(fs::exists(r_b.primary_script), "blender script written");
  CHECK(fs::exists("/tmp/ms_dcc_blender/scene_job.json"), "blender scene_job.json");
  std::ifstream blender_file(r_b.primary_script);
  std::string blender_script((std::istreambuf_iterator<char>(blender_file)), std::istreambuf_iterator<char>());
  CHECK(blender_script.find("bpy.ops.import_scene.gltf") != std::string::npos &&
        blender_script.find("bpy.ops.import_scene.fbx") != std::string::npos,
        "blender bridge supports glTF and FBX asset import");
  CHECK(blender_script.find("rotation_quaternion") != std::string::npos &&
        blender_script.find("make_material") != std::string::npos,
        "blender bridge applies transforms and materials");
  CHECK(r_b.exit_code == 0, "blender dry_run ok");

  // MoonRay
  auto r_m = bridge.submit(scene, DccTarget::MoonRay, "/tmp/ms_dcc_moonray", true);
  CHECK(fs::exists(r_m.primary_script), "moonray rdla written");
  CHECK(fs::exists("/tmp/ms_dcc_moonray/run_moonray.sh"), "moonray shell");
  CHECK(r_m.exit_code == 0, "moonray dry_run ok");

  // Gaffer
  auto r_g = bridge.submit(scene, DccTarget::Gaffer, "/tmp/ms_dcc_gaffer", true);
  CHECK(fs::exists(r_g.primary_script), "gaffer script written");
  CHECK(fs::exists("/tmp/ms_dcc_gaffer/run_gaffer.sh"), "gaffer shell");
  CHECK(r_g.exit_code == 0, "gaffer dry_run ok");

  // Appleseed offline-render package
  auto r_a = bridge.submit(scene, DccTarget::Appleseed, "/tmp/ms_dcc_appleseed", true);
  CHECK(fs::exists(r_a.primary_script), "appleseed project written");
  CHECK(fs::exists("/tmp/ms_dcc_appleseed/run_appleseed.sh"), "appleseed launcher written");
  CHECK(r_a.exit_code == 0, "appleseed dry_run ok");
  std::ifstream appleseed_file(r_a.primary_script);
  std::string appleseed_xml((std::istreambuf_iterator<char>(appleseed_file)), std::istreambuf_iterator<char>());
  CHECK(appleseed_xml.find("<project") != std::string::npos && appleseed_xml.find("1280 720") != std::string::npos,
        "appleseed project carries scene resolution");

  // Branding logo present in tree
  CHECK(fs::exists("assets/branding/ic_motion_studio_logo.png") ||
        fs::exists("/home/workdir/artifacts/MotionStudio_Rebuild/assets/branding/ic_motion_studio_logo.png"),
        "MS logo asset");

  // Gaffer python script runs without gaffer binary
  int py = std::system("python3 /tmp/ms_dcc_gaffer/ms_gaffer_job.py >/tmp/ms_gaffer_out.txt 2>&1");
  CHECK(py == 0, "gaffer job python runs");

  std::cout << (g_fail ? "\nPASS 6 FAILED\n" : "\nPASS 6 PASSED\n");
  return g_fail ? 1 : 0;
}
