#pragma once
/** NLE transitions — Kdenlive / Resolve-style set */
#include "ms_image.hpp"
#include "ms_error.hpp"
#include "ms_types.hpp"
#include "ms_project.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

namespace ms {

enum class TransitionType : uint8_t {
  None = 0,
  // Fades
  Dissolve,
  FadeBlack,
  FadeWhite,
  FadeToColor,
  // Wipes
  WipeLeft,
  WipeRight,
  WipeUp,
  WipeDown,
  WipeDiagonalTL,  // top-left → bottom-right
  WipeDiagonalTR,
  WipeCircle,
  WipeBarnDoorH,   // open horizontal
  WipeBarnDoorV,
  // Push / slide
  SlideLeft,
  SlideRight,
  SlideUp,
  SlideDown,
  // Misc
  Iris,
  Zoom,
  DipToColor,
};

struct Transition {
  TransitionType type = TransitionType::Dissolve;
  TimeSec start = 0;
  TimeSec duration = 0.5;
  float softness = 0.05f;
  float color_r = 0, color_g = 0, color_b = 0; // FadeToColor / Dip
};

const char* transition_name(TransitionType t);

Error apply_transition(const ImageBuffer& a, const ImageBuffer& b,
                       TransitionType type, float u, float softness,
                       ImageBuffer& out);

Error apply_transition(const ImageBuffer& a, const ImageBuffer& b,
                       const Transition& tr, float u, ImageBuffer& out);

/** Timeline transition spanning two adjacent clips on a track */
struct ClipTransition {
  uint64_t id = 0;
  ClipId outgoing = 0; // left clip
  ClipId incoming = 0; // right clip
  TransitionType type = TransitionType::Dissolve;
  TimeSec duration = 0.5;
  float softness = 0.05f;
};

class TransitionList {
public:
  uint64_t add(const ClipTransition& t);
  bool remove(uint64_t id);
  const std::vector<ClipTransition>& all() const { return items_; }
  const ClipTransition* find_at(TimeSec t, const std::vector<ClipInfo>& clips) const;
private:
  std::vector<ClipTransition> items_;
  uint64_t next_ = 1;
};

} // namespace ms
