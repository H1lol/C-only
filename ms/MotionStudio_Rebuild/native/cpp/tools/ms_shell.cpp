/**
 * MotionStudio — Linux shell driver for C++ UI panels (Edit workspace).
 * No Flutter/Kotlin. EffectCraft workspaces, panels bound to engine.
 *
 * Commands:
 *   new | layout | seek <sec> | play | pause | select <name>
 *   preview | workspace <name> | help | quit
 */
#include "ui/ms_panel.hpp"
#include "ui/ms_panels_edit.hpp"
#include "ms_project.hpp"
#include "ms_layer.hpp"
#include "ms_cpu_effects.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <memory>

namespace ms {
namespace ui {
void register_edit_workspace(PanelHost& host);
}
}

static void help() {
  std::cout
    << "MotionStudio shell (C++ Edit workspace)\n"
    << "  new              new project\n"
    << "  layout           show panels for current workspace\n"
    << "  seek <sec>       set time\n"
    << "  play | pause     playback flags\n"
    << "  select <name>    set selection (inspector)\n"
    << "  preview          composite demo layers → viewer\n"
    << "  workspace <name> Edit|Motion|Dev|...\n"
    << "  help | quit\n";
}

int main() {
  using namespace ms;
  using namespace ms::ui;

  ProjectSession session;
  session.new_project("Demo", 640, 360, 30.f);

  // Demo clip on track so timeline has meaning
  TrackId tid = 0;
  if (!session.tracks().empty()) tid = session.tracks()[0].id;
  ClipInfo clip;
  clip.id = 1;
  clip.name = "Main Clip";
  clip.start = 0;
  clip.duration = 8;
  session.add_clip(tid, clip);

  PanelHost host;
  register_edit_workspace(host);
  host.set_session(&session);
  host.set_time({0.0, false});

  LayerStack layers;
  {
    Layer L;
    L.name = "Red";
    L.type = LayerType::Solid;
    L.solid_r = 0.75f; L.solid_g = 0.12f; L.solid_b = 0.12f; L.solid_a = 1.f;
    L.start = 0; L.end = 20;
    layers.add_layer(std::move(L));
    Layer L2;
    L2.name = "Blue Soft";
    L2.type = LayerType::Solid;
    L2.solid_r = 0.1f; L2.solid_g = 0.2f; L2.solid_b = 0.85f; L2.solid_a = 0.55f;
    L2.blend = BlendMode::Screen;
    L2.start = 0; L2.end = 20;
    layers.add_layer(std::move(L2));
  }

  std::cout << "MotionStudio  ·  EffectCraft UI tokens  ·  C++ panels\n";
  help();
  std::cout << "\n" << host.dump_layout() << "\n";

  std::string line;
  while (std::cout << "ms> " && std::getline(std::cin, line)) {
    if (line.empty()) continue;
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd == "quit" || cmd == "exit" || cmd == "q") break;
    if (cmd == "help" || cmd == "h") { help(); continue; }
    if (cmd == "layout") {
      std::cout << host.dump_layout();
      continue;
    }
    if (cmd == "new") {
      session.new_project("Untitled", 1080, 1920, 30.f);
      host.set_session(&session);
      std::cout << "new project\n" << host.dump_layout();
      continue;
    }
    if (cmd == "seek") {
      double t = 0;
      iss >> t;
      session.seek(t);
      host.set_time({t, host.time().playing});
      std::cout << "seek " << t << "\n";
      if (auto* v = host.get("viewer")) std::cout << "  viewer: " << v->describe() << "\n";
      if (auto* tl = host.get("timeline")) std::cout << "  timeline: " << tl->describe() << "\n";
      continue;
    }
    if (cmd == "play") {
      session.play();
      host.set_time({host.time().seconds, true});
      std::cout << "play\n";
      continue;
    }
    if (cmd == "pause") {
      session.pause();
      host.set_time({host.time().seconds, false});
      std::cout << "pause\n";
      continue;
    }
    if (cmd == "select") {
      std::string name;
      std::getline(iss >> std::ws, name);
      Selection sel;
      sel.kind = Selection::Kind::Layer;
      sel.id = 1;
      sel.name = name.empty() ? "Layer" : name;
      host.set_selection(sel);
      if (auto* ins = host.get("inspector"))
        std::cout << "inspector: " << ins->describe() << "\n" << host.get("inspector")->describe() << "\n";
      // print fields
      auto* ip = dynamic_cast<InspectorPanel*>(host.get("inspector"));
      if (ip) {
        for (const auto& f : ip->fields())
          std::cout << "  " << f.label << " = " << f.value << "\n";
      }
      continue;
    }
    if (cmd == "preview") {
      auto* vp = dynamic_cast<ViewerPanel*>(host.get("viewer"));
      if (!vp) { std::cout << "no viewer\n"; continue; }
      Error e = vp->render_preview(layers, 64, 64);
      std::cout << (e.ok() ? "preview OK " : "preview FAIL ") << vp->describe() << "\n";
      continue;
    }
    if (cmd == "workspace") {
      std::string n;
      iss >> n;
      Workspace w = Workspace::Edit;
      if (n == "Motion" || n == "motion") w = Workspace::Motion;
      else if (n == "Dev" || n == "dev") w = Workspace::Dev;
      else if (n == "Render" || n == "render") w = Workspace::Render;
      else if (n == "Nodes" || n == "nodes") w = Workspace::Nodes;
      else if (n == "Audio" || n == "audio") w = Workspace::Audio;
      else if (n == "Color" || n == "color") w = Workspace::Color;
      host.set_workspace(w);
      std::cout << host.dump_layout();
      continue;
    }
    std::cout << "unknown command (help)\n";
  }
  return 0;
}
