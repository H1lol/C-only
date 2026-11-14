#pragma once

#include "ms_types.hpp"
#include <string>
#include <memory>
#include <functional>

namespace ms {

// ============================================================
// Media decoder interface (Android MediaCodec / iOS VideoToolbox)
// ============================================================

struct VideoFrame {
    uint32_t texture_id = 0;   // OES or regular texture
    int      width      = 0;
    int      height     = 0;
    TimeSec  pts        = 0.0;
    bool     is_eos     = false;
};

class IMediaDecoder {
public:
    virtual ~IMediaDecoder() = default;

    virtual bool open(const std::string& path) = 0;
    virtual void close() = 0;

    virtual bool seek(TimeSec t) = 0;
    virtual bool decode_next(VideoFrame& out) = 0;

    virtual TimeSec duration() const = 0;
    virtual int     width() const = 0;
    virtual int     height() const = 0;
    virtual float   fps() const = 0;
};

// ============================================================
// Media encoder interface
// ============================================================

struct EncodeConfig {
    int   width     = 1080;
    int   height    = 1920;
    float fps       = 30.f;
    int   bitrate   = 8'000'000;   // 8 Mbps
    std::string path;
};

class IMediaEncoder {
public:
    virtual ~IMediaEncoder() = default;

    virtual bool open(const EncodeConfig& cfg) = 0;
    virtual void close() = 0;

    // Feed a rendered texture / frame
    virtual bool encode_frame(uint32_t texture_id, TimeSec pts) = 0;
    virtual bool finish() = 0;
};

// Factory (platform specific later)
std::unique_ptr<IMediaDecoder> create_decoder();
std::unique_ptr<IMediaEncoder> create_encoder();

} // namespace ms
