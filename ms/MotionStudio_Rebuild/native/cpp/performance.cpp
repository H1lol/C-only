#include "include/ms_performance.hpp"
#include <algorithm>
namespace ms {
void PerformanceMonitor::begin_frame(){ start_=std::chrono::steady_clock::now(); }
void PerformanceMonitor::end_frame(bool hit){ auto now=std::chrono::steady_clock::now(); stats_.frame_ms=std::chrono::duration<double,std::milli>(now-start_).count(); stats_.fps=stats_.frame_ms>0?1000.0/stats_.frame_ms:0; stats_.rendered++; if(hit)stats_.cache_hits++; else stats_.cache_misses++; }
void PerformanceMonitor::set_backend(const std::string& n){stats_.backend=n;}
void PerformanceMonitor::set_cache_bytes(size_t b){stats_.cache_bytes=b;}
void RenderBudget::set_target_fps(double fps){target_fps_=std::max(1.0,fps);}
int RenderBudget::preview_scale(double ms) const { double b=frame_budget_ms(); if(ms<=b*.75)return 100; if(ms<=b)return 85; if(ms<=b*1.5)return 67; return 50; }
}
