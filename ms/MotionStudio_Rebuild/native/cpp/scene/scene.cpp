#include "ms_scene.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace ms {

uint64_t Scene::add(SceneObject o) {
  if (o.id == 0) o.id = next_id_++;
  else next_id_ = std::max(next_id_, o.id + 1);
  objects_.push_back(std::move(o));
  return objects_.back().id;
}

bool Scene::remove(uint64_t id) {
  auto it = std::remove_if(objects_.begin(), objects_.end(),
                           [id](const SceneObject& o) { return o.id == id; });
  if (it == objects_.end()) return false;
  objects_.erase(it, objects_.end());
  return true;
}

SceneObject* Scene::find(uint64_t id) {
  for (auto& o : objects_) if (o.id == id) return &o;
  return nullptr;
}

namespace {
std::string json_escape(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (unsigned char c : value) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          const char hex[] = "0123456789abcdef";
          out += "\\u00";
          out += hex[(c >> 4) & 0xf];
          out += hex[c & 0xf];
        } else out += static_cast<char>(c);
    }
  }
  return out;
}
}

std::string Scene::to_job_json() const {
  std::ostringstream os;
  os << std::setprecision(9)
     << "{\n  \"backend\": \"" << json_escape(render_.backend) << "\",\n"
     << "  \"width\": " << render_.width << ",\n"
     << "  \"height\": " << render_.height << ",\n"
     << "  \"samples\": " << render_.samples << ",\n"
     << "  \"color_space\": \"" << json_escape(render_.color_space) << "\",\n"
     << "  \"output_path\": \"" << json_escape(render_.output_path) << "\",\n"
     << "  \"objects\": [\n";
  for (size_t i = 0; i < objects_.size(); ++i) {
    const auto& o = objects_[i];
    os << "    {\"id\": " << o.id
       << ", \"name\": \"" << json_escape(o.name) << "\""
       << ", \"type\": " << static_cast<int>(o.type)
       << ", \"parent\": " << o.parent
       << ", \"visible\": " << (o.visible ? "true" : "false")
       << ", \"pos\": [" << o.xform.position.x << "," << o.xform.position.y
       << "," << o.xform.position.z << "]"
       << ", \"rotation\": [" << o.xform.rotation.x << "," << o.xform.rotation.y
       << "," << o.xform.rotation.z << "," << o.xform.rotation.w << "]"
       << ", \"scale\": [" << o.xform.scale.x << "," << o.xform.scale.y
       << "," << o.xform.scale.z << "]";
    if (o.type == SceneObjectType::Camera) {
      os << ", \"focal_mm\": " << o.focal_mm
         << ", \"sensor_mm\": " << o.sensor_mm
         << ", \"near_clip\": " << o.near_clip
         << ", \"far_clip\": " << o.far_clip;
    }
    if (o.type == SceneObjectType::Light) {
      os << ", \"light_type\": " << static_cast<int>(o.light)
         << ", \"intensity\": " << o.intensity
         << ", \"color\": [" << o.color_r << "," << o.color_g << "," << o.color_b << "]";
    }
    if (!o.asset_ref.empty())
      os << ", \"asset\": \"" << json_escape(o.asset_ref) << "\"";
    if (o.type == SceneObjectType::MeshRef) {
      os << ", \"material\": {\"name\": \"" << json_escape(o.material.name)
         << "\", \"base\": [" << o.material.base_r << "," << o.material.base_g
         << "," << o.material.base_b << "," << o.material.base_a
         << "], \"metallic\": " << o.material.metallic
         << ", \"roughness\": " << o.material.roughness
         << ", \"emission\": " << o.material.emission
         << ", \"base_color_texture\": \"" << json_escape(o.material.base_color_texture) << "\"}";
    }
    os << "}";
    if (i + 1 < objects_.size()) os << ",";
    os << "\n";
  }
  os << "  ]\n}\n";
  return os.str();
}

} // namespace ms
