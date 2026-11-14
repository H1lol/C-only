#pragma once
#include "ms_project.hpp"
#include <chrono>
#include <string>
namespace ms {
class AutosaveController {
public:
  explicit AutosaveController(ProjectSession* project=nullptr): project_(project) {}
  void set_project(ProjectSession* p){project_=p;}
  void set_path(const std::string& p){path_=p;}
  void set_interval(double seconds){interval_=seconds>1?seconds:1;}
  void set_enabled(bool e){enabled_=e;}
  bool enabled()const{return enabled_;}
  const std::string& path()const{return path_;}
  Error tick();
  Error force();
  double seconds_since_last() const;
private:
  ProjectSession* project_=nullptr; std::string path_; double interval_=10; bool enabled_=true; std::chrono::steady_clock::time_point last_{};
};
}
