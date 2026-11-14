#include "ms_animation.hpp"
#include <algorithm>
#include <functional>
#include <cctype>
#include <cmath>
#include <sstream>
#include <functional>

namespace ms {

Transform2D transform_at(const PropertySet& props, double time, const Transform2D& base) {
  Transform2D t = base;
  t.position.x = static_cast<float>(props.get("position.x", time, base.position.x));
  t.position.y = static_cast<float>(props.get("position.y", time, base.position.y));
  t.scale.x = static_cast<float>(props.get("scale.x", time, base.scale.x));
  t.scale.y = static_cast<float>(props.get("scale.y", time, base.scale.y));
  if (props.find("scale") && !props.find("scale.x")) {
    float s = static_cast<float>(props.get("scale", time, 1.0));
    t.scale.x = t.scale.y = s;
  }
  t.rotation = static_cast<float>(props.get("rotation", time, base.rotation));
  t.anchor.x = static_cast<float>(props.get("anchor.x", time, base.anchor.x));
  t.anchor.y = static_cast<float>(props.get("anchor.y", time, base.anchor.y));
  return t;
}

Error motion_blur_accumulate(
    double time, double shutter, int samples,
    int width, int height,
    const std::function<Error(double, ImageBuffer&)>& sample_fn,
    ImageBuffer& out) {
  samples = std::max(1, samples);
  if (!out.allocate(width, height, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "mblur");
  std::vector<float> acc(static_cast<size_t>(width) * height * 4, 0.f);
  const double half = shutter * 0.5;
  for (int i = 0; i < samples; ++i) {
    double u = samples == 1 ? 0.0 : (double)i / (samples - 1);
    double t = time - half + shutter * u;
    ImageBuffer frame;
    Error e = sample_fn(t, frame);
    if (!e.ok()) return e;
    if (!frame.valid() || frame.width() != width || frame.height() != height) {
      frame.allocate(width, height);
      frame.clear(0, 0, 0, 0);
    }
    const uint8_t* p = frame.data();
    for (size_t k = 0; k < acc.size(); ++k) acc[k] += p[k] / 255.f;
  }
  uint8_t* d = out.data();
  for (size_t k = 0; k < acc.size(); ++k) {
    float v = acc[k] / samples;
    d[k] = static_cast<uint8_t>(std::clamp(v, 0.f, 1.f) * 255.f + 0.5f);
  }
  return Error::success();
}

double eval_simple_expression(const std::string& expr, const PropertySet& props, double time) {
  // Tokenizer very small: numbers, $names, + - * /
  struct Tok { enum Type { Num, Id, Op, End } type; double num; char op; std::string id; };
  std::vector<Tok> toks;
  size_t i = 0;
  while (i < expr.size()) {
    if (std::isspace(static_cast<unsigned char>(expr[i]))) { ++i; continue; }
    if (std::isdigit(static_cast<unsigned char>(expr[i])) || expr[i] == '.') {
      size_t j = i;
      while (j < expr.size() && (std::isdigit(static_cast<unsigned char>(expr[j])) || expr[j] == '.')) ++j;
      Tok t; t.type = Tok::Num; t.num = std::stod(expr.substr(i, j - i)); toks.push_back(t);
      i = j; continue;
    }
    if (expr[i] == '$') {
      size_t j = ++i;
      while (j < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[j])) || expr[j] == '_' || expr[j] == '.')) ++j;
      Tok t; t.type = Tok::Id; t.id = expr.substr(i, j - i); toks.push_back(t);
      i = j; continue;
    }
    if (std::string("+-*/").find(expr[i]) != std::string::npos) {
      Tok t; t.type = Tok::Op; t.op = expr[i]; toks.push_back(t); ++i; continue;
    }
    ++i; // skip unknown
  }
  Tok end; end.type = Tok::End; toks.push_back(end);

  auto value = [&](const Tok& t) -> double {
    if (t.type == Tok::Num) return t.num;
    if (t.type == Tok::Id) return props.get(t.id, time, 0.0);
    return 0.0;
  };

  if (toks.empty() || toks[0].type == Tok::End) return 0.0;
  double acc = value(toks[0]);
  for (size_t k = 1; k + 1 < toks.size(); k += 2) {
    if (toks[k].type != Tok::Op) break;
    double rhs = value(toks[k + 1]);
    switch (toks[k].op) {
      case '+': acc += rhs; break;
      case '-': acc -= rhs; break;
      case '*': acc *= rhs; break;
      case '/': acc = (std::abs(rhs) < 1e-12) ? 0.0 : acc / rhs; break;
    }
  }
  return acc;
}

Error render_text_placeholder(ImageBuffer& out, int w, int h,
                              const std::string& text,
                              float r, float g, float b) {
  if (!out.allocate(w, h, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "text");
  out.clear(0, 0, 0, 0);
  // Block-glyph placeholder: draw bars for each character in a row
  const int row = h / 2;
  const int glyph_w = std::max(2, w / static_cast<int>(std::max<size_t>(1, text.size() + 1)));
  int x = glyph_w / 2;
  for (size_t ci = 0; ci < text.size() && x < w - 2; ++ci) {
    if (text[ci] == ' ') { x += glyph_w; continue; }
    for (int yy = row - h / 8; yy < row + h / 8; ++yy) {
      for (int xx = x; xx < x + glyph_w / 2 && xx < w; ++xx) {
        if (yy < 0 || yy >= h) continue;
        uint8_t* p = out.data() + (yy * w + xx) * 4;
        p[0] = static_cast<uint8_t>(std::clamp(r, 0.f, 1.f) * 255);
        p[1] = static_cast<uint8_t>(std::clamp(g, 0.f, 1.f) * 255);
        p[2] = static_cast<uint8_t>(std::clamp(b, 0.f, 1.f) * 255);
        p[3] = 255;
      }
    }
    x += glyph_w;
  }
  return Error::success();
}

Error render_shape_rect(ImageBuffer& out, int w, int h,
                        float r, float g, float b, float margin_n) {
  if (!out.allocate(w, h, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "shape");
  out.clear(0, 0, 0, 0);
  int m = static_cast<int>(margin_n * std::min(w, h));
  for (int y = m; y < h - m; ++y)
    for (int x = m; x < w - m; ++x) {
      uint8_t* p = out.data() + (y * w + x) * 4;
      p[0] = static_cast<uint8_t>(r * 255); p[1] = static_cast<uint8_t>(g * 255);
      p[2] = static_cast<uint8_t>(b * 255); p[3] = 255;
    }
  return Error::success();
}

Error render_shape_ellipse(ImageBuffer& out, int w, int h, float r, float g, float b) {
  if (!out.allocate(w, h, PixelFormat::RGBA8))
    return Error::make(ErrorCode::OutOfMemory, "ellipse");
  out.clear(0, 0, 0, 0);
  float cx = w * 0.5f, cy = h * 0.5f, rx = w * 0.35f, ry = h * 0.35f;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      float nx = (x - cx) / rx, ny = (y - cy) / ry;
      if (nx * nx + ny * ny <= 1.f) {
        uint8_t* p = out.data() + (y * w + x) * 4;
        p[0] = static_cast<uint8_t>(r * 255); p[1] = static_cast<uint8_t>(g * 255);
        p[2] = static_cast<uint8_t>(b * 255); p[3] = 255;
      }
    }
  return Error::success();
}

} // namespace ms
