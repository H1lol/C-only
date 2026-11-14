#pragma once
#include "ms_error.hpp"
#include "ms_image.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <unordered_set>

namespace ms {

enum class NodeOp : uint8_t {
  Read = 0, Write, Merge, Blur, Grade, Transform, Constant, Viewer,
  Saturation, Invert, CompositeOver
};

struct NodeParam {
  std::string name;
  double value = 0.0;
};

struct GraphNode {
  uint64_t id = 0;
  std::string name;
  NodeOp op = NodeOp::Constant;
  bool enabled = true;
  std::vector<NodeParam> params;
  std::string media_path; // for Read
  float solid_r = 0, solid_g = 0, solid_b = 0, solid_a = 1;
};

struct GraphEdge {
  uint64_t from = 0;
  uint64_t to = 0;
  int from_out = 0;
  int to_in = 0;
};

class NodeGraph {
public:
  uint64_t add(GraphNode n);
  bool remove(uint64_t id);
  bool connect(uint64_t from, uint64_t to, int from_out = 0, int to_in = 0);
  bool disconnect(uint64_t from, uint64_t to, int to_in = 0);
  bool set_parameter(uint64_t node_id, const std::string& name, double value);
  Error validate() const;
  GraphNode* find(uint64_t id);
  const GraphNode* find(uint64_t id) const;
  const std::vector<GraphNode>& nodes() const { return nodes_; }
  const std::vector<GraphEdge>& edges() const { return edges_; }

  /** Evaluate graph ending at viewer_or_write node into output */
  Error evaluate(uint64_t sink_id, double time_sec, int w, int h, ImageBuffer& out);

private:
  std::vector<GraphNode> nodes_;
  std::vector<GraphEdge> edges_;
  uint64_t next_id_ = 1;

  Error eval_node(uint64_t id, double time, int w, int h,
                  std::unordered_map<uint64_t, ImageBuffer>& cache,
                  std::unordered_set<uint64_t>& visiting, ImageBuffer& out);
  std::vector<uint64_t> inputs_of(uint64_t id) const;
  int input_capacity(NodeOp op) const;
  bool reaches(uint64_t start, uint64_t target) const;
};

} // namespace ms
