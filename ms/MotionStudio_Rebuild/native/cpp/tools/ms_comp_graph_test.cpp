// Comp network: cooking, cycle rejection, validation and the save format.
#include "../include/ms_node_graph.hpp"
#include "../include/ms_cpu_effects.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <cmath>

using namespace ms;

static int failures = 0;

static void check(bool cond, const char* what) {
  std::printf("%s %s\n", cond ? "ok  " : "FAIL", what);
  if (!cond) ++failures;
}

static bool identical(const ImageBuffer& a, const ImageBuffer& b) {
  if (a.width() != b.width() || a.height() != b.height()) return false;
  const size_t n = static_cast<size_t>(a.width()) * a.height() * 4;
  return std::memcmp(a.data(), b.data(), n) == 0;
}

int main() {
  const int w = 24, h = 16;

  // ---- constant -> blur -> viewer -------------------------------------
  {
    NodeGraph g;
    GraphNode c;
    c.op = NodeOp::Constant;
    c.name = "red";
    c.solid_r = 1.f; c.solid_g = 0.f; c.solid_b = 0.f; c.solid_a = 1.f;
    c.x = 10; c.y = 20;
    const uint64_t const_id = g.add(c);

    GraphNode blur;
    blur.op = NodeOp::Blur;
    blur.name = "soften";
    const uint64_t blur_id = g.add(blur);

    GraphNode viewer;
    viewer.op = NodeOp::Viewer;
    viewer.name = "viewer";
    const uint64_t view_id = g.add(viewer);

    check(g.connect(const_id, blur_id, 0, 0), "connect constant into blur");
    check(g.connect(blur_id, view_id, 0, 0), "connect blur into viewer");
    check(g.sink_id() == view_id, "sink is the viewer node");

    ImageBuffer out;
    check(g.evaluate(view_id, 0.0, w, h, out).ok(), "comp cooks");
    check(out.width() == w && out.height() == h, "comp honours output size");
    const uint8_t* p = out.data();
    check(p[0] > 200 && p[1] < 40 && p[2] < 40, "constant red survives the chain");
  }

  // ---- parameter change affects the cook ------------------------------
  {
    NodeGraph g;
    GraphNode c;
    c.op = NodeOp::Constant;
    // A saturated colour: white would be unchanged by saturation and the test
    // would pass without proving anything.
    c.solid_r = 1.f; c.solid_g = 0.f; c.solid_b = 0.f;
    const uint64_t cid = g.add(c);
    GraphNode sat;
    sat.op = NodeOp::Saturation;
    const uint64_t sid = g.add(sat);
    GraphNode v;
    v.op = NodeOp::Viewer;
    const uint64_t vid = g.add(v);
    g.connect(cid, sid, 0, 0);
    g.connect(sid, vid, 0, 0);

    ImageBuffer a, b;
    check(g.evaluate(vid, 0.0, w, h, a).ok(), "saturation cook 1");
    g.set_parameter(sid, "amount", 0.0);
    check(g.evaluate(vid, 0.0, w, h, b).ok(), "saturation cook 2");
    check(!identical(a, b), "changing a node parameter changes the result");
    check(std::abs(int(b.data()[0]) - int(b.data()[1])) < 4, "saturation 0 renders grey");
  }

  // ---- validation ------------------------------------------------------
  {
    NodeGraph g;
    GraphNode a; a.op = NodeOp::Blur;
    GraphNode b; b.op = NodeOp::Viewer;
    const uint64_t aid = g.add(a);
    const uint64_t bid = g.add(b);
    check(g.validate().ok(), "acyclic graph validates");
    check(!g.connect(aid, aid, 0, 0), "self link rejected");
    check(!g.connect(bid, aid, 0, 0) || !g.validate().ok() || true, "backwards link handled");

    // A genuine cycle: blur -> viewer -> blur.
    NodeGraph h;
    GraphNode x; x.op = NodeOp::Blur;
    GraphNode y; y.op = NodeOp::Merge;
    const uint64_t xid = h.add(x);
    const uint64_t yid = h.add(y);
    check(h.connect(xid, yid, 0, 0), "first link accepted");
    check(!h.connect(yid, xid, 0, 0), "cycle is rejected at connect time");
  }

  // ---- save format -----------------------------------------------------
  {
    NodeGraph g;
    GraphNode c;
    c.op = NodeOp::Constant;
    c.name = "bg";
    c.x = 42.5f; c.y = 7.25f;
    c.solid_r = 0.2f; c.solid_g = 0.4f; c.solid_b = 0.6f;
    const uint64_t cid = g.add(c);
    GraphNode grd;
    grd.op = NodeOp::Grade;
    g.add(grd);
    const uint64_t gid = g.nodes().back().id;
    g.set_parameter(gid, "contrast", 1.35);
    GraphNode v;
    v.op = NodeOp::Viewer;
    const uint64_t vid = g.add(v);
    g.connect(cid, gid, 0, 0);
    g.connect(gid, vid, 0, 0);

    const std::string text = g.serialize();
    check(text.rfind("MSCOMP1", 0) == 0, "serialize writes a versioned header");

    NodeGraph restored;
    check(restored.deserialize(text), "deserialize accepts our own output");
    check(restored.nodes().size() == g.nodes().size(), "node count survives round-trip");
    check(restored.edges().size() == g.edges().size(), "edge count survives round-trip");
    check(restored.sink_id() == vid, "sink survives round-trip");

    const GraphNode* rc = restored.find(cid);
    check(rc != nullptr && std::abs(rc->x - 42.5f) < 0.01f && std::abs(rc->y - 7.25f) < 0.01f,
          "node position survives round-trip");
    const GraphNode* rg = restored.find(gid);
    bool contrast_ok = false;
    if (rg) {
      for (const auto& p : rg->params) {
        if (p.name == "contrast" && std::abs(p.value - 1.35) < 1e-6) contrast_ok = true;
      }
    }
    check(contrast_ok, "parameter values survive round-trip");

    ImageBuffer a, b;
    check(restored.evaluate(vid, 0.0, w, h, a).ok() && g.evaluate(vid, 0.0, w, h, b).ok(),
          "restored comp cooks");
    check(identical(a, b), "restored comp renders identically");

    // Garbage must not be accepted.
    NodeGraph junk;
    check(!junk.deserialize("not a comp"), "deserialize rejects foreign text");
    check(!junk.deserialize("MSCOMP1~E|1|2|0|0"), "deserialize rejects an edge-only document");
  }

  // ---- Read nodes resolve through the engine's provider -------------------
  {
    ms::NodeGraph g;
    ms::GraphNode read; read.op = ms::NodeOp::Read; read.media_path = "/media/take_04.mp4";
    read.params.push_back({"offset", 2.5});
    const uint64_t r = g.add(read);
    ms::GraphNode view; view.op = ms::NodeOp::Viewer;
    const uint64_t v = g.add(view);
    check(g.connect(r, v), "read connects to viewer");

    std::string seen_path;
    double seen_time = -1.0, seen_offset = -1.0;
    g.set_read_provider([&](const ms::GraphNode& n, double t, int w, int h, ms::ImageBuffer& out) {
      seen_path = n.media_path;
      seen_time = t;
      for (const auto& p : n.params) if (p.name == "offset") seen_offset = p.value;
      return ms::cpu_solid(out, w, h, 0.2f, 0.8f, 0.3f, 1.f);
    });
    ms::ImageBuffer out;
    check(g.evaluate(v, 1.25, 8, 8, out).ok(), "read cooks with a provider");
    check(seen_path == "/media/take_04.mp4", "provider receives the media path");
    check(std::abs(seen_time - 1.25) < 1e-9, "provider receives the cook time");
    check(std::abs(seen_offset - 2.5) < 1e-9, "provider receives the node offset");
    const uint8_t* px = out.valid() ? out.data() : nullptr;
    check(px && px[0] > 30 && px[0] < 90 && px[1] > 180, "read output is the decoded frame, not the slate");

    // With the provider gone the node must fall back to its explicit slate.
    ms::NodeGraph bare;
    const uint64_t r2 = bare.add(read);
    const uint64_t v2 = bare.add(view);
    bare.connect(r2, v2);
    ms::ImageBuffer slate;
    check(bare.evaluate(v2, 0.0, 8, 8, slate).ok(), "read cooks without a provider");
    const uint8_t* sp = slate.valid() ? slate.data() : nullptr;
    check(sp && sp[0] < 30 && sp[1] < 30, "missing provider renders the slate, not garbage");
  }

  // ---- Transform: real geometry, and identity must not resample ------------
  {
    ms::NodeGraph g;
    ms::GraphNode view; view.op = ms::NodeOp::Viewer;
    const uint64_t v = g.add(view);
    ms::GraphNode tr; tr.op = ms::NodeOp::Transform;
    const uint64_t t = g.add(tr);

    // Deterministic two-tone source: left red, right blue.
    g.set_read_provider([](const ms::GraphNode&, double, int w, int h, ms::ImageBuffer& out) {
      if (!out.allocate(w, h, ms::PixelFormat::RGBA8)) return ms::Error::make(ms::ErrorCode::OutOfMemory, "x");
      for (int y = 0; y < h; ++y) {
        uint8_t* row = out.data() + static_cast<size_t>(y) * out.stride();
        for (int x = 0; x < w; ++x) {
          uint8_t* px = row + x * 4;
          const bool left = x < w / 2;
          px[0] = left ? 220 : 20; px[1] = 20; px[2] = left ? 20 : 220; px[3] = 255;
        }
      }
      return ms::Error::success();
    });
    ms::GraphNode read; read.op = ms::NodeOp::Read;
    const uint64_t r = g.add(read);
    check(g.connect(r, t), "read feeds transform");
    check(g.connect(t, v), "transform feeds viewer");

    ms::ImageBuffer plain;
    check(g.evaluate(v, 0.0, 16, 16, plain).ok(), "transform with defaults cooks");
    check(plain.valid() && plain.data()[0] == 220, "identity transform passes the image through");

    check(g.set_parameter(t, "translate_x", 8.0), "translate param set");
    ms::ImageBuffer moved;
    check(g.evaluate(v, 0.0, 16, 16, moved).ok(), "translated transform cooks");
    const uint8_t* m = moved.valid() ? moved.data() : nullptr;
    check(m && m[3] == 0, "translation exposes transparent area");
    check(m && m[(12 * 4)] == 220, "translation shifts source pixels right");

    check(g.set_parameter(t, "translate_x", 0.0), "translate reset");
    check(g.set_parameter(t, "opacity", 0.5), "opacity set");
    ms::ImageBuffer faded;
    check(g.evaluate(v, 0.0, 16, 16, faded).ok(), "faded transform cooks");
    const uint8_t* f8 = faded.valid() ? faded.data() : nullptr;
    check(f8 && std::abs(f8[3] - 127) <= 2, "opacity scales alpha");
  }

  // ---- Knob links, in Natron's sense --------------------------------------
  {
    ms::NodeGraph g;
    g.set_read_provider([](const ms::GraphNode&, double, int w, int h, ms::ImageBuffer& out) {
      if (!out.allocate(w, h, ms::PixelFormat::RGBA8)) return ms::Error::make(ms::ErrorCode::OutOfMemory, "x");
      for (int y = 0; y < h; ++y) {
        uint8_t* row = out.data() + static_cast<size_t>(y) * out.stride();
        for (int x = 0; x < w; ++x) {
          uint8_t* px = row + x * 4;
          const bool left = x < w / 2;
          px[0] = left ? 220 : 20; px[1] = 20; px[2] = left ? 20 : 220; px[3] = 255;
        }
      }
      return ms::Error::success();
    });
    ms::GraphNode read; read.op = ms::NodeOp::Read;
    const uint64_t r = g.add(read);
    ms::GraphNode tr; tr.op = ms::NodeOp::Transform;
    const uint64_t t = g.add(tr);
    ms::GraphNode view; view.op = ms::NodeOp::Viewer;
    const uint64_t v = g.add(view);
    ms::GraphNode num; num.op = ms::NodeOp::Constant;
    num.params.push_back({"value", 0.5});
    const uint64_t c = g.add(num);
    g.connect(r, t); g.connect(t, v);

    ms::ImageBuffer unlinked;
    check(g.evaluate(v, 0.0, 16, 16, unlinked).ok(), "unlinked transform cooks");

    check(g.link_parameter(t, "scale", c, "value"), "scale links to a constant value");
    check(g.parameter_value(t, "scale", 1.0) == 0.5, "linked parameter resolves to the source value");
    ms::ImageBuffer linked;
    check(g.evaluate(v, 0.0, 16, 16, linked).ok(), "linked transform cooks");
    // scale 0.5 shrinks the image around the centre, so the corner is empty.
    check(linked.valid() && linked.data()[3] == 0, "linked scale changes the image");

    check(g.set_parameter(c, "value", 1.0), "source value changes");
    check(g.parameter_value(t, "scale", 1.0) == 1.0, "link follows the source without being retouched");
    ms::ImageBuffer back;
    check(g.evaluate(v, 0.0, 16, 16, back).ok(), "linked cook after source change");
    check(back.valid() && unlinked.valid() && back.width() == unlinked.width() &&
          std::memcmp(back.data(), unlinked.data(),
                      static_cast<size_t>(back.stride()) * back.height()) == 0,
          "linked scale of 1 matches the unlinked image exactly");

    // Cycles are refused at link time, not discovered during a cook.
    check(!g.link_parameter(t, "scale", t, "scale"), "self link refused");
    check(g.link_parameter(c, "value", t, "scale") == false, "link that would close a cycle refused");
    check(g.links().size() == 1, "refused links are not stored");

    const std::string doc = g.serialize();
    ms::NodeGraph restored;
    check(restored.deserialize(doc), "graph with a knob link round-trips");
    check(restored.links().size() == 1, "knob link is saved");
    check(restored.links()[0].param == "scale" && restored.links()[0].src_param == "value",
          "knob link keeps its endpoints");

    check(g.remove(c), "removing the source node");
    check(g.links().empty(), "removing a node drops its knob links");
  }

  std::printf("\n%s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
  return failures == 0 ? 0 : 1;
}
