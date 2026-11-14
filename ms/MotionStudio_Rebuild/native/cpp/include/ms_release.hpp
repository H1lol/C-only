#pragma once
#include <string>
namespace ms {
struct ReleaseCheck {
  bool ok=true;
  bool project_io=false;
  bool crash_recovery=false;
  bool performance=false;
  bool licenses=false;
  std::string summary;
};
ReleaseCheck run_release_check();
}
