#pragma once
/**
 * Milestone 5 — Auto-edit, captions, JSON templates
 */
#include "ms_error.hpp"
#include "ms_project.hpp"
#include <string>
#include <vector>

namespace ms {

struct CutMark {
  double time_sec = 0;
  std::string kind; // "scene" | "silence_start" | "silence_end" | "manual"
  float score = 0;
};

struct CaptionCue {
  double in_sec = 0;
  double out_sec = 0;
  std::string text;
};

/** Scene cuts via frame-difference sampling (PySceneDetect-style idea) */
Error detect_scenes(const std::string& media_path,
                    std::vector<CutMark>& out,
                    double threshold = 0.35,
                    double sample_fps = 2.0);

/** Silence regions from audio levels (Auto-Editor-style idea) */
Error detect_silence(const std::string& media_path,
                     std::vector<CutMark>& out,
                     double silence_db = -35.0,
                     double min_silence_sec = 0.35);

/** Apply scene cuts as split points on first video track */
Error apply_scene_cuts(ProjectSession& session, MediaId media, TrackId track,
                       const std::vector<CutMark>& cuts);

/** Remove silence spans (ripple) on track for matching media clips */
Error apply_silence_removal(ProjectSession& session, TrackId track,
                            const std::vector<CutMark>& silence_marks);

/** SRT load/save */
Error load_srt(const std::string& path, std::vector<CaptionCue>& cues);
Error save_srt(const std::string& path, const std::vector<CaptionCue>& cues);

/** Minimal ASS (dialogue lines only) */
Error save_ass(const std::string& path, const std::vector<CaptionCue>& cues,
               const std::string& title = "MotionStudio");

/** Burn captions into export via ffmpeg subtitles filter (soft path: sidecar) */
Error export_with_srt(const std::string& video, const std::string& srt,
                      const std::string& output);

/**
 * MoviePy-style JSON template:
 * { "name", "width", "height", "fps", "clips":[ {"path","start","duration","source_in"} ],
 *   "captions":[ {"in","out","text"} ] }
 */
Error project_from_json_template(const std::string& json, ProjectSession& session);
Error project_to_json_template(const ProjectSession& session, std::string& json,
                               const std::vector<CaptionCue>* captions = nullptr);

} // namespace ms
