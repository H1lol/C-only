#include "include/ms_autosave.hpp"
namespace ms {
double AutosaveController::seconds_since_last() const { if(last_==std::chrono::steady_clock::time_point{}) return 1e9; return std::chrono::duration<double>(std::chrono::steady_clock::now()-last_).count(); }
Error AutosaveController::tick(){if(!enabled_||!project_||path_.empty()||!project_->is_dirty())return Error::success();if(seconds_since_last()<interval_)return Error::success();return force();}
Error AutosaveController::force(){if(!project_||path_.empty())return Error::make(ErrorCode::InvalidArgument,"autosave project/path missing");auto e=project_->autosave(path_);if(e.ok())last_=std::chrono::steady_clock::now();return e;}
}
