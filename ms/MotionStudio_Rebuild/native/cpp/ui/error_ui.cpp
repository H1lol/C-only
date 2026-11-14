#include "ms_error_ui.hpp"
#include <sstream>
#include <algorithm>

namespace ms {

ErrorUI& ErrorUI::instance() {
  static ErrorUI ui;
  return ui;
}

int64_t ErrorUI::now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

ErrorScreen ErrorUI::make_screen(const Error& e, const std::string& context,
                                 ErrorScreenKind kind_override) {
  ErrorScreen s;
  s.id = next_id_++;
  s.code = e.code;
  s.severity = severity_for(e.code);
  s.kind = kind_override != ErrorScreenKind::None ? kind_override : default_kind_for(s.severity);
  s.title = title_for(e.code);
  s.message = e.message.empty() ? error_code_string(e.code) : e.message;
  s.detail = e.detail;
  s.context = context.empty() ? e.context : context;
  s.created_ms = now_ms();
  if (s.kind == ErrorScreenKind::Toast) s.auto_dismiss_ms = 4000;
  if (s.kind == ErrorScreenKind::Banner) s.auto_dismiss_ms = 8000;

  s.actions.push_back({"dismiss", "OK"});
  if (s.severity >= ErrorSeverity::Error) {
    s.actions.push_back({"details", "Details"});
    s.actions.push_back({"open_problems", "Problems"});
  }
  if (e.code == ErrorCode::MediaOpenFailed || e.code == ErrorCode::MediaDecodeFailed ||
      e.code == ErrorCode::NetworkTimeout || e.code == ErrorCode::NetworkUnavailable) {
    s.actions.push_back({"retry", "Retry"});
  }
  return s;
}

uint64_t ErrorUI::show(const Error& e, const std::string& context,
                       ErrorScreenKind kind_override) {
  if (e.ok()) return 0;
  ErrorScreen s;
  Listener fn;
  {
    std::lock_guard<std::mutex> lock(mu_);
    s = make_screen(e, context, kind_override);
    active_.push_back(s);
    history_.push_back(s);
    if (history_.size() > 200)
      history_.erase(history_.begin(), history_.begin() + 50);
    fn = listener_;
  }
  if (fn) fn(s);
  return s.id;
}

uint64_t ErrorUI::show_message(ErrorSeverity sev, const std::string& title,
                               const std::string& message,
                               ErrorScreenKind kind,
                               const std::string& detail,
                               const std::string& context) {
  Error e = Error::make(ErrorCode::Unknown, message, detail, context);
  ErrorScreen s;
  Listener fn;
  {
    std::lock_guard<std::mutex> lock(mu_);
    s = make_screen(e, context, kind);
    s.severity = sev;
    s.kind = kind;
    s.title = title.empty() ? severity_string(sev) : title;
    if (kind == ErrorScreenKind::Toast) s.auto_dismiss_ms = 4000;
    active_.push_back(s);
    history_.push_back(s);
    fn = listener_;
  }
  if (fn) fn(s);
  return s.id;
}

bool ErrorUI::dismiss(uint64_t id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = std::remove_if(active_.begin(), active_.end(),
                           [id](const ErrorScreen& x) { return x.id == id; });
  if (it == active_.end()) return false;
  active_.erase(it, active_.end());
  return true;
}

void ErrorUI::dismiss_all() {
  std::lock_guard<std::mutex> lock(mu_);
  active_.clear();
}

void ErrorUI::clear_history() {
  std::lock_guard<std::mutex> lock(mu_);
  history_.clear();
}

std::vector<ErrorScreen> ErrorUI::active() const {
  std::lock_guard<std::mutex> lock(mu_);
  return active_;
}

bool ErrorUI::try_blocking(ErrorScreen& out) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto it = active_.rbegin(); it != active_.rend(); ++it) {
    if (it->kind == ErrorScreenKind::Modal || it->kind == ErrorScreenKind::FullScreen) {
      out = *it;
      return true;
    }
  }
  return false;
}

const ErrorScreen* ErrorUI::blocking() const {
  return nullptr; // use try_blocking
}

std::string ErrorUI::describe_active() const {
  std::lock_guard<std::mutex> lock(mu_);
  if (active_.empty()) return "(no active errors)\n";
  std::ostringstream os;
  for (const auto& s : active_) {
    os << "[" << severity_string(s.severity) << "] " << s.title << ": " << s.message;
    if (!s.context.empty()) os << " (" << s.context << ")";
    os << "\n";
  }
  return os.str();
}

std::string ErrorUI::describe_history(size_t max_items) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::ostringstream os;
  size_t start = history_.size() > max_items ? history_.size() - max_items : 0;
  for (size_t i = start; i < history_.size(); ++i) {
    const auto& s = history_[i];
    os << "#" << s.id << " [" << severity_string(s.severity) << "] "
       << s.title << " — " << s.message << "\n";
  }
  if (history_.empty()) os << "(empty)\n";
  return os.str();
}

} // namespace ms
