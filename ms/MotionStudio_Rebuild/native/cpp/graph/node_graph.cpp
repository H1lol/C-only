#include "ms_node_graph.hpp"
#include "ms_cpu_effects.hpp"
#include "ms_cpu_effects_extra.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace ms {

uint64_t NodeGraph::add(GraphNode n) {
  if (n.id == 0) n.id = next_id_++;
  else next_id_ = std::max(next_id_, n.id + 1);
  if (find(n.id)) return 0;
  nodes_.push_back(std::move(n));
  return nodes_.back().id;
}

bool NodeGraph::remove(uint64_t id) {
  auto it = std::remove_if(nodes_.begin(), nodes_.end(), [id](const GraphNode& n) { return n.id == id; });
  if (it == nodes_.end()) return false;
  nodes_.erase(it, nodes_.end());
  edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
               [id](const GraphEdge& e) { return e.from == id || e.to == id; }), edges_.end());
  return true;
}

int NodeGraph::input_capacity(NodeOp op) const {
  switch (op) {
    case NodeOp::Read: case NodeOp::Constant: return 0;
    case NodeOp::Merge: case NodeOp::CompositeOver: return 2;
    default: return 1;
  }
}

bool NodeGraph::reaches(uint64_t start, uint64_t target) const {
  std::vector<uint64_t> stack{start};
  std::unordered_set<uint64_t> seen;
  while (!stack.empty()) {
    const auto current = stack.back(); stack.pop_back();
    if (current == target) return true;
    if (!seen.insert(current).second) continue;
    for (const auto& e : edges_) if (e.from == current) stack.push_back(e.to);
  }
  return false;
}

bool NodeGraph::connect(uint64_t from, uint64_t to, int from_out, int to_in) {
  const auto* src = find(from); const auto* dst = find(to);
  if (!src || !dst || from == to || from_out != 0 || to_in < 0 || to_in >= input_capacity(dst->op)) return false;
  if (reaches(to, from)) return false; // reject cycles before they can reach the evaluator
  for (auto& e : edges_) {
    if (e.to == to && e.to_in == to_in) {
      if (e.from == from && e.from_out == from_out) return true;
      e.from = from; e.from_out = from_out; // one source per input port
      return true;
    }
  }
  edges_.push_back({from, to, from_out, to_in});
  return true;
}

bool NodeGraph::disconnect(uint64_t from, uint64_t to, int to_in) {
  const auto old = edges_.size();
  edges_.erase(std::remove_if(edges_.begin(), edges_.end(), [&](const GraphEdge& e) {
    return e.from == from && e.to == to && e.to_in == to_in;
  }), edges_.end());
  return old != edges_.size();
}

bool NodeGraph::set_parameter(uint64_t node_id, const std::string& name, double value) {
  auto* n = find(node_id);
  if (!n || name.empty() || !std::isfinite(value)) return false;
  for (auto& p : n->params) if (p.name == name) { p.value = value; return true; }
  n->params.push_back({name, value});
  return true;
}

GraphNode* NodeGraph::find(uint64_t id) {
  for (auto& n : nodes_) if (n.id == id) return &n;
  return nullptr;
}
const GraphNode* NodeGraph::find(uint64_t id) const {
  for (const auto& n : nodes_) if (n.id == id) return &n;
  return nullptr;
}

std::vector<uint64_t> NodeGraph::inputs_of(uint64_t id) const {
  std::vector<GraphEdge> edges;
  for (const auto& e : edges_) if (e.to == id) edges.push_back(e);
  std::sort(edges.begin(), edges.end(), [](const GraphEdge& a, const GraphEdge& b) { return a.to_in < b.to_in; });
  std::vector<uint64_t> in;
  for (const auto& e : edges) in.push_back(e.from);
  return in;
}

Error NodeGraph::validate() const {
  std::set<uint64_t> ids;
  for (const auto& n : nodes_) {
    if (n.id == 0 || !ids.insert(n.id).second)
      return Error::make(ErrorCode::InvalidArgument, "node graph contains duplicate or zero node id");
  }
  std::set<std::pair<uint64_t, int>> ports;
  for (const auto& e : edges_) {
    const auto* src = find(e.from); const auto* dst = find(e.to);
    if (!src || !dst) return Error::make(ErrorCode::InvalidArgument, "node graph edge references missing node");
    if (e.from == e.to || e.from_out != 0 || e.to_in < 0 || e.to_in >= input_capacity(dst->op))
      return Error::make(ErrorCode::InvalidArgument, "node graph edge uses invalid port or self-link");
    if (!ports.insert({e.to, e.to_in}).second)
      return Error::make(ErrorCode::InvalidArgument, "node graph input port has multiple connections");
  }
  // Kahn's algorithm catches cycles without recursion or stack-depth risk.
  std::unordered_map<uint64_t, int> indegree;
  for (const auto& n : nodes_) indegree[n.id] = 0;
  for (const auto& e : edges_) ++indegree[e.to];
  std::vector<uint64_t> ready;
  for (const auto& kv : indegree) if (kv.second == 0) ready.push_back(kv.first);
  std::size_t visited = 0;
  while (!ready.empty()) {
    auto id = ready.back(); ready.pop_back(); ++visited;
    for (const auto& e : edges_) if (e.from == id && --indegree[e.to] == 0) ready.push_back(e.to);
  }
  if (visited != nodes_.size()) return Error::make(ErrorCode::InvalidArgument, "node graph contains a cycle");
  return Error::success();
}

Error NodeGraph::eval_node(uint64_t id, double time, int w, int h,
                           std::unordered_map<uint64_t, ImageBuffer>& cache,
                           std::unordered_set<uint64_t>& visiting, ImageBuffer& out) {
  (void)time;
  if (cache.count(id)) { out.copy_from(cache[id]); return Error::success(); }
  if (!visiting.insert(id).second) return Error::make(ErrorCode::InvalidArgument, "cycle detected during node evaluation");
  auto finish = [&](Error e) {
    if (e.ok()) cache[id].copy_from(out);
    visiting.erase(id);
    return e;
  };
  GraphNode* n = find(id);
  if (!n) return finish(Error::make(ErrorCode::InvalidArgument, "missing node"));
  if (!n->enabled) {
    out.allocate(w, h, PixelFormat::RGBA8); out.clear(0, 0, 0, 0);
    return finish(Error::success());
  }
  if (w <= 0 || h <= 0) return finish(Error::make(ErrorCode::InvalidArgument, "invalid graph output size"));

  auto param = [&](const char* name, double def) {
    for (const auto& p : n->params) if (p.name == name) return p.value;
    return def;
  };
  std::array<uint64_t, 2> input_ids{0, 0};
  for (const auto& e : edges_) if (e.to == id && e.to_in >= 0 && e.to_in < 2) input_ids[static_cast<std::size_t>(e.to_in)] = e.from;
  ImageBuffer a, b;
  if (input_ids[0]) { Error e = eval_node(input_ids[0], time, w, h, cache, visiting, a); if (!e.ok()) return finish(e); }
  if (input_ids[1]) { Error e = eval_node(input_ids[1], time, w, h, cache, visiting, b); if (!e.ok()) return finish(e); }

  Error result = Error::success();
  switch (n->op) {
    case NodeOp::Constant:
      result = cpu_solid(out, w, h, n->solid_r, n->solid_g, n->solid_b, n->solid_a); break;
    case NodeOp::Read:
      // Decode comes from the engine through the provider. Without one the node
      // stays an explicit slate rather than reading uninitialised memory.
      if (read_provider_) {
        result = read_provider_(*n, time, w, h, out);
        if (result.ok() && !out.valid())
          result = Error::make(ErrorCode::MediaDecodeFailed, "read node produced no frame");
      } else {
        result = cpu_solid(out, w, h, n->solid_r, n->solid_g, n->solid_b, n->solid_a);
      }
      break;
    case NodeOp::Blur: {
      if (!a.valid()) cpu_solid(a, w, h, 0.2f, 0.2f, 0.25f, 1.f);
      result = cpu_gaussian_blur(a, out, static_cast<float>(std::max(0.0, param("radius", 4.0)))); break;
    }
    case NodeOp::Grade: {
      if (!a.valid()) cpu_solid(a, w, h, 0.5f, 0.5f, 0.5f, 1.f);
      result = cpu_brightness_contrast(a, out, static_cast<float>(param("exposure", 0.0) * 0.1),
                                       static_cast<float>(param("contrast", 1.0))); break;
    }
    case NodeOp::Saturation: {
      if (!a.valid()) cpu_solid(a, w, h, 0.5f, 0.5f, 0.5f, 1.f);
      if (a.format() != PixelFormat::RGBA8) { result = Error::make(ErrorCode::NotSupported, "saturation node requires RGBA8 input"); break; }
      result = out.allocate(w, h, PixelFormat::RGBA8) ? Error::success() : Error::make(ErrorCode::OutOfMemory, "saturation output");
      if (!result.ok()) break;
      const float sat = static_cast<float>(std::clamp(param("amount", 1.0), 0.0, 4.0));
      const uint8_t* src = a.data(); uint8_t* dst = out.data();
      for (int i = 0; i < w * h; ++i) {
        const float r = src[0] / 255.f, g = src[1] / 255.f, bl = src[2] / 255.f;
        const float l = 0.2126f*r + 0.7152f*g + 0.0722f*bl;
        dst[0] = static_cast<uint8_t>(std::clamp(l + (r-l)*sat, 0.f, 1.f)*255.f + .5f);
        dst[1] = static_cast<uint8_t>(std::clamp(l + (g-l)*sat, 0.f, 1.f)*255.f + .5f);
        dst[2] = static_cast<uint8_t>(std::clamp(l + (bl-l)*sat, 0.f, 1.f)*255.f + .5f);
        dst[3] = src[3]; src += 4; dst += 4;
      }
      break;
    }
    case NodeOp::Invert:
      if (!a.valid()) cpu_solid(a, w, h, 0.5f, 0.5f, 0.5f, 1.f);
      result = cpu_invert(a, out); break;
    case NodeOp::Merge: {
      if (!a.valid() && b.valid()) { out.copy_from(b); break; }
      if (!a.valid()) cpu_solid(a, w, h, 0, 0, 0, 1);
      if (!b.valid()) { out.copy_from(a); break; }
      if (a.format() != PixelFormat::RGBA8 || b.format() != PixelFormat::RGBA8) { result = Error::make(ErrorCode::NotSupported, "merge requires RGBA8 inputs"); break; }
      if (!out.allocate(w, h, PixelFormat::RGBA8)) { result = Error::make(ErrorCode::OutOfMemory, "merge output"); break; }
      const uint8_t* A = a.data(); const uint8_t* B = b.data(); uint8_t* D = out.data();
      const float mix = static_cast<float>(std::clamp(param("mix", 0.5), 0.0, 1.0));
      for (int i = 0; i < w*h; ++i) { for (int c=0;c<4;++c) D[c]=static_cast<uint8_t>(std::clamp((A[c]*(1.f-mix)+B[c]*mix),0.f,255.f)+.5f); A+=4; B+=4; D+=4; }
      break;
    }
    case NodeOp::CompositeOver: {
      if (!a.valid()) { if (b.valid()) out.copy_from(b); else cpu_solid(out,w,h,0,0,0,0); break; }
      if (!b.valid()) { out.copy_from(a); break; }
      if (a.format()!=PixelFormat::RGBA8 || b.format()!=PixelFormat::RGBA8) { result=Error::make(ErrorCode::NotSupported,"over requires RGBA8 inputs"); break; }
      if (!out.allocate(w,h,PixelFormat::RGBA8)) { result=Error::make(ErrorCode::OutOfMemory,"over output"); break; }
      const uint8_t* F=a.data(); const uint8_t* B=b.data(); uint8_t* D=out.data();
      for (int i=0;i<w*h;++i) {
        const float af=F[3]/255.f, ab=B[3]/255.f, oa=af+ab*(1.f-af);
        for(int c=0;c<3;++c){ const float premul=(F[c]/255.f)*af+(B[c]/255.f)*ab*(1.f-af); D[c]=static_cast<uint8_t>(std::clamp(oa>1e-8f?premul/oa:0.f,0.f,1.f)*255.f+.5f); }
        D[3]=static_cast<uint8_t>(std::clamp(oa,0.f,1.f)*255.f+.5f); F+=4; B+=4; D+=4;
      }
      break;
    }
    case NodeOp::Transform: case NodeOp::Viewer: case NodeOp::Write:
      if (a.valid()) out.copy_from(a); else result = cpu_solid(out, w, h, 0.1f, 0.1f, 0.12f, 1.f); break;
  }
  return finish(result);
}

Error NodeGraph::evaluate(uint64_t sink_id, double time_sec, int w, int h, ImageBuffer& out) {
  if (sink_id == 0 || !find(sink_id)) return Error::make(ErrorCode::InvalidArgument, "missing graph sink");
  Error valid = validate(); if (!valid.ok()) return valid;
  std::unordered_map<uint64_t, ImageBuffer> cache;
  std::unordered_set<uint64_t> visiting;
  return eval_node(sink_id, time_sec, w, h, cache, visiting, out);
}


const char* graph_op_name(NodeOp op) {
  switch (op) {
    case NodeOp::Read: return "read";
    case NodeOp::Write: return "write";
    case NodeOp::Merge: return "merge";
    case NodeOp::Blur: return "blur";
    case NodeOp::Grade: return "grade";
    case NodeOp::Transform: return "transform";
    case NodeOp::Constant: return "constant";
    case NodeOp::Viewer: return "viewer";
    case NodeOp::Saturation: return "saturation";
    case NodeOp::Invert: return "invert";
    case NodeOp::CompositeOver: return "over";
  }
  return "constant";
}

bool graph_op_from_name(const std::string& name, NodeOp& out) {
  static const NodeOp all[] = {NodeOp::Read, NodeOp::Write, NodeOp::Merge, NodeOp::Blur,
                               NodeOp::Grade, NodeOp::Transform, NodeOp::Constant,
                               NodeOp::Viewer, NodeOp::Saturation, NodeOp::Invert,
                               NodeOp::CompositeOver};
  for (NodeOp op : all) {
    if (name == graph_op_name(op)) { out = op; return true; }
  }
  return false;
}

uint64_t NodeGraph::sink_id() const {
  for (const auto& n : nodes_) {
    if (n.op == NodeOp::Viewer || n.op == NodeOp::Write) return n.id;
  }
  return 0;
}

std::string NodeGraph::serialize() const {
  std::string s = "MSCOMP1";
  auto clean = [](const std::string& in) {
    std::string o;
    for (char c : in) o.push_back(c == '~' || c == '|' || c == ':' || c == ',' ? '_' : c);
    return o;
  };
  for (const auto& n : nodes_) {
    s += "~N|";
    s += std::to_string(n.id);
    s += "|";
    s += graph_op_name(n.op);
    s += "|";
    s += n.enabled ? "1" : "0";
    s += "|" + std::to_string(n.x) + "|" + std::to_string(n.y);
    s += "|" + std::to_string(n.solid_r) + "|" + std::to_string(n.solid_g);
    s += "|" + std::to_string(n.solid_b) + "|" + std::to_string(n.solid_a);
    s += "|" + clean(n.media_path);
    s += "|" + clean(n.name);
    s += "|";
    bool first = true;
    for (const auto& p : n.params) {
      if (!first) s += ",";
      first = false;
      s += clean(p.name) + ":" + std::to_string(p.value);
    }
  }
  for (const auto& e : edges_) {
    s += "~E|" + std::to_string(e.from) + "|" + std::to_string(e.to) + "|" +
         std::to_string(e.from_out) + "|" + std::to_string(e.to_in);
  }
  return s;
}

bool NodeGraph::deserialize(const std::string& text) {
  if (text.rfind("MSCOMP1", 0) != 0) return false;
  std::vector<GraphNode> nodes;
  std::vector<GraphEdge> edges;
  uint64_t max_id = 0;

  auto split = [](const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
      if (c == sep) { out.push_back(cur); cur.clear(); }
      else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
  };

  // Fields are sanitised of '~' and '|' when written, so a plain split is exact
  // and cannot lose a record the way an offset walk can.
  const std::vector<std::string> records = split(text, '~');
  for (size_t i = 1; i < records.size(); ++i) {
    const std::string& record = records[i];
    if (record.empty()) continue;
    const std::vector<std::string> f = split(record, '|');
    if (f.empty()) continue;
    if (f[0] == "N" && f.size() >= 12) {
      GraphNode n;
      n.id = std::strtoull(f[1].c_str(), nullptr, 10);
      graph_op_from_name(f[2], n.op);
      n.enabled = f[3] == "1";
      n.x = std::strtof(f[4].c_str(), nullptr);
      n.y = std::strtof(f[5].c_str(), nullptr);
      n.solid_r = std::strtof(f[6].c_str(), nullptr);
      n.solid_g = std::strtof(f[7].c_str(), nullptr);
      n.solid_b = std::strtof(f[8].c_str(), nullptr);
      n.solid_a = std::strtof(f[9].c_str(), nullptr);
      n.media_path = f[10];
      n.name = f[11];
      if (f.size() > 12 && !f[12].empty()) {
        for (const auto& pair : split(f[12], ',')) {
          const size_t colon = pair.find(':');
          if (colon == std::string::npos) continue;
          n.params.push_back({pair.substr(0, colon), std::strtod(pair.c_str() + colon + 1, nullptr)});
        }
      }
      max_id = std::max(max_id, n.id);
      nodes.push_back(std::move(n));
    } else if (f[0] == "E" && f.size() >= 5) {
      GraphEdge e;
      e.from = std::strtoull(f[1].c_str(), nullptr, 10);
      e.to = std::strtoull(f[2].c_str(), nullptr, 10);
      e.from_out = std::atoi(f[3].c_str());
      e.to_in = std::atoi(f[4].c_str());
      edges.push_back(e);
    }
  }

  if (nodes.empty()) return false;
  nodes_ = std::move(nodes);
  edges_ = std::move(edges);
  next_id_ = max_id + 1;
  return validate().ok();
}

} // namespace ms
