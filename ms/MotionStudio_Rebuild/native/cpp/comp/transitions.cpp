#include "ms_transitions.hpp"
#include <cmath>

namespace ms {

const char* transition_name(TransitionType t) {
  switch (t) {
    case TransitionType::None: return "none";
    case TransitionType::Dissolve: return "dissolve";
    case TransitionType::FadeBlack: return "fade_black";
    case TransitionType::FadeWhite: return "fade_white";
    case TransitionType::FadeToColor: return "fade_to_color";
    case TransitionType::WipeLeft: return "wipe_left";
    case TransitionType::WipeRight: return "wipe_right";
    case TransitionType::WipeUp: return "wipe_up";
    case TransitionType::WipeDown: return "wipe_down";
    case TransitionType::WipeDiagonalTL: return "wipe_diag_tl";
    case TransitionType::WipeDiagonalTR: return "wipe_diag_tr";
    case TransitionType::WipeCircle: return "wipe_circle";
    case TransitionType::WipeBarnDoorH: return "barn_h";
    case TransitionType::WipeBarnDoorV: return "barn_v";
    case TransitionType::SlideLeft: return "slide_left";
    case TransitionType::SlideRight: return "slide_right";
    case TransitionType::SlideUp: return "slide_up";
    case TransitionType::SlideDown: return "slide_down";
    case TransitionType::Iris: return "iris";
    case TransitionType::Zoom: return "zoom";
    case TransitionType::DipToColor: return "dip_color";
    default: return "unknown";
  }
}

static void sample_buf(const ImageBuffer& img, float x, float y,
                       float& r, float& g, float& b, float& a) {
  const int w = img.width(), h = img.height();
  if (x < 0 || y < 0 || x >= w - 1 || y >= h - 1) {
    r = g = b = a = 0.f;
    return;
  }
  int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
  int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
  float tx = x - x0, ty = y - y0;
  auto px = [&](int X, int Y) {
    const uint8_t* p = img.data() + (Y * w + X) * 4;
    return p;
  };
  const uint8_t* p00 = px(x0, y0);
  const uint8_t* p10 = px(x1, y0);
  const uint8_t* p01 = px(x0, y1);
  const uint8_t* p11 = px(x1, y1);
  auto L = [](float a, float b, float t) { return a + (b - a) * t; };
  r = L(L(p00[0], p10[0], tx), L(p01[0], p11[0], tx), ty) / 255.f;
  g = L(L(p00[1], p10[1], tx), L(p01[1], p11[1], tx), ty) / 255.f;
  b = L(L(p00[2], p10[2], tx), L(p01[2], p11[2], tx), ty) / 255.f;
  a = L(L(p00[3], p10[3], tx), L(p01[3], p11[3], tx), ty) / 255.f;
}

Error apply_transition(const ImageBuffer& a, const ImageBuffer& b,
                       const Transition& tr, float u, ImageBuffer& out) {
  return apply_transition(a, b, tr.type, u, tr.softness, out);
}

Error apply_transition(const ImageBuffer& a, const ImageBuffer& b,
                       TransitionType type, float u, float softness,
                       ImageBuffer& out) {
  if (!a.valid() || !b.valid())
    return Error::make(ErrorCode::InvalidArgument, "transition inputs");
  const int w = a.width(), h = a.height();
  if (b.width() != w || b.height() != h)
    return Error::make(ErrorCode::InvalidArgument, "transition size mismatch");
  if (!out.allocate(w, h, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "transition out");

  u = std::clamp(u, 0.f, 1.f);
  softness = std::clamp(softness, 0.f, 0.5f);
  const uint8_t* A = a.data();
  const uint8_t* B = b.data();
  uint8_t* D = out.data();
  const int n = w * h;

  auto mix_pix = [&](int i, float t) {
    t = std::clamp(t, 0.f, 1.f);
    for (int c = 0; c < 4; ++c) {
      float va = A[i * 4 + c] / 255.f;
      float vb = B[i * 4 + c] / 255.f;
      D[i * 4 + c] = static_cast<uint8_t>(std::clamp(va * (1.f - t) + vb * t, 0.f, 1.f) * 255.f + 0.5f);
    }
  };

  auto soft_step = [&](float edge, float cut) {
    if (softness < 1e-4f) return edge < cut ? 1.f : 0.f;
    float lo = cut - softness, hi = cut + softness;
    if (edge <= lo) return 1.f;
    if (edge >= hi) return 0.f;
    return 1.f - (edge - lo) / (2.f * softness + 1e-6f);
  };

  switch (type) {
    case TransitionType::None:
      for (int i = 0; i < n; ++i) mix_pix(i, 0.f);
      break;
    case TransitionType::Dissolve:
      for (int i = 0; i < n; ++i) mix_pix(i, u);
      break;
    case TransitionType::FadeBlack:
    case TransitionType::DipToColor:
      for (int i = 0; i < n; ++i) {
        if (u < 0.5f) {
          float t = u * 2.f;
          for (int c = 0; c < 3; ++c)
            D[i*4+c] = static_cast<uint8_t>(A[i*4+c] * (1.f - t));
          D[i*4+3] = A[i*4+3];
        } else {
          float t = (u - 0.5f) * 2.f;
          for (int c = 0; c < 3; ++c)
            D[i*4+c] = static_cast<uint8_t>(B[i*4+c] * t);
          D[i*4+3] = B[i*4+3];
        }
      }
      break;
    case TransitionType::FadeWhite:
      for (int i = 0; i < n; ++i) {
        if (u < 0.5f) {
          float t = u * 2.f;
          for (int c = 0; c < 3; ++c) {
            float v = A[i*4+c] / 255.f;
            D[i*4+c] = static_cast<uint8_t>((v + (1.f - v) * t) * 255.f);
          }
          D[i*4+3] = A[i*4+3];
        } else {
          float t = (u - 0.5f) * 2.f;
          for (int c = 0; c < 3; ++c) {
            float v = B[i*4+c] / 255.f;
            D[i*4+c] = static_cast<uint8_t>((1.f + (v - 1.f) * t) * 255.f);
          }
          D[i*4+3] = B[i*4+3];
        }
      }
      break;
    case TransitionType::FadeToColor:
      for (int i = 0; i < n; ++i) {
        if (u < 0.5f) {
          float t = u * 2.f;
          for (int c = 0; c < 3; ++c) {
            float v = A[i*4+c] / 255.f;
            float col = (c == 0 ? 0.f : 0.f); // black default; color via Transition overload later
            D[i*4+c] = static_cast<uint8_t>((v * (1.f - t) + col * t) * 255.f);
          }
          D[i*4+3] = 255;
        } else {
          float t = (u - 0.5f) * 2.f;
          for (int c = 0; c < 3; ++c) {
            float v = B[i*4+c] / 255.f;
            D[i*4+c] = static_cast<uint8_t>((0.f * (1.f - t) + v * t) * 255.f);
          }
          D[i*4+3] = 255;
        }
      }
      break;
    case TransitionType::WipeLeft:
    case TransitionType::WipeRight:
    case TransitionType::WipeUp:
    case TransitionType::WipeDown:
    case TransitionType::WipeDiagonalTL:
    case TransitionType::WipeDiagonalTR:
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          int i = y * w + x;
          float edge = 0.f;
          if (type == TransitionType::WipeLeft) edge = (float)x / w;
          else if (type == TransitionType::WipeRight) edge = 1.f - (float)x / w;
          else if (type == TransitionType::WipeUp) edge = (float)y / h;
          else if (type == TransitionType::WipeDown) edge = 1.f - (float)y / h;
          else if (type == TransitionType::WipeDiagonalTL)
            edge = 0.5f * ((float)x / w + (float)y / h);
          else
            edge = 0.5f * ((1.f - (float)x / w) + (float)y / h);
          mix_pix(i, soft_step(edge, u));
        }
      }
      break;
    case TransitionType::WipeCircle:
    case TransitionType::Iris: {
      float cx = 0.5f, cy = 0.5f;
      float maxr = 0.75f;
      float rad = u * maxr;
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          int i = y * w + x;
          float nx = (float)x / w - cx, ny = (float)y / h - cy;
          float d = std::sqrt(nx * nx + ny * ny);
          mix_pix(i, soft_step(d, rad));
        }
      }
      break;
    }
    case TransitionType::WipeBarnDoorH:
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          int i = y * w + x;
          float edge = std::abs((float)x / w - 0.5f) * 2.f;
          mix_pix(i, soft_step(edge, u));
        }
      }
      break;
    case TransitionType::WipeBarnDoorV:
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          int i = y * w + x;
          float edge = std::abs((float)y / h - 0.5f) * 2.f;
          mix_pix(i, soft_step(edge, u));
        }
      }
      break;
    case TransitionType::SlideLeft:
    case TransitionType::SlideRight:
    case TransitionType::SlideUp:
    case TransitionType::SlideDown: {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          float ox = 0, oy = 0;
          if (type == TransitionType::SlideLeft) ox = u * w;
          else if (type == TransitionType::SlideRight) ox = -u * w;
          else if (type == TransitionType::SlideUp) oy = u * h;
          else oy = -u * h;
          float ar, ag, ab, aa, br, bg, bb, ba;
          sample_buf(a, x + ox, y + oy, ar, ag, ab, aa);
          sample_buf(b, x + ox - (type == TransitionType::SlideLeft ? w : type == TransitionType::SlideRight ? -w : 0),
                        y + oy - (type == TransitionType::SlideUp ? h : type == TransitionType::SlideDown ? -h : 0),
                        br, bg, bb, ba);
          // simpler: A moves out, B comes in
          float sxA = x, syA = y, sxB = x, syB = y;
          if (type == TransitionType::SlideLeft) { sxA = x + u * w; sxB = x + u * w - w; }
          if (type == TransitionType::SlideRight) { sxA = x - u * w; sxB = x - u * w + w; }
          if (type == TransitionType::SlideUp) { syA = y + u * h; syB = y + u * h - h; }
          if (type == TransitionType::SlideDown) { syA = y - u * h; syB = y - u * h + h; }
          sample_buf(a, sxA, syA, ar, ag, ab, aa);
          sample_buf(b, sxB, syB, br, bg, bb, ba);
          // prefer B if in frame
          float useB = (sxB >= 0 && sxB < w && syB >= 0 && syB < h) ? 1.f : 0.f;
          if (type == TransitionType::SlideLeft || type == TransitionType::SlideRight)
            useB = (sxB >= 0 && sxB < w) ? 1.f : 0.f;
          int i = y * w + x;
          float t = useB;
          D[i*4+0] = static_cast<uint8_t>(std::clamp(ar*(1-t)+br*t, 0.f, 1.f)*255);
          D[i*4+1] = static_cast<uint8_t>(std::clamp(ag*(1-t)+bg*t, 0.f, 1.f)*255);
          D[i*4+2] = static_cast<uint8_t>(std::clamp(ab*(1-t)+bb*t, 0.f, 1.f)*255);
          D[i*4+3] = static_cast<uint8_t>(std::clamp(aa*(1-t)+ba*t, 0.f, 1.f)*255);
        }
      }
      break;
    }
    case TransitionType::Zoom: {
      // A zooms out / B zooms in
      float scaleA = 1.f + u * 0.5f;
      float scaleB = 1.5f - u * 0.5f;
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          float nx = (x + 0.5f) / w - 0.5f;
          float ny = (y + 0.5f) / h - 0.5f;
          float ar, ag, ab, aa, br, bg, bb, ba;
          sample_buf(a, (nx / scaleA + 0.5f) * w, (ny / scaleA + 0.5f) * h, ar, ag, ab, aa);
          sample_buf(b, (nx / scaleB + 0.5f) * w, (ny / scaleB + 0.5f) * h, br, bg, bb, ba);
          int i = y * w + x;
          D[i*4+0] = static_cast<uint8_t>(std::clamp(ar*(1-u)+br*u, 0.f, 1.f)*255);
          D[i*4+1] = static_cast<uint8_t>(std::clamp(ag*(1-u)+bg*u, 0.f, 1.f)*255);
          D[i*4+2] = static_cast<uint8_t>(std::clamp(ab*(1-u)+bb*u, 0.f, 1.f)*255);
          D[i*4+3] = static_cast<uint8_t>(std::clamp(aa*(1-u)+ba*u, 0.f, 1.f)*255);
        }
      }
      break;
    }
  }
  return Error::success();
}

uint64_t TransitionList::add(const ClipTransition& t_in) {
  ClipTransition t = t_in;
  if (t.id == 0) t.id = next_++;
  items_.push_back(t);
  return items_.back().id;
}

bool TransitionList::remove(uint64_t id) {
  auto it = std::remove_if(items_.begin(), items_.end(),
                           [id](const ClipTransition& t) { return t.id == id; });
  if (it == items_.end()) return false;
  items_.erase(it, items_.end());
  return true;
}

const ClipTransition* TransitionList::find_at(TimeSec t, const std::vector<ClipInfo>& clips) const {
  for (const auto& tr : items_) {
    const ClipInfo *L = nullptr, *R = nullptr;
    for (const auto& c : clips) {
      if (c.id == tr.outgoing) L = &c;
      if (c.id == tr.incoming) R = &c;
    }
    if (!L || !R) continue;
    TimeSec mid = L->start + L->duration;
    TimeSec t0 = mid - tr.duration * 0.5;
    TimeSec t1 = mid + tr.duration * 0.5;
    if (t >= t0 && t <= t1) return &tr;
  }
  return nullptr;
}

} // namespace ms
