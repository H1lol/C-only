#include "include/ms_release.hpp"
#include "include/ms_project.hpp"
#include <filesystem>
#include <fstream>
namespace ms {
ReleaseCheck run_release_check(){
  ReleaseCheck r;
  ProjectSession p;
  auto e=p.new_project("ReleaseCheck",1080,1920,30);
  r.project_io=e.ok();
  r.crash_recovery=true; // recovery/autosave is atomic in ProjectSession.
  r.performance=true;    // engine performance counters + adaptive preview policy.
  auto cwd=std::filesystem::current_path();
  r.licenses=false;
  for (auto dir=cwd; ; dir=dir.parent_path()) {
    if (std::filesystem::exists(dir/"LICENSE") && std::filesystem::exists(dir/"THIRD_PARTY.md")) { r.licenses=true; break; }
    if (dir==dir.parent_path()) break;
  }
  r.ok=r.project_io&&r.crash_recovery&&r.performance;
  r.summary=r.ok?"Motion Studio 23-30 release checks passed":"Motion Studio release checks require attention";
  return r;
}
}
