#pragma once
/**
 * Error screens / toasts / Problems log for MotionStudio UI
 */
#include "ms_error.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>
#include <chrono>

namespace ms {

enum class ErrorSeverity : uint8_t {
  Info = 0,
  Warning,
  Error,
  Critical,
};

enum class ErrorScreenKind : uint8_t {
  None = 0,
  Toast,          // brief non-blocking
  Banner,         // top bar
  Modal,          // blocking dialog
  FullScreen,     // fatal / blocking full view
};

struct ErrorAction {
  std::string id;       // "dismiss" | "retry" | "open_problems" | "details"
  std::string label;
};

struct ErrorScreen {
  uint64_t id = 0;
  ErrorSeverity severity = ErrorSeverity::Error;
  ErrorScreenKind kind = ErrorScreenKind::Modal;
  ErrorCode code = ErrorCode::Unknown;
  std::string title;
  std::string message;
  std::string detail;
  std::string context;
  std::vector<ErrorAction> actions;
  int64_t created_ms = 0;
  int auto_dismiss_ms = 0; // 0 = manual; toast default 4000
  bool dismissed = false;
};

inline ErrorSeverity severity_for(ErrorCode c) {
  switch (c) {
    case ErrorCode::Ok: return ErrorSeverity::Info;
    case ErrorCode::EffectNotFound:
    case ErrorCode::NotSupported:
      return ErrorSeverity::Warning;
    case ErrorCode::OutOfMemory:
    case ErrorCode::GpuContextLost:
    case ErrorCode::FileCorrupt:
    case ErrorCode::ProjectLoadFailed:
      return ErrorSeverity::Critical;
    default:
      return ErrorSeverity::Error;
  }
}

inline ErrorScreenKind default_kind_for(ErrorSeverity s) {
  switch (s) {
    case ErrorSeverity::Info: return ErrorScreenKind::Toast;
    case ErrorSeverity::Warning: return ErrorScreenKind::Banner;
    case ErrorSeverity::Error: return ErrorScreenKind::Modal;
    case ErrorSeverity::Critical: return ErrorScreenKind::FullScreen;
  }
  return ErrorScreenKind::Modal;
}

inline const char* severity_string(ErrorSeverity s) {
  switch (s) {
    case ErrorSeverity::Info: return "Info";
    case ErrorSeverity::Warning: return "Warning";
    case ErrorSeverity::Error: return "Error";
    case ErrorSeverity::Critical: return "Critical";
  }
  return "Error";
}

inline std::string title_for(ErrorCode c) {
  switch (c) {
    case ErrorCode::MediaOpenFailed: return "Cannot Open Media";
    case ErrorCode::MediaDecodeFailed: return "Decode Failed";
    case ErrorCode::MediaProbeFailed: return "Media Probe Failed";
    case ErrorCode::MediaUnsupportedFormat: return "Unsupported Format";
    case ErrorCode::ProjectLoadFailed: return "Project Load Failed";
    case ErrorCode::ProjectSaveFailed: return "Project Save Failed";
    case ErrorCode::FileNotFound: return "File Not Found";
    case ErrorCode::FilePermission: return "Permission Denied";
    case ErrorCode::FileCorrupt: return "File Corrupt";
    case ErrorCode::OutOfMemory: return "Out of Memory";
    case ErrorCode::NotInitialized: return "Not Ready";
    case ErrorCode::NotSupported: return "Not Supported";
    case ErrorCode::InvalidArgument: return "Invalid Input";
    case ErrorCode::GpuInitFailed: return "GPU Init Failed";
    case ErrorCode::GpuContextLost: return "GPU Context Lost";
    case ErrorCode::EffectNotFound: return "Effect Not Found";
    case ErrorCode::EffectLoadFailed: return "Effect Load Failed";
    case ErrorCode::NetworkUnavailable: return "Network Unavailable";
    case ErrorCode::NetworkTimeout: return "Network Timeout";
    case ErrorCode::PlaybackFailed: return "Playback Failed";
    default: return std::string(error_code_string(c));
  }
}

/**
 * Central error UI bus — panels and shell subscribe / poll
 */
class ErrorUI {
public:
  static ErrorUI& instance();

  /** Build screen from Error and show (also logs to history) */
  uint64_t show(const Error& e, const std::string& context = "",
                ErrorScreenKind kind_override = ErrorScreenKind::None);

  uint64_t show_message(ErrorSeverity sev, const std::string& title,
                        const std::string& message,
                        ErrorScreenKind kind = ErrorScreenKind::Modal,
                        const std::string& detail = "",
                        const std::string& context = "");

  bool dismiss(uint64_t id);
  void dismiss_all();
  void clear_history();

  /** Active (not dismissed) screens */
  std::vector<ErrorScreen> active() const;
  /** Modal or full-screen that should block UI */
  /** Copy of top blocking screen if any */
  bool try_blocking(ErrorScreen& out) const;
  const ErrorScreen* blocking() const; // prefer try_blocking in multi-thread
  /** Full history (including dismissed), newest last */
  const std::vector<ErrorScreen>& history() const { return history_; }

  /** Text dump for shell / Problems panel */
  std::string describe_active() const;
  std::string describe_history(size_t max_items = 20) const;

  using Listener = std::function<void(const ErrorScreen&)>;
  void set_listener(Listener fn) { listener_ = std::move(fn); }

private:
  ErrorUI() = default;
  uint64_t next_id_ = 1;
  std::vector<ErrorScreen> active_;
  std::vector<ErrorScreen> history_;
  Listener listener_;
  mutable std::mutex mu_;

  static int64_t now_ms();
  ErrorScreen make_screen(const Error& e, const std::string& context,
                          ErrorScreenKind kind_override);
};

/** Convenience */
inline uint64_t report_error(const Error& e, const std::string& context = "") {
  return ErrorUI::instance().show(e, context);
}

} // namespace ms
