#pragma once

#include <string>
#include <optional>

namespace ms {

// ============================================================
// Unified error system for Phase 2+
// ============================================================

enum class ErrorCode : int {
    Ok = 0,

    // General
    Unknown,
    InvalidArgument,
    OutOfMemory,
    NotInitialized,
    AlreadyInitialized,
    NotSupported,

    // Project / File
    ProjectLoadFailed,
    ProjectSaveFailed,
    FileNotFound,
    FilePermission,
    FileCorrupt,

    // Media
    MediaOpenFailed,
    MediaDecodeFailed,
    MediaUnsupportedFormat,
    MediaProbeFailed,

    // GPU / Render
    GpuInitFailed,
    GpuContextLost,
    GpuOutOfMemory,
    ShaderCompileFailed,
    ShaderLinkFailed,
    TextureCreateFailed,
    FboCreateFailed,
    ReadbackFailed,

    // Effects
    EffectNotFound,
    EffectLoadFailed,
    EffectParamInvalid,
    EffectDownloadFailed,
    EffectChecksumMismatch,
    EffectIncompatibleVersion,

    // Network (for on-demand effects)
    NetworkUnavailable,
    NetworkTimeout,
    NetworkHttpError,

    // Playback
    PlaybackFailed,
};

struct Error {
    ErrorCode   code = ErrorCode::Ok;
    std::string message;
    std::string detail;       // technical detail (optional)
    std::string context;      // where it happened

    bool ok() const { return code == ErrorCode::Ok; }
    explicit operator bool() const { return ok(); }

    static Error success() { return {}; }

    static Error make(ErrorCode c, const std::string& msg,
                      const std::string& detail = "",
                      const std::string& ctx = "") {
        Error e;
        e.code = c;
        e.message = msg;
        e.detail = detail;
        e.context = ctx;
        return e;
    }
};

inline const char* error_code_string(ErrorCode c) {
    switch (c) {
        case ErrorCode::Ok: return "Ok";
        case ErrorCode::Unknown: return "Unknown";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::OutOfMemory: return "OutOfMemory";
        case ErrorCode::NotInitialized: return "NotInitialized";
        case ErrorCode::AlreadyInitialized: return "AlreadyInitialized";
        case ErrorCode::NotSupported: return "NotSupported";
        case ErrorCode::ProjectLoadFailed: return "ProjectLoadFailed";
        case ErrorCode::ProjectSaveFailed: return "ProjectSaveFailed";
        case ErrorCode::FileNotFound: return "FileNotFound";
        case ErrorCode::FilePermission: return "FilePermission";
        case ErrorCode::FileCorrupt: return "FileCorrupt";
        case ErrorCode::MediaOpenFailed: return "MediaOpenFailed";
        case ErrorCode::MediaDecodeFailed: return "MediaDecodeFailed";
        case ErrorCode::MediaUnsupportedFormat: return "MediaUnsupportedFormat";
        case ErrorCode::MediaProbeFailed: return "MediaProbeFailed";
        case ErrorCode::GpuInitFailed: return "GpuInitFailed";
        case ErrorCode::GpuContextLost: return "GpuContextLost";
        case ErrorCode::GpuOutOfMemory: return "GpuOutOfMemory";
        case ErrorCode::ShaderCompileFailed: return "ShaderCompileFailed";
        case ErrorCode::ShaderLinkFailed: return "ShaderLinkFailed";
        case ErrorCode::TextureCreateFailed: return "TextureCreateFailed";
        case ErrorCode::FboCreateFailed: return "FboCreateFailed";
        case ErrorCode::ReadbackFailed: return "ReadbackFailed";
        case ErrorCode::EffectNotFound: return "EffectNotFound";
        case ErrorCode::EffectLoadFailed: return "EffectLoadFailed";
        case ErrorCode::EffectParamInvalid: return "EffectParamInvalid";
        case ErrorCode::EffectDownloadFailed: return "EffectDownloadFailed";
        case ErrorCode::EffectChecksumMismatch: return "EffectChecksumMismatch";
        case ErrorCode::EffectIncompatibleVersion: return "EffectIncompatibleVersion";
        case ErrorCode::NetworkUnavailable: return "NetworkUnavailable";
        case ErrorCode::NetworkTimeout: return "NetworkTimeout";
        case ErrorCode::NetworkHttpError: return "NetworkHttpError";
        case ErrorCode::PlaybackFailed: return "PlaybackFailed";
        default: return "Unknown";
    }
}

} // namespace ms
