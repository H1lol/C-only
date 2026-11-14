#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace ms {

// ============================================================
// Basic types
// ============================================================

using TimeSec   = double;
using FrameIdx  = int64_t;
using TrackId   = uint64_t;
using ClipId    = uint64_t;
using EffectId  = uint64_t;
using MediaId   = uint64_t;

struct Vec2 {
    float x = 0.f;
    float y = 0.f;
};

struct Vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f;
};

struct Color {
    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
};

struct Rect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
};

struct Transform2D {
    Vec2  position {0.f, 0.f};
    Vec2  scale    {1.f, 1.f};
    float rotation = 0.f;   // degrees
    Vec2  anchor   {0.5f, 0.5f};
};

enum class BlendMode : uint8_t {
    Normal = 0,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion
};

enum class TrackType : uint8_t {
    Video = 0,
    Overlay,
    Audio,
    Adjustment,
    Text,
    Effect
};

// ============================================================
// Preview / Playback state
// ============================================================

struct PreviewState {
    TimeSec time        = 0.0;
    int     width       = 1080;
    int     height      = 1920;
    bool    playing     = false;
    float   fps         = 30.f;
    int64_t frame_index = 0;
};

// ============================================================
// Effect parameter
// ============================================================

enum class ParamType : uint8_t {
    Float = 0,
    Int,
    Bool,
    Color,
    Vec2,
    Enum
};

struct EffectParam {
    std::string name;
    ParamType   type  = ParamType::Float;
    float       value = 0.f;
    float       min_v = 0.f;
    float       max_v = 1.f;
    float       def_v = 0.f;
};

struct EffectInstance {
    EffectId    id;
    std::string base_id;          // "blur", "chromatic", "glitch_rgb"...
    bool        enabled = true;
    std::vector<EffectParam> params;
    TimeSec     start    = 0.0;
    TimeSec     duration = -1.0;  // -1 = full clip
};

} // namespace ms
