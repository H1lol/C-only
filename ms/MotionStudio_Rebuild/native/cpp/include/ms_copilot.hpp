#pragma once
/**
 * Phase 6 — Local Copilot (rules first; optional HTTP later)
 */
#include <string>
#include <vector>

namespace ms {

struct CopilotContext {
  std::string workspace;
  std::string selection;
  std::string last_error;
  int clip_count = 0;
  bool dirty = false;
};

struct CopilotSuggestion {
  std::string answer;
  std::string action; // optional command hint
};

class CopilotService {
public:
  /** Always-on local rules — no network */
  static CopilotSuggestion ask_local(const std::string& question, const CopilotContext& ctx);

  /** Optional: if OPENAI_BASE_URL set, try HTTP; else fall back to local */
  static CopilotSuggestion ask(const std::string& question, const CopilotContext& ctx);
};

/** Phase 6 — shell helper (non-interactive command capture) */
class TerminalRunner {
public:
  static std::string run(const std::string& command, int max_chars = 4000);
};

} // namespace ms
