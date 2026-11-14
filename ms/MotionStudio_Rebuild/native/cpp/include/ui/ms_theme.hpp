#pragma once
/** EffectCraft visual tokens — single source of truth for C++ UI */
namespace ms::ui {

struct Color {
  float r, g, b, a;
  static constexpr Color rgba(int R, int G, int B, int A = 255) {
    return {R / 255.f, G / 255.f, B / 255.f, A / 255.f};
  }
};

struct Theme {
  // Surfaces
  static constexpr Color background    = Color::rgba(0x1B, 0x1C, 0x1F);
  static constexpr Color surface       = Color::rgba(0x23, 0x23, 0x26);
  static constexpr Color surface_high  = Color::rgba(0x2A, 0x2A, 0x2E);
  static constexpr Color border        = Color::rgba(0x3A, 0x3A, 0x3A);
  // Accent (EffectCraft blue)
  static constexpr Color primary       = Color::rgba(0x2D, 0x8C, 0xEB);
  static constexpr Color primary_soft  = Color::rgba(0x4A, 0xA0, 0xF5);
  // Text
  static constexpr Color text          = Color::rgba(0xE3, 0xE3, 0xE3);
  static constexpr Color text_secondary= Color::rgba(0xC9, 0xC9, 0xCC);
  static constexpr Color text_muted    = Color::rgba(0x8C, 0x8C, 0x8C);
  // Semantic
  static constexpr Color error         = Color::rgba(0xFF, 0x8A, 0x7A);
  static constexpr Color success       = Color::rgba(0x3D, 0xDC, 0x97);

  static constexpr int panel_title_pt  = 11;
  static constexpr int body_pt         = 12;
  static constexpr int toolbar_h       = 36;
  static constexpr int panel_min_w     = 200;
};

enum class Workspace : int {
  Edit = 0,
  Motion,
  Nodes,
  Color,
  Audio,
  Render,
  Dev,
  Count
};

inline const char* workspace_name(Workspace w) {
  switch (w) {
    case Workspace::Edit:   return "Edit";
    case Workspace::Motion: return "Motion";
    case Workspace::Nodes:  return "Nodes";
    case Workspace::Color:  return "Color";
    case Workspace::Audio:  return "Audio";
    case Workspace::Render: return "Render";
    case Workspace::Dev:    return "Dev";
    default: return "?";
  }
}

} // namespace ms::ui
