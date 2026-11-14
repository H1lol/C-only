#pragma once

#include <string>
#include <functional>

namespace ms {

/// Commands the Flutter UI can send to the engine / session
/// (mirrors EffectCraft's "everything is a command" idea)
enum class UiCommand : int {
  NewProject = 1,
  OpenProject,
  SaveProject,
  Undo,
  Redo,
  Play,
  Pause,
  Stop,
  Seek,
  ToggleProjectPanel,
  TogglePropertiesPanel,
  OpenEffectsDrawer,
  ApplyEffect,
  SelectLayer,
  ImportMedia,
};

struct UiCommandEvent {
  UiCommand cmd;
  double    time_value = 0.0;   // for Seek
  int       int_value  = 0;     // layer index, etc.
  std::string str_value;        // effect id, path, etc.
};

using UiCommandHandler = std::function<void(const UiCommandEvent&)>;

/// Register a handler from the engine side
void set_ui_command_handler(UiCommandHandler handler);

/// Called from Flutter via platform channel / FFI
bool dispatch_ui_command(const UiCommandEvent& ev);

} // namespace ms
