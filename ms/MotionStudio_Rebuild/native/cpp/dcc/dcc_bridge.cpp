#include "ms_dcc_bridge.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <array>
#include <cstdio>
#include <algorithm>
#include <iomanip>
#include <sys/wait.h>

namespace ms {
namespace fs = std::filesystem;

DccBridge::DccBridge(DccBinaryPaths paths) : paths_(std::move(paths)) {}

bool DccBridge::command_exists(const std::string& cmd) {
  // Take first token if spaces (e.g. "gaffer execute")
  std::string base = cmd;
  auto sp = base.find(' ');
  if (sp != std::string::npos) base = base.substr(0, sp);
  if (base.empty()) return false;
  // Absolute path
  if (base[0] == '/' || (base.size() > 2 && base[1] == ':'))
    return fs::exists(base);
  std::string c = "command -v " + base + " >/dev/null 2>&1";
  return std::system(c.c_str()) == 0;
}

bool DccBridge::has_blender() const { return command_exists(paths_.blender); }
bool DccBridge::has_moonray() const { return command_exists(paths_.moonray); }
bool DccBridge::has_gaffer() const {
  return command_exists(paths_.gaffer) || command_exists(paths_.gaffer_execute);
}
bool DccBridge::has_appleseed() const { return command_exists(paths_.appleseed); }

Scene make_default_studio_scene(int width, int height) {
  Scene s;
  s.render().width = width;
  s.render().height = height;
  s.render().samples = 64;
  s.render().color_space = "ACEScg";

  SceneObject cam;
  cam.name = "Camera";
  cam.type = SceneObjectType::Camera;
  cam.xform.position = {0, 1.5f, 4.f};
  cam.focal_mm = 35.f;
  s.add(cam);

  SceneObject key;
  key.name = "KeyLight";
  key.type = SceneObjectType::Light;
  key.light = LightType::Area;
  key.xform.position = {2.f, 3.f, 2.f};
  key.intensity = 5.f;
  key.color_r = 1.f; key.color_g = 0.98f; key.color_b = 0.95f;
  s.add(key);

  SceneObject fill;
  fill.name = "FillLight";
  fill.type = SceneObjectType::Light;
  fill.light = LightType::Directional;
  fill.xform.position = {-2.f, 2.f, 1.f};
  fill.intensity = 1.5f;
  s.add(fill);

  SceneObject mesh;
  mesh.name = "HeroMesh";
  mesh.type = SceneObjectType::MeshRef;
  mesh.asset_ref = "assets/meshes/hero.obj";
  mesh.xform.scale = {1, 1, 1};
  s.add(mesh);

  return s;
}

Error DccBridge::write_blender(const Scene& scene, const std::string& job_dir, std::string& script) {
  fs::create_directories(job_dir);
  // scene dump
  {
    std::ofstream j(job_dir + "/scene_job.json");
    j << scene.to_job_json();
  }
  script = job_dir + "/ms_blender_job.py";
  std::ofstream py(script);
  if (!py) return Error::make(ErrorCode::FilePermission, "cannot write blender script", script);

  py <<
R"PY(# MotionStudio -> Blender 5.2+ scene bridge
# Run: blender -b -P ms_blender_job.py
import json
from pathlib import Path

JOB = Path(__file__).resolve().parent
OUT = JOB / "render"
OUT.mkdir(exist_ok=True)
data = json.loads((JOB / "scene_job.json").read_text(encoding="utf-8"))

try:
    import bpy
except ImportError as exc:
    raise SystemExit("MotionStudio: run this script with Blender's bundled Python") from exc

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = max(1, int(data.get("samples", 64)))
scene.render.resolution_x = max(16, int(data.get("width", 1920)))
scene.render.resolution_y = max(16, int(data.get("height", 1080)))
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(OUT / "frame_0001.png")
scene.render.film_transparent = False

created_by_id = {}

def apply_transform(obj, spec):
    obj.location = tuple(spec.get("pos", (0, 0, 0)))
    q = spec.get("rotation", (0, 0, 0, 1))
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = (q[3], q[0], q[1], q[2])
    obj.scale = tuple(spec.get("scale", (1, 1, 1)))
    obj.hide_render = not bool(spec.get("visible", True))
    obj.hide_viewport = not bool(spec.get("visible", True))

def make_material(spec):
    m = spec.get("material", {})
    mat = bpy.data.materials.new(m.get("name") or spec.get("name", "MS Material"))
    mat.diffuse_color = tuple(m.get("base", (0.8, 0.8, 0.8, 1.0)))
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = mat.diffuse_color
        bsdf.inputs["Metallic"].default_value = float(m.get("metallic", 0.0))
        bsdf.inputs["Roughness"].default_value = float(m.get("roughness", 0.5))
        emission = float(m.get("emission", 0.0))
        if "Emission Strength" in bsdf.inputs:
            bsdf.inputs["Emission Strength"].default_value = emission
        texture = m.get("base_color_texture", "")
        if texture and Path(texture).is_file():
            image = bpy.data.images.load(str(Path(texture).resolve()), check_existing=True)
            tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
            tex.image = image
            mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return mat

def import_mesh(path):
    ext = path.suffix.lower()
    before = set(bpy.context.scene.objects)
    try:
        if ext in (".glb", ".gltf"):
            bpy.ops.import_scene.gltf(filepath=str(path))
        elif ext == ".fbx":
            bpy.ops.import_scene.fbx(filepath=str(path))
        elif ext == ".obj":
            if hasattr(bpy.ops.wm, "obj_import"):
                bpy.ops.wm.obj_import(filepath=str(path))
            else:
                bpy.ops.import_scene.obj(filepath=str(path))
        elif ext == ".stl":
            if hasattr(bpy.ops.wm, "stl_import"):
                bpy.ops.wm.stl_import(filepath=str(path))
            else:
                bpy.ops.import_mesh.stl(filepath=str(path))
        elif ext in (".usd", ".usda", ".usdc"):
            bpy.ops.wm.usd_import(filepath=str(path))
        else:
            print("MotionStudio: unsupported mesh format:", ext)
            return []
    except Exception as exc:
        print("MotionStudio: mesh import failed:", path, exc)
        return []
    return [obj for obj in bpy.context.scene.objects if obj not in before]

for spec in data.get("objects", []):
    obj_id = int(spec.get("id", 0))
    kind = int(spec.get("type", 0))
    name = spec.get("name", "MS Object")
    made = []
    if kind == 1:  # Camera
        camera_data = bpy.data.cameras.new(name)
        camera_data.lens = float(spec.get("focal_mm", 35.0))
        camera_data.sensor_width = float(spec.get("sensor_mm", 36.0))
        camera_data.clip_start = float(spec.get("near_clip", 0.1))
        camera_data.clip_end = float(spec.get("far_clip", 10000.0))
        obj = bpy.data.objects.new(name, camera_data)
        scene.collection.objects.link(obj)
        scene.camera = obj
        made = [obj]
    elif kind == 2:  # Light
        types = ("POINT", "SUN", "SPOT", "AREA")
        light_type = int(spec.get("light_type", 3))
        light_data = bpy.data.lights.new(name, types[max(0, min(3, light_type))])
        light_data.energy = max(0.0, float(spec.get("intensity", 1.0))) * 50.0
        color = spec.get("color", (1.0, 1.0, 1.0))
        light_data.color = tuple(color[:3])
        obj = bpy.data.objects.new(name, light_data)
        scene.collection.objects.link(obj)
        made = [obj]
    elif kind == 3:  # Mesh reference
        asset = spec.get("asset", "")
        asset_path = Path(asset)
        if asset and not asset_path.is_absolute():
            asset_path = (JOB / asset_path).resolve()
        if asset and asset_path.is_file():
            made = import_mesh(asset_path)
        if not made:
            bpy.ops.mesh.primitive_cube_add(size=1.0)
            made = [bpy.context.object]
            print("MotionStudio: using fallback cube for", name, "asset:", asset or "(none)")
        for obj in made:
            obj.name = name if len(made) == 1 else name + "_" + obj.name
            if obj.type == "MESH":
                mat = make_material(spec)
                if obj.data.materials:
                    obj.data.materials[0] = mat
                else:
                    obj.data.materials.append(mat)
    elif kind == 4:  # Volume placeholder
        bpy.ops.mesh.primitive_cube_add(size=1.0)
        volume_obj = bpy.context.object
        volume_obj.name = name + "_VolumeBounds"
        made = [volume_obj]
        print("MotionStudio: volume bounds exported; volume grid assignment requires a VDB asset")

    if made:
        for obj in made:
            apply_transform(obj, spec)
        created_by_id[obj_id] = made[0]

# Parent after all objects exist so forward references are supported.
for spec in data.get("objects", []):
    child = created_by_id.get(int(spec.get("id", 0)))
    parent = created_by_id.get(int(spec.get("parent", 0)))
    if child and parent and child != parent:
        child.parent = parent

# ACES/AgX-friendly default; Blender manages the installed OCIO configuration.
try:
    scene.view_settings.view_transform = "AgX"
except Exception:
    pass

print("MotionStudio Blender scene ready:", len(created_by_id), "objects")
bpy.ops.render.render(write_still=True)
print("MotionStudio Blender render complete:", scene.render.filepath)
)PY";
  return Error::success();
}

Error DccBridge::write_moonray(const Scene& scene, const std::string& job_dir, std::string& script) {
  fs::create_directories(job_dir);
  {
    std::ofstream j(job_dir + "/scene_job.json");
    j << scene.to_job_json();
  }
  // MoonRay uses RDL2; we emit a minimal rdla-style text + shell wrapper
  script = job_dir + "/ms_moonray_job.rdla";
  std::ofstream rdla(script);
  if (!rdla) return Error::make(ErrorCode::FilePermission, "cannot write rdla", script);

  rdla << "// MotionStudio → MoonRay job (Pass 6)\n";
  rdla << "// Generated from Scene::to_job_json — refine with full RDL bindings in production\n";
  rdla << "if (rdl2) {\n";
  for (const auto& o : scene.objects()) {
    rdla << "  // object " << o.name << " type=" << static_cast<int>(o.type)
         << " pos=[" << o.xform.position.x << "," << o.xform.position.y
         << "," << o.xform.position.z << "]\n";
  }
  rdla << "}\n";

  std::ofstream sh(job_dir + "/run_moonray.sh");
  sh << "#!/usr/bin/env bash\nset -euo pipefail\n"
     << "DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n"
     << "OUT=\"$DIR/render\"\nmkdir -p \"$OUT\"\n"
     << "SCENE_JSON=\"$DIR/scene_job.json\"\n"
     << "RDLA=\"$DIR/ms_moonray_job.rdla\"\n"
     << "echo \"MotionStudio MoonRay job: $SCENE_JSON\"\n"
     << "# Example (adjust to your MoonRay install):\n"
     << "# moonray -in \"$RDLA\" -out \"$OUT/beauty.exr\"\n"
     << "if command -v moonray >/dev/null 2>&1; then\n"
     << "  echo \"moonray found — wire full RDL scene for production renders\"\n"
     << "  # moonray -in \"$RDLA\" -out \"$OUT/beauty.exr\" || true\n"
     << "  exit 0\n"
     << "else\n"
     << "  echo \"moonray binary not on PATH\"\n"
     << "  exit 0\n"
     << "fi\n";
  return Error::success();
}

Error DccBridge::write_gaffer(const Scene& scene, const std::string& job_dir, std::string& script) {
  fs::create_directories(job_dir);
  {
    std::ofstream j(job_dir + "/scene_job.json");
    j << scene.to_job_json();
  }
  script = job_dir + "/ms_gaffer_job.py";
  std::ofstream py(script);
  if (!py) return Error::make(ErrorCode::FilePermission, "cannot write gaffer script", script);

  py <<
R"PY(# MotionStudio → Gaffer job (Pass 6)
# Run with: gaffer execute ms_gaffer_job.py
# or open in gaffer GUI for node graph edit

"""
Gaffer is a node-based VFX app (Image Engine).
MotionStudio exports a scene_job.json and this bootstrap script.
Production path: build a .gfr script with cameras/lights from JSON.
"""
import json
from pathlib import Path

JOB = Path(__file__).resolve().parent
data = json.loads((JOB / "scene_job.json").read_text())

print("MotionStudio Gaffer job")
print("  resolution:", data.get("width"), "x", data.get("height"))
print("  samples:", data.get("samples"))
print("  objects:", len(data.get("objects", [])))
for o in data.get("objects", []):
    print("   -", o.get("name"), "type", o.get("type"), "pos", o.get("pos"))

# When running inside Gaffer Python:
try:
    import Gaffer
    import GafferScene
    print("Gaffer modules available — extend with SceneReader / Camera / Lights nodes")
except ImportError:
    print("Gaffer Python API not in this interpreter (expected outside gaffer shell)")
)PY";

  std::ofstream sh(job_dir + "/run_gaffer.sh");
  sh << "#!/usr/bin/env bash\nset -euo pipefail\n"
     << "DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n"
     << "echo \"MotionStudio Gaffer job in $DIR\"\n"
     << "if command -v gaffer >/dev/null 2>&1; then\n"
     << "  # gaffer execute \"$DIR/ms_gaffer_job.py\" || gaffer \"$DIR/ms_gaffer_job.py\"\n"
     << "  echo \"gaffer found — open/execute script for full graph\"\n"
     << "  exit 0\n"
     << "else\n"
     << "  echo \"gaffer binary not on PATH\"\n"
     << "  python3 \"$DIR/ms_gaffer_job.py\" || true\n"
     << "  exit 0\n"
     << "fi\n";
  return Error::success();
}

namespace {
float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
std::string xml_escape(const std::string& in) {
  std::string out; out.reserve(in.size());
  for (char c : in) {
    switch (c) { case '&': out += "&amp;"; break; case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break; case '\"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break; default: out += c; }
  }
  return out;
}
void write_matrix(std::ostream& os, const Transform3D& t) {
  const double x=t.rotation.x, y=t.rotation.y, z=t.rotation.z, w=t.rotation.w;
  const double xx=x*x, yy=y*y, zz=z*z, xy=x*y, xz=x*z, yz=y*z, wx=w*x, wy=w*y, wz=w*z;
  const double m00=1-2*(yy+zz), m01=2*(xy-wz), m02=2*(xz+wy);
  const double m10=2*(xy+wz), m11=1-2*(xx+zz), m12=2*(yz-wx);
  const double m20=2*(xz-wy), m21=2*(yz+wx), m22=1-2*(xx+yy);
  os << std::setprecision(12)
     << m00*t.scale.x << ' ' << m01*t.scale.y << ' ' << m02*t.scale.z << ' ' << t.position.x << '\n'
     << m10*t.scale.x << ' ' << m11*t.scale.y << ' ' << m12*t.scale.z << ' ' << t.position.y << '\n'
     << m20*t.scale.x << ' ' << m21*t.scale.y << ' ' << m22*t.scale.z << ' ' << t.position.z << '\n'
     << "0 0 0 1\n";
}
}

Error DccBridge::write_appleseed(const Scene& scene, const std::string& job_dir, std::string& script) {
  fs::create_directories(job_dir);
  {
    std::ofstream json(job_dir + "/scene_job.json");
    if (!json) return Error::make(ErrorCode::FilePermission, "cannot write Appleseed scene manifest", job_dir);
    json << scene.to_job_json();
  }
  script = job_dir + "/ms_appleseed_job.appleseed";
  std::ofstream out(script);
  if (!out) return Error::make(ErrorCode::FilePermission, "cannot write Appleseed project", script);

  const int width = std::max(16, scene.render().width);
  const int height = std::max(16, scene.render().height);
  const int samples = std::max(1, scene.render().samples);
  const SceneObject* camera = nullptr;
  for (const auto& obj : scene.objects()) if (obj.type == SceneObjectType::Camera && obj.visible) { camera = &obj; break; }
  Transform3D camera_xform;
  if (camera) camera_xform = camera->xform;
  const float focal = camera ? camera->focal_mm : 35.f;

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<!-- Generated by Motion Studio. Appleseed is an optional external renderer. -->\n"
      << "<project format_revision=\"29\">\n  <scene>\n"
      << "    <camera name=\"ms_camera\" model=\"pinhole_camera\">\n"
      << "      <parameter name=\"film_dimensions\" value=\"0.036 " << (0.036 * height / width) << "\" />\n"
      << "      <parameter name=\"focal_length\" value=\"" << (focal / 1000.0) << "\" />\n"
      << "      <transform time=\"0\"><matrix>\n";
  write_matrix(out, camera_xform);
  out << "      </matrix></transform>\n    </camera>\n"
      << "    <color name=\"ms_environment_color\"><parameter name=\"color_space\" value=\"srgb\"/><values>0.12 0.14 0.20</values><alpha>1</alpha></color>\n"
      << "    <environment_edf name=\"ms_environment_edf\" model=\"constant_environment_edf\"><parameter name=\"radiance\" value=\"ms_environment_color\"/></environment_edf>\n"
      << "    <environment_shader name=\"ms_environment_shader\" model=\"edf_environment_shader\"><parameter name=\"environment_edf\" value=\"ms_environment_edf\"/></environment_shader>\n"
      << "    <environment name=\"ms_environment\" model=\"generic_environment\"><parameter name=\"environment_edf\" value=\"ms_environment_edf\"/><parameter name=\"environment_shader\" value=\"ms_environment_shader\"/></environment>\n"
      << "    <assembly name=\"ms_assembly\">\n"
      << "      <surface_shader name=\"ms_surface_shader\" model=\"physical_surface_shader\"/>\n"
      << "      <object name=\"ms_sphere\" model=\"example_sphere_object\"><parameter name=\"radius\" value=\"1\"/></object>\n";

  int mesh_index = 0, light_index = 0;
  for (const auto& obj : scene.objects()) {
    if (!obj.visible) continue;
    if (obj.type == SceneObjectType::MeshRef) {
      ++mesh_index;
      out << "      <!-- MS mesh: " << xml_escape(obj.name) << "; source asset: "
          << xml_escape(obj.asset_ref) << "; geometry proxy: unit sphere until mesh serialization is added. -->\n";
      out << "      <color name=\"ms_color_" << mesh_index << "\"><parameter name=\"color_space\" value=\"srgb\"/><values>"
          << clampf(obj.material.base_r,0.f,1.f) << ' ' << clampf(obj.material.base_g,0.f,1.f) << ' ' << clampf(obj.material.base_b,0.f,1.f)
          << "</values><alpha>1</alpha></color>\n"
          << "      <bsdf name=\"ms_bsdf_" << mesh_index << "\" model=\"lambertian_brdf\"><parameter name=\"reflectance\" value=\"ms_color_" << mesh_index << "\"/></bsdf>\n"
          << "      <material name=\"ms_material_" << mesh_index << "\" model=\"generic_material\"><parameter name=\"bsdf\" value=\"ms_bsdf_" << mesh_index << "\"/><parameter name=\"surface_shader\" value=\"ms_surface_shader\"/></material>\n"
          << "      <object_instance name=\"ms_instance_" << mesh_index << "\" object=\"ms_sphere\"><transform><matrix>\n";
      Transform3D xf = obj.xform;
      // The current Scene API stores a mesh asset reference, not mesh geometry. Emit a
      // clearly documented sphere proxy when geometry cannot be resolved by Appleseed.
      write_matrix(out, xf);
      out << "      </matrix></transform><assign_material slot=\"default\" side=\"front\" material=\"ms_material_" << mesh_index << "\"/></object_instance>\n";
    } else if (obj.type == SceneObjectType::Light) {
      ++light_index;
      out << "      <color name=\"ms_light_power_" << light_index << "\"><parameter name=\"color_space\" value=\"srgb\"/><parameter name=\"multiplier\" value=\""
          << std::max(0.f,obj.intensity) * 35.f << "\"/><values>"
          << clampf(obj.color_r,0.f,1.f) << ' ' << clampf(obj.color_g,0.f,1.f) << ' ' << clampf(obj.color_b,0.f,1.f)
          << "</values><alpha>1</alpha></color>\n"
          << "      <light name=\"ms_light_" << light_index << "\" model=\"point_light\"><parameter name=\"intensity\" value=\"ms_light_power_" << light_index << "\"/><transform><matrix>\n";
      write_matrix(out, obj.xform);
      out << "      </matrix></transform></light>\n";
    }
  }
  if (mesh_index == 0) {
    out << "      <color name=\"ms_default_color\"><parameter name=\"color_space\" value=\"srgb\"/><values>0.55 0.58 0.68</values><alpha>1</alpha></color>\n"
        << "      <bsdf name=\"ms_default_bsdf\" model=\"lambertian_brdf\"><parameter name=\"reflectance\" value=\"ms_default_color\"/></bsdf>\n"
        << "      <material name=\"ms_default_material\" model=\"generic_material\"><parameter name=\"bsdf\" value=\"ms_default_bsdf\"/><parameter name=\"surface_shader\" value=\"ms_surface_shader\"/></material>\n"
        << "      <object_instance name=\"ms_default_instance\" object=\"ms_sphere\"><assign_material slot=\"default\" side=\"front\" material=\"ms_default_material\"/></object_instance>\n";
  }
  out << "    </assembly>\n    <assembly_instance name=\"ms_assembly_inst\" assembly=\"ms_assembly\"/>\n"
      << "  </scene>\n  <output><frame name=\"beauty\"><parameter name=\"camera\" value=\"ms_camera\"/><parameter name=\"resolution\" value=\"" << width << ' ' << height << "\"/></frame></output>\n"
      << "  <configurations><configuration name=\"final\" base=\"base_final\"><parameters name=\"uniform_pixel_renderer\"><parameter name=\"samples\" value=\"" << samples << "\"/></parameters></configuration><configuration name=\"interactive\" base=\"base_interactive\"/></configurations>\n"
      << "</project>\n";
  if (!out.good()) return Error::make(ErrorCode::FilePermission, "failed writing Appleseed project", script);

  std::ofstream sh(job_dir + "/run_appleseed.sh");
  if (!sh) return Error::make(ErrorCode::FilePermission, "cannot write Appleseed launcher", job_dir);
  sh << "#!/usr/bin/env bash\nset -euo pipefail\n"
     << "DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n"
     << "mkdir -p \"$DIR/render\"\n"
     << "BIN=\"${APPLESEED_CLI:-appleseed.cli}\"\n";
  sh << "if ! command -v \"$BIN\" >/dev/null 2>&1; then echo \"appleseed.cli not found; project file is ready\"; exit 2; fi\n"
     << "exec \"$BIN\" --configuration final --output \"$DIR/render/beauty.exr\" \"$DIR/ms_appleseed_job.appleseed\"\n";
  return Error::success();
}

Error DccBridge::write_job(const Scene& scene, DccTarget target, const std::string& job_dir,
                           std::string& primary_script_out) {
  switch (target) {
    case DccTarget::Blender: return write_blender(scene, job_dir, primary_script_out);
    case DccTarget::MoonRay: return write_moonray(scene, job_dir, primary_script_out);
    case DccTarget::Gaffer: return write_gaffer(scene, job_dir, primary_script_out);
    case DccTarget::Appleseed: return write_appleseed(scene, job_dir, primary_script_out);
  }
  return Error::make(ErrorCode::InvalidArgument, "unknown DCC target");
}

int DccBridge::run_process(const std::string& cmd, std::string& log_out) {
  log_out.clear();
  std::string full = cmd + " 2>&1";
  FILE* pipe = popen(full.c_str(), "r");
  if (!pipe) { log_out = "popen failed"; return -1; }
  char buf[256];
  while (fgets(buf, sizeof(buf), pipe)) log_out += buf;
  int st = pclose(pipe);
  if (WIFEXITED(st)) return WEXITSTATUS(st);
  return -1;
}

DccJobResult DccBridge::submit(const Scene& scene, DccTarget target,
                               const std::string& job_dir, bool dry_run) {
  DccJobResult r;
  r.job_dir = job_dir;
  Error e = write_job(scene, target, job_dir, r.primary_script);
  if (!e.ok()) {
    r.message = e.message;
    return r;
  }

  switch (target) {
    case DccTarget::Blender:
      r.binary_found = has_blender();
      break;
    case DccTarget::MoonRay:
      r.binary_found = has_moonray();
      break;
    case DccTarget::Gaffer:
      r.binary_found = has_gaffer();
      break;
    case DccTarget::Appleseed:
      r.binary_found = has_appleseed();
      break;
  }

  if (dry_run || !r.binary_found) {
    r.launched = false;
    r.message = dry_run ? "dry_run: job files written"
                        : "binary not found: job files written only";
    r.exit_code = 0;
    return r;
  }

  std::string cmd;
  if (target == DccTarget::Blender) {
    cmd = paths_.blender + " -b -P \"" + r.primary_script + "\"";
  } else if (target == DccTarget::MoonRay) {
    cmd = "bash \"" + job_dir + "/run_moonray.sh\"";
  } else if (target == DccTarget::Gaffer) {
    cmd = "bash \"" + job_dir + "/run_gaffer.sh\"";
  } else {
    cmd = "bash \"" + job_dir + "/run_appleseed.sh\"";
  }
  r.launched = true;
  r.exit_code = run_process(cmd, r.log);
  r.message = "process finished";
  return r;
}

Error DccBridge::load_render_preview(const std::string& image_path, ImageBuffer& out) {
  // Minimal PPM loader for previews written by MS itself; PNG needs lib later
  std::ifstream f(image_path, std::ios::binary);
  if (!f) return Error::make(ErrorCode::FileNotFound, "preview", image_path);
  std::string magic;
  f >> magic;
  if (magic != "P6") return Error::make(ErrorCode::NotSupported, "only P6 PPM preview");
  int w = 0, h = 0, maxv = 0;
  f >> w >> h >> maxv;
  f.get();
  if (!out.allocate(w, h, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "preview");
  for (int i = 0; i < w * h; ++i) {
    char rgb[3];
    f.read(rgb, 3);
    uint8_t* p = out.data() + i * 4;
    p[0] = rgb[0]; p[1] = rgb[1]; p[2] = rgb[2]; p[3] = 255;
  }
  return Error::success();
}

} // namespace ms
