#pragma once
/** Lightweight scene graph — Blender/UE/MoonRay ideas without embedding them */
#include <string>
#include <vector>
#include <cstdint>

namespace ms {

struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };

struct Transform3D {
  Vec3 position;
  Quat rotation;
  Vec3 scale {1, 1, 1};
};

enum class SceneObjectType : uint8_t {
  Empty = 0, Camera, Light, MeshRef, Volume, Locator
};

enum class LightType : uint8_t { Point = 0, Directional, Spot, Area };

struct Material3D { std::string name="Default"; float base_r=.8f,base_g=.8f,base_b=.8f,base_a=1.f; float metallic=0.f,roughness=.5f,emission=0.f; std::string base_color_texture; };

struct SceneObject {
  uint64_t id = 0;
  std::string name;
  SceneObjectType type = SceneObjectType::Empty;
  Transform3D xform;
  uint64_t parent = 0;
  bool visible = true;

  // Camera
  float focal_mm = 35.f;
  float sensor_mm = 36.f;
  float near_clip = 0.1f;
  float far_clip = 10000.f;

  // Light
  LightType light = LightType::Directional;
  float intensity = 1.f;
  float color_r = 1, color_g = 1, color_b = 1;

  // Mesh reference (path / asset id — not full UE mesh)
  std::string asset_ref;
  Material3D material;
};

struct RenderSettings {
  int width = 1920;
  int height = 1080;
  int samples = 64;
  std::string backend = "realtime_gpu"; // moonray | appleseed | unreal_remote
  bool progressive = true;
  std::string color_space = "ACEScg";
  std::string output_path;
};

class Scene {
public:
  uint64_t add(SceneObject o);
  bool remove(uint64_t id);
  SceneObject* find(uint64_t id);
  const std::vector<SceneObject>& objects() const { return objects_; }
  RenderSettings& render() { return render_; }
  const RenderSettings& render() const { return render_; }
  /** Export a minimal scene description for offline backends */
  std::string to_job_json() const;

private:
  std::vector<SceneObject> objects_;
  RenderSettings render_;
  uint64_t next_id_ = 1;
};

} // namespace ms
