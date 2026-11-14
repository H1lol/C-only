#include "ms_copilot.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <array>
#include <memory>

namespace ms {
namespace {
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool has(const std::string& q, const char* k) {
  return q.find(k) != std::string::npos;
}
} // namespace

CopilotSuggestion CopilotService::ask_local(const std::string& question, const CopilotContext& ctx) {
  CopilotSuggestion s;
  std::string q = lower(question);

  if (has(q, "split") && has(q, "clip")) {
    s.answer = "Use timeline split: select clip, split at playhead (Phase 1: ProjectSession::split_clip).";
    s.action = "split_clip";
    return s;
  }
  if (has(q, "blur") || has(q, "gaussian")) {
    s.answer = "Apply CPU effect gaussian_blur via EffectStack or node Blur. Params: radius.";
    s.action = "effects apply gaussian_blur";
    return s;
  }
  if (has(q, "export") || has(q, "render")) {
    s.answer = "Phase 5: enqueue a RenderQueue job (realtime_cpu) or SequenceExporter for frame sequence.";
    s.action = "render_queue process";
    return s;
  }
  if (has(q, "undo")) {
    s.answer = "ProjectSession::undo() restores last snapshot. can_undo() checks stack.";
    s.action = "undo";
    return s;
  }
  if (has(q, "save") || has(q, "project")) {
    s.answer = "Save with ProjectSession::save_to_file path.msproj. Autosave when dirty.";
    s.action = "save";
    return s;
  }
  if (has(q, "node") || has(q, "grade")) {
    s.answer = "Phase 4: NodeGraph (Constant/Blur/Grade/Merge/Viewer) and ColorPipeline::apply_grade.";
    return s;
  }
  if (has(q, "error") || !ctx.last_error.empty()) {
    s.answer = "Last error context: " + (ctx.last_error.empty() ? "(none)" : ctx.last_error) +
               ". Check Problems panel; use Terminal for logs.";
    return s;
  }

  s.answer = "MotionStudio local Copilot. Workspace=" + ctx.workspace +
             " clips=" + std::to_string(ctx.clip_count) +
             (ctx.dirty ? " (dirty)" : "") +
             ". Try: split clip, blur, export, undo, save, nodes.";
  return s;
}

CopilotSuggestion CopilotService::ask(const std::string& question, const CopilotContext& ctx) {
  // Phase 6: cloud optional — not required; always safe local
  return ask_local(question, ctx);
}

std::string TerminalRunner::run(const std::string& command, int max_chars) {
  std::string cmd = command + " 2>&1";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return "error: popen failed";
  std::string out;
  char buf[256];
  while (fgets(buf, sizeof(buf), pipe)) {
    out += buf;
    if (static_cast<int>(out.size()) >= max_chars) break;
  }
  pclose(pipe);
  if (static_cast<int>(out.size()) > max_chars) out.resize(max_chars);
  return out;
}

} // namespace ms
