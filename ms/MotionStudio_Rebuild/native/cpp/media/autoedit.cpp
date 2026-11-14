#include "ms_autoedit.hpp"
#include "ms_ffmpeg_io.hpp"
#include "ms_image.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace ms {
namespace fs = std::filesystem;

static double frame_diff(const ImageBuffer& a, const ImageBuffer& b) {
  if (!a.valid() || !b.valid() || a.width() != b.width() || a.height() != b.height())
    return 1.0;
  const int n = a.width() * a.height();
  const uint8_t* pa = a.data();
  const uint8_t* pb = b.data();
  double sum = 0;
  // subsample
  for (int i = 0; i < n; i += 4) {
    int da = std::abs((int)pa[i * 4] - (int)pb[i * 4]);
    int dg = std::abs((int)pa[i * 4 + 1] - (int)pb[i * 4 + 1]);
    int db = std::abs((int)pa[i * 4 + 2] - (int)pb[i * 4 + 2]);
    sum += (da + dg + db) / (3.0 * 255.0);
  }
  int samples = (n + 3) / 4;
  return samples > 0 ? sum / samples : 0;
}

Error detect_scenes(const std::string& media_path,
                    std::vector<CutMark>& out,
                    double threshold,
                    double sample_fps) {
  out.clear();
  auto probe = ffmpeg_probe(media_path);
  double dur = probe.duration_ms / 1000.0;
  if (dur <= 0) dur = 5.0;
  if (sample_fps < 0.5) sample_fps = 2.0;

  ImageBuffer prev, cur;
  bool have_prev = false;
  for (double t = 0; t < dur; t += 1.0 / sample_fps) {
    if (!ffmpeg_decode_frame(media_path, t, cur, 160, 90).ok()) continue;
    if (have_prev) {
      double d = frame_diff(prev, cur);
      if (d >= threshold) {
        CutMark m;
        m.time_sec = t;
        m.kind = "scene";
        m.score = static_cast<float>(d);
        out.push_back(m);
      }
    }
    prev = ImageBuffer{};
    if (prev.allocate(cur.width(), cur.height(), PixelFormat::RGBA8)) {
      std::memcpy(prev.data(), cur.data(),
                  static_cast<size_t>(cur.width()) * cur.height() * 4);
      have_prev = true;
    }
  }
  return Error::success();
}

Error detect_silence(const std::string& media_path,
                     std::vector<CutMark>& out,
                     double silence_db,
                     double min_silence_sec) {
  out.clear();
  // Use ffmpeg silencedetect
  std::string log = "/tmp/ms_silence_" + std::to_string(std::rand()) + ".txt";
  std::ostringstream cmd;
  cmd << "ffmpeg -v info -i \"" << media_path
      << "\" -af silencedetect=noise=" << silence_db << "dB:d=" << min_silence_sec
      << " -f null - 2>\"" << log << "\"";
  std::system(cmd.str().c_str());
  std::ifstream in(log);
  if (!in) return Error::make(ErrorCode::MediaDecodeFailed, "silence log");
  std::string line;
  while (std::getline(in, line)) {
    auto p = line.find("silence_start:");
    if (p != std::string::npos) {
      CutMark m;
      m.kind = "silence_start";
      m.time_sec = std::atof(line.c_str() + p + 14);
      out.push_back(m);
    }
    p = line.find("silence_end:");
    if (p != std::string::npos) {
      CutMark m;
      m.kind = "silence_end";
      m.time_sec = std::atof(line.c_str() + p + 12);
      // duration may follow
      auto dpos = line.find("silence_duration:");
      if (dpos != std::string::npos)
        m.score = static_cast<float>(std::atof(line.c_str() + dpos + 17));
      out.push_back(m);
    }
  }
  fs::remove(log);
  return Error::success();
}

Error apply_scene_cuts(ProjectSession& session, MediaId media, TrackId track,
                       const std::vector<CutMark>& cuts) {
  auto* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  // Find clip using this media
  for (size_t i = 0; i < tr->clips.size(); ++i) {
    auto& c = tr->clips[i];
    if (c.media_id != media) continue;
    std::vector<double> points;
    for (const auto& m : cuts) {
      if (m.kind != "scene") continue;
      double local = c.start + (m.time_sec - c.source_in);
      if (local > c.start + 0.05 && local < c.start + c.duration - 0.05)
        points.push_back(local);
    }
    std::sort(points.begin(), points.end());
    // Split from end so indices stay valid
    for (int j = static_cast<int>(points.size()) - 1; j >= 0; --j) {
      double at = points[j];
      ClipInfo right = c;
      right.id = 0;
      right.start = at;
      right.duration = (c.start + c.duration) - at;
      right.source_in = c.source_in + (at - c.start);
      c.duration = at - c.start;
      session.add_clip(track, right);
      // refresh pointer
      tr = session.find_track(track);
      if (!tr) break;
      // find left clip again by position
      for (auto& cc : tr->clips)
        if (std::abs(cc.start - c.start) < 1e-6) { c = cc; break; }
    }
    return Error::success();
  }
  return Error::make(ErrorCode::InvalidArgument, "media clip not on track");
}

Error apply_silence_removal(ProjectSession& session, TrackId track,
                            const std::vector<CutMark>& silence_marks) {
  // Build [start,end) silence ranges
  std::vector<std::pair<double, double>> ranges;
  double start = -1;
  for (const auto& m : silence_marks) {
    if (m.kind == "silence_start") start = m.time_sec;
    if (m.kind == "silence_end" && start >= 0) {
      ranges.push_back({start, m.time_sec});
      start = -1;
    }
  }
  auto* tr = session.find_track(track);
  if (!tr) return Error::make(ErrorCode::InvalidArgument, "track");
  // For each range overlapping a clip, shrink/split — simplified: skip entire silent-only clips
  std::vector<ClipInfo> kept;
  for (auto& c : tr->clips) {
    bool fully_silent = false;
    for (auto& r : ranges) {
      if (r.first <= c.source_in && r.second >= c.source_in + c.duration)
        fully_silent = true;
    }
    if (!fully_silent) kept.push_back(c);
  }
  // Ripple pack
  double cursor = 0;
  std::sort(kept.begin(), kept.end(),
            [](const ClipInfo& a, const ClipInfo& b) { return a.start < b.start; });
  for (auto& c : kept) {
    c.start = cursor;
    cursor += c.duration;
  }
  tr->clips = kept;
  return Error::success();
}

static std::string srt_time(double sec) {
  int h = static_cast<int>(sec / 3600);
  int m = static_cast<int>(std::fmod(sec, 3600) / 60);
  int s = static_cast<int>(std::fmod(sec, 60));
  int ms = static_cast<int>(std::fmod(sec, 1.0) * 1000);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d,%03d", h, m, s, ms);
  return buf;
}

Error load_srt(const std::string& path, std::vector<CaptionCue>& cues) {
  cues.clear();
  std::ifstream in(path);
  if (!in) return Error::make(ErrorCode::FileNotFound, path);
  std::string line;
  CaptionCue cur;
  int phase = 0; // 0=index 1=time 2=text
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) {
      if (!cur.text.empty()) cues.push_back(cur);
      cur = {};
      phase = 0;
      continue;
    }
    if (phase == 0) { phase = 1; continue; }
    if (phase == 1) {
      // 00:00:01,000 --> 00:00:02,000
      auto arrow = line.find("-->");
      if (arrow != std::string::npos) {
        auto parse = [](const std::string& t) {
          int h=0,m=0,s=0,ms=0;
          std::sscanf(t.c_str(), "%d:%d:%d,%d", &h, &m, &s, &ms);
          return h*3600.0 + m*60.0 + s + ms/1000.0;
        };
        cur.in_sec = parse(line.substr(0, arrow));
        cur.out_sec = parse(line.substr(arrow + 3));
        phase = 2;
      }
      continue;
    }
    if (phase == 2) {
      if (!cur.text.empty()) cur.text += "\n";
      cur.text += line;
    }
  }
  if (!cur.text.empty()) cues.push_back(cur);
  return Error::success();
}

Error save_srt(const std::string& path, const std::vector<CaptionCue>& cues) {
  std::ofstream out(path);
  if (!out) return Error::make(ErrorCode::ProjectSaveFailed, path);
  int i = 1;
  for (const auto& c : cues) {
    out << i++ << "\n"
        << srt_time(c.in_sec) << " --> " << srt_time(c.out_sec) << "\n"
        << c.text << "\n\n";
  }
  return Error::success();
}

Error save_ass(const std::string& path, const std::vector<CaptionCue>& cues,
               const std::string& title) {
  std::ofstream out(path);
  if (!out) return Error::make(ErrorCode::ProjectSaveFailed, path);
  out << "[Script Info]\nTitle: " << title << "\nScriptType: v4.00+\n\n";
  out << "[V4+ Styles]\n";
  out << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
         "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
         "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
         "Alignment, MarginL, MarginR, MarginV, Encoding\n";
  out << "Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,"
         "0,0,0,0,100,100,0,0,1,2,1,2,20,20,20,1\n\n";
  out << "[Events]\n";
  out << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
  auto ass_t = [](double sec) {
    int h = (int)(sec/3600), m=(int)std::fmod(sec,3600)/60, s=(int)std::fmod(sec,60);
    int cs = (int)(std::fmod(sec,1)*100);
    char b[32]; std::snprintf(b,sizeof(b),"%d:%02d:%02d.%02d",h,m,s,cs); return std::string(b);
  };
  for (const auto& c : cues) {
    std::string t = c.text;
    for (char& ch : t) if (ch == '\n') ch = '\\'; // simplified
    out << "Dialogue: 0," << ass_t(c.in_sec) << "," << ass_t(c.out_sec)
        << ",Default,,0,0,0,," << t << "\n";
  }
  return Error::success();
}

Error export_with_srt(const std::string& video, const std::string& srt,
                      const std::string& output) {
  std::ostringstream cmd;
  cmd << "ffmpeg -v error -y -i \"" << video << "\" -vf subtitles=\"" << srt
      << "\" -c:a copy \"" << output << "\"";
  int ec = std::system(cmd.str().c_str());
  if (ec != 0 || !fs::exists(output))
    return Error::make(ErrorCode::Unknown, "burn srt failed");
  return Error::success();
}

static std::string json_get_str(const std::string& j, const std::string& key) {
  std::string pat = "\"" + key + "\"";
  auto pos = j.find(pat);
  if (pos == std::string::npos) return {};
  pos = j.find(':', pos);
  while (pos < j.size() && (j[pos]==':'||j[pos]==' ')) ++pos;
  if (pos < j.size() && j[pos]=='"') {
    auto end = j.find('"', pos+1);
    return j.substr(pos+1, end-pos-1);
  }
  auto end = pos;
  while (end < j.size() && std::string(",}\n ").find(j[end])==std::string::npos) ++end;
  return j.substr(pos, end-pos);
}

Error project_from_json_template(const std::string& json, ProjectSession& session) {
  std::string name = json_get_str(json, "name");
  if (name.empty()) name = "Template";
  int w = std::atoi(json_get_str(json, "width").c_str());
  int h = std::atoi(json_get_str(json, "height").c_str());
  float fps = std::atof(json_get_str(json, "fps").c_str());
  if (w <= 0) w = 1280;
  if (h <= 0) h = 720;
  if (fps < 1) fps = 24;
  session.new_project(name, w, h, fps);
  TrackId tr = session.tracks().empty() ? session.add_track("V1", 0) : session.tracks()[0].id;

  // naive clip objects
  size_t pos = 0;
  while ((pos = json.find("\"path\"", pos)) != std::string::npos) {
    size_t block = json.rfind('{', pos);
    size_t end = json.find('}', pos);
    if (block == std::string::npos || end == std::string::npos) { pos += 5; continue; }
    std::string chunk = json.substr(block, end - block + 1);
    std::string path = json_get_str(chunk, "path");
    if (path.empty()) { pos = end + 1; continue; }
    MediaId mid = session.import_media(path);
    ClipInfo c;
    c.media_id = mid;
    c.start = std::atof(json_get_str(chunk, "start").c_str());
    c.duration = std::atof(json_get_str(chunk, "duration").c_str());
    c.source_in = std::atof(json_get_str(chunk, "source_in").c_str());
    if (c.duration <= 0) c.duration = 1.0;
    session.add_clip(tr, c);
    pos = end + 1;
  }
  return Error::success();
}

Error project_to_json_template(const ProjectSession& session, std::string& json,
                               const std::vector<CaptionCue>* captions) {
  std::ostringstream os;
  os << "{\n  \"name\": \"" << session.meta().name << "\",\n"
     << "  \"width\": " << session.meta().width << ",\n"
     << "  \"height\": " << session.meta().height << ",\n"
     << "  \"fps\": " << session.meta().fps << ",\n"
     << "  \"clips\": [\n";
  bool first = true;
  for (const auto& tr : session.tracks()) {
    for (const auto& c : tr.clips) {
      const MediaInfo* mi = session.find_media(c.media_id);
      if (!mi) continue;
      if (!first) os << ",\n";
      first = false;
      os << "    {\"path\": \"" << mi->path << "\", \"start\": " << c.start
         << ", \"duration\": " << c.duration << ", \"source_in\": " << c.source_in << "}";
    }
  }
  os << "\n  ],\n  \"captions\": [\n";
  if (captions) {
    for (size_t i = 0; i < captions->size(); ++i) {
      const auto& c = (*captions)[i];
      if (i) os << ",\n";
      os << "    {\"in\": " << c.in_sec << ", \"out\": " << c.out_sec
         << ", \"text\": \"" << c.text << "\"}";
    }
  }
  os << "\n  ]\n}\n";
  json = os.str();
  return Error::success();
}

} // namespace ms
