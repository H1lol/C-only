#include "ms_node_graph.hpp"
#include <iostream>

static int fails = 0;
#define CHECK(x, name) do { if (!(x)) { std::cerr << "FAIL " << name << "\n"; ++fails; } else std::cout << "ok " << name << "\n"; } while (0)

int main() {
  using namespace ms;
  NodeGraph graph;
  GraphNode red; red.name="Red"; red.op=NodeOp::Constant; red.solid_r=1; red.solid_a=.5f;
  GraphNode blue; blue.name="Blue"; blue.op=NodeOp::Constant; blue.solid_b=1; blue.solid_a=1;
  GraphNode over; over.name="Over"; over.op=NodeOp::CompositeOver;
  const auto r=graph.add(red), b=graph.add(blue), o=graph.add(over);
  CHECK(r && b && o, "nodes receive stable IDs");
  CHECK(graph.connect(r,o,0,0), "foreground connects to port 0");
  CHECK(graph.connect(b,o,0,1), "background connects to port 1");
  CHECK(graph.validate().ok(), "valid DAG accepted");
  CHECK(!graph.connect(o,r,0,0), "cycle rejected at connect time");
  CHECK(!graph.connect(r,o,0,2), "invalid input port rejected");
  CHECK(graph.set_parameter(o,"mix",0.75), "node parameter can be updated");
  ImageBuffer out;
  CHECK(graph.evaluate(o,0,16,16,out).ok(), "graph evaluates to image");
  CHECK(out.valid() && out.width()==16 && out.height()==16, "output dimensions preserved");
  const auto* px=out.data();
  CHECK(px[0] >= 126 && px[0] <= 129 && px[2] >= 126 && px[2] <= 129 && px[3] == 255, "alpha-over compositing is correct");
  CHECK(graph.disconnect(r,o,0), "input can be disconnected");
  CHECK(graph.validate().ok(), "graph remains valid after disconnect");
  CHECK(graph.remove(b), "node removal succeeds");
  CHECK(graph.validate().ok(), "node removal cleans connected edges");
  std::cout << (fails ? "GRAPH SAFETY TEST FAILED\n" : "GRAPH SAFETY TEST PASSED\n");
  return fails ? 1 : 0;
}
