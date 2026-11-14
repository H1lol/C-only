/**
 * MotionStudio full C++ UI application (Linux shell front-end).
 * EffectCraft tokens · every panel registered · workspaces switch visibility.
 *
 * This is the product UI model. Qt/other widget toolkits bind to the same IPanel API later.
 */
#include "ui/ms_panel.hpp"
#include "ui/ms_panels_all.hpp"
#include "ms_project.hpp"
#include "ms_layer.hpp"
#include "ms_cpu_effects.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <cstring>

using namespace ms;
using namespace ms::ui;

static void banner() {
  std::cout
    << "╔══════════════════════════════════════════════════════════╗\n"
    << "║  MotionStudio  ·  C++ UI  ·  EffectCraft visual system   ║\n"
    << "║  Workspaces: Edit Motion Nodes Color Audio Render Dev    ║\n"
    << "╚══════════════════════════════════════════════════════════╝\n";
}

static void help() {
  std::cout
    << "\n── Global ──────────────────────────────────────────\n"
    << "  help | quit | layout | workspace <Name>\n"
    << "  new [name] | seek <sec> | play | pause\n"
    << "  select layer <id>|<name>\n"
    << "\n── Edit / Motion ───────────────────────────────────\n"
    << "  preview [w h]     composite layers → viewer\n"
    << "  layers            list layer panel rows\n"
    << "  effects           list effects\n"
    << "  effects apply <id>\n"
    << "\n── Nodes ───────────────────────────────────────────\n"
    << "  node add <type> [name]\n"
    << "  node connect <from> <to>\n"
    << "  node list\n"
    << "\n── Color ───────────────────────────────────────────\n"
    << "  scopes            analyze current viewer frame\n"
    << "  grade             show grade params\n"
    << "  grade set <exp> <con> <sat>\n"
    << "\n── Audio ───────────────────────────────────────────\n"
    << "  mixer             list strips\n"
    << "\n── Render ──────────────────────────────────────────\n"
    << "  render backends\n"
    << "  render enqueue <name> <backend>\n"
    << "  render list | render cancel <id>\n"
    << "\n── Dev ─────────────────────────────────────────────\n"
    << "  sh <command>      run in Terminal panel\n"
    << "  ask <text>        Copilot\n"
    << "  problems          list | problems clear\n"
    << "\n";
}

static Workspace parse_ws(const std::string& n) {
  if (n == "Motion" || n == "motion") return Workspace::Motion;
  if (n == "Nodes" || n == "nodes") return Workspace::Nodes;
  if (n == "Color" || n == "color") return Workspace::Color;
  if (n == "Audio" || n == "audio") return Workspace::Audio;
  if (n == "Render" || n == "render") return Workspace::Render;
  if (n == "Dev" || n == "dev") return Workspace::Dev;
  return Workspace::Edit;
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  ProjectSession session;
  session.new_project("Demo", 1080, 1920, 30.f);
  if (!session.tracks().empty()) {
    ClipInfo c;
    c.id = 1;
    c.name = "Main Clip";
    c.start = 0;
    c.duration = 10;
    session.add_clip(session.tracks()[0].id, c);
  }

  LayerStack layers;
  {
    Layer a; a.name = "Background"; a.type = LayerType::Solid;
    a.solid_r = 0.12f; a.solid_g = 0.12f; a.solid_b = 0.15f; a.solid_a = 1.f;
    a.start = 0; a.end = 30; layers.add_layer(std::move(a));
    Layer b; b.name = "Accent"; b.type = LayerType::Solid;
    b.solid_r = 0.18f; b.solid_g = 0.45f; b.solid_b = 0.85f; b.solid_a = 0.7f;
    b.blend = BlendMode::Screen; b.start = 0; b.end = 30;
    layers.add_layer(std::move(b));
  }

  PanelHost host;
  register_all_panels(host);
  host.set_session(&session);
  host.set_time({0.0, false});

  if (auto* lp = dynamic_cast<LayerPanel*>(host.get("layers")))
    lp->bind_stack(&layers);
  if (auto* term = dynamic_cast<TerminalPanel*>(host.get("terminal")))
    term->set_cwd(".");

  banner();
  help();
  std::cout << host.dump_layout() << "\n";

  std::string line;
  while (std::cout << "ms> " && std::getline(std::cin, line)) {
    if (line.empty()) continue;
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    auto problems = dynamic_cast<ProblemsPanel*>(host.get("problems"));

    try {
      if (cmd == "quit" || cmd == "exit" || cmd == "q") break;
      if (cmd == "help" || cmd == "h") { help(); continue; }
      if (cmd == "layout") { std::cout << host.dump_layout(); continue; }

      if (cmd == "workspace") {
        std::string n; iss >> n;
        host.set_workspace(parse_ws(n));
        std::cout << host.dump_layout();
        continue;
      }

      if (cmd == "new") {
        std::string name; iss >> name;
        if (name.empty()) name = "Untitled";
        session.new_project(name, 1080, 1920, 30.f);
        host.set_session(&session);
        std::cout << "new project " << name << "\n" << host.dump_layout();
        continue;
      }

      if (cmd == "seek") {
        double t = 0; iss >> t;
        session.seek(t);
        host.set_time({t, host.time().playing});
        std::cout << "t=" << t << "\n";
        continue;
      }
      if (cmd == "play") {
        session.play();
        host.set_time({host.time().seconds, true});
        std::cout << "playing\n";
        continue;
      }
      if (cmd == "pause") {
        session.pause();
        host.set_time({host.time().seconds, false});
        std::cout << "paused\n";
        continue;
      }

      if (cmd == "select") {
        std::string kind, rest;
        iss >> kind;
        std::getline(iss >> std::ws, rest);
        Selection sel;
        if (kind == "layer") {
          sel.kind = Selection::Kind::Layer;
          if (!rest.empty() && std::isdigit(static_cast<unsigned char>(rest[0])))
            sel.id = std::stoull(rest);
          else {
            sel.name = rest;
            if (auto* lp = dynamic_cast<LayerPanel*>(host.get("layers"))) {
              for (const auto& r : lp->rows())
                if (r.name == rest) { sel.id = r.id; break; }
            }
            if (sel.id == 0 && layers.layers().size()) {
              for (const auto& L : layers.layers())
                if (L.name == rest) { sel.id = L.id; break; }
              if (sel.id == 0) sel.id = layers.layers().front().id;
            }
          }
          if (sel.name.empty()) {
            if (auto* L = layers.find(sel.id)) sel.name = L->name;
          }
        } else {
          sel.kind = Selection::Kind::Layer;
          sel.name = kind + (rest.empty() ? "" : " " + rest);
          sel.id = 1;
        }
        host.set_selection(sel);
        if (auto* lp = dynamic_cast<LayerPanel*>(host.get("layers"))) lp->refresh();
        auto* ip = dynamic_cast<InspectorPanel*>(host.get("inspector"));
        if (ip) {
          std::cout << "selected: " << ip->describe() << "\n";
          for (const auto& f : ip->fields())
            std::cout << "  " << f.label << " = " << f.value << "\n";
        }
        continue;
      }

      if (cmd == "preview") {
        int w = 64, h = 64;
        iss >> w >> h;
        if (w <= 0) w = 64;
        if (h <= 0) h = 64;
        auto* vp = dynamic_cast<ViewerPanel*>(host.get("viewer"));
        if (!vp) continue;
        Error e = vp->render_preview(layers, w, h);
        if (!e.ok() && problems) problems->push(e, "preview");
        std::cout << (e.ok() ? "OK " : "FAIL ") << vp->describe() << "\n";
        continue;
      }

      if (cmd == "layers") {
        auto* lp = dynamic_cast<LayerPanel*>(host.get("layers"));
        if (lp) {
          lp->refresh();
          for (const auto& r : lp->rows())
            std::cout << (r.selected ? ">>" : "  ") << r.id << " " << r.name
                      << " vis=" << r.visible << " " << r.blend << "\n";
        }
        continue;
      }

      if (cmd == "effects") {
        std::string sub; iss >> sub;
        auto* ep = dynamic_cast<EffectsPanel*>(host.get("effects"));
        if (!ep) continue;
        ep->refresh();
        if (sub == "apply") {
          std::string id; iss >> id;
          Error e = ep->apply_to_selection(id, &layers);
          if (!e.ok()) {
            std::cout << e.message << "\n";
            if (problems) problems->push(e, "effects apply");
            if (auto* cp = dynamic_cast<CopilotPanel*>(host.get("copilot")))
              cp->ingest_error(e.message);
          } else {
            std::cout << "applied " << id << "\n";
            if (auto* lp = dynamic_cast<LayerPanel*>(host.get("layers"))) lp->refresh();
          }
        } else {
          for (const auto& it : ep->items())
            std::cout << "  " << it.id << " — " << it.name
                      << " [" << it.category << "]"
                      << (it.open_source ? " OS" : "")
                      << (it.installed ? " installed" : "") << "\n";
        }
        continue;
      }

      if (cmd == "node") {
        auto* gp = dynamic_cast<GraphPanel*>(host.get("graph"));
        if (!gp) continue;
        std::string sub; iss >> sub;
        if (sub == "add") {
          std::string type, name;
          iss >> type;
          std::getline(iss >> std::ws, name);
          auto id = gp->add_node(type, name);
          std::cout << "node " << id << " " << type << "\n";
        } else if (sub == "connect") {
          uint64_t a = 0, b = 0; iss >> a >> b;
          gp->connect(a, b);
          std::cout << "connected " << a << " -> " << b << "\n";
        } else if (sub == "list") {
          for (const auto& n : gp->nodes())
            std::cout << "  " << n.id << " " << n.type << " " << n.name
                      << (n.enabled ? "" : " [off]") << "\n";
          for (const auto& e : gp->edges())
            std::cout << "  edge " << e.from << " -> " << e.to << "\n";
        }
        std::cout << gp->describe() << "\n";
        continue;
      }

      if (cmd == "scopes") {
        auto* sp = dynamic_cast<ScopesPanel*>(host.get("scopes"));
        auto* vp = dynamic_cast<ViewerPanel*>(host.get("viewer"));
        if (sp && vp) {
          if (!vp->frame().valid())
            vp->render_preview(layers, 64, 64);
          Error e = sp->analyze(vp->frame());
          if (!e.ok() && problems) problems->push(e, "scopes");
          std::cout << sp->describe() << "\n";
        }
        continue;
      }

      if (cmd == "grade") {
        auto* g = dynamic_cast<GradePanel*>(host.get("grade"));
        if (!g) continue;
        std::string sub; iss >> sub;
        if (sub == "set") {
          iss >> g->exposure >> g->contrast >> g->saturation;
        }
        std::cout << g->describe() << "\n";
        continue;
      }

      if (cmd == "mixer") {
        auto* m = dynamic_cast<MixerPanel*>(host.get("mixer"));
        if (m) {
          m->refresh();
          for (const auto& s : m->strips())
            std::cout << "  " << s.name << " " << s.level_db << "dB peak=" << s.peak
                      << (s.mute ? " MUTE" : "") << "\n";
        }
        continue;
      }

      if (cmd == "render") {
        auto* rq = dynamic_cast<RenderQueuePanel*>(host.get("render_queue"));
        if (!rq) continue;
        std::string sub; iss >> sub;
        if (sub == "backends") {
          for (const auto& b : rq->available_backends()) std::cout << "  " << b << "\n";
        } else if (sub == "enqueue") {
          std::string name, backend;
          iss >> name >> backend;
          if (backend.empty()) backend = "realtime_gpu";
          auto id = rq->enqueue(name, backend);
          std::cout << "job " << id << " queued on " << backend << "\n";
        } else if (sub == "cancel") {
          uint64_t id = 0; iss >> id;
          std::cout << (rq->cancel(id) ? "cancelled\n" : "not found\n");
        } else {
          for (const auto& j : rq->jobs())
            std::cout << "  #" << j.id << " " << j.name << " " << j.backend
                      << " " << j.status << " " << int(j.progress * 100) << "%\n";
        }
        continue;
      }

      if (cmd == "sh" || cmd == "shell") {
        std::string rest;
        std::getline(iss >> std::ws, rest);
        auto* term = dynamic_cast<TerminalPanel*>(host.get("terminal"));
        if (term && !rest.empty()) {
          Error e = term->run(rest);
          std::cout << term->last_output();
          if (!term->last_output().empty() && term->last_output().back() != '\n')
            std::cout << "\n";
          if (!e.ok() && problems) problems->push(e, "terminal");
        }
        continue;
      }

      if (cmd == "ask") {
        std::string rest;
        std::getline(iss >> std::ws, rest);
        auto* cp = dynamic_cast<CopilotPanel*>(host.get("copilot"));
        if (cp) {
          cp->ask(rest);
          for (const auto& m : cp->messages()) {
            if (&m == &cp->messages().back() || &m == &cp->messages()[cp->messages().size() - 2])
              std::cout << (m.from_user ? "you> " : "copilot> ") << m.text << "\n";
          }
          if (!cp->last_suggestion().empty())
            std::cout << "suggest: " << cp->last_suggestion() << "\n";
        }
        continue;
      }

      if (cmd == "problems") {
        std::string sub; iss >> sub;
        if (!problems) continue;
        if (sub == "clear") problems->clear();
        for (const auto& it : problems->items())
          std::cout << "  [" << error_code_string(it.code) << "] " << it.message
                    << (it.context.empty() ? "" : " @ " + it.context) << "\n";
        if (problems->items().empty()) std::cout << "  (clean)\n";
        continue;
      }

      std::cout << "unknown (help)\n";
    } catch (const std::exception& ex) {
      std::cout << "error: " << ex.what() << "\n";
      if (problems)
        problems->push(Error::make(ErrorCode::Unknown, ex.what()), "shell");
    }
  }
  std::cout << "bye\n";
  return 0;
}
