#pragma once
/**
 * Android platform decoding through AMediaCodec / AMediaExtractor (NDK).
 *
 * The desktop paths shell out to the `ffmpeg` CLI, which does not exist on
 * Android, so before this the engine silently fell back to SlateDecoder and
 * every imported clip rendered as a flat placeholder colour. This is the
 * decoder Android actually needs: hardware codecs through the platform API,
 * no bundled binaries and no extra licence obligations.
 *
 * Only compiled into the Android build; the desktop build keeps using FFmpeg.
 */
#include "ms_media.hpp"
#include "ms_audio.hpp"
#include "ms_error.hpp"
#include <memory>
#include <string>

namespace ms {

#if defined(__ANDROID__)

/** True when this build can decode through the platform. */
bool android_decoder_available();

/** Creates a MediaCodec-backed decoder for `path`, or nullptr. */
std::unique_ptr<IMediaDecoder> create_android_decoder(const std::string& path);

/**
 * Decodes the audio track of `path` to interleaved float PCM at `target_rate`.
 * Used for timeline playback and for waveform peaks on Android.
 */
Error android_decode_audio(const std::string& path, AudioBuffer& out,
                           int target_rate = 48000, int target_ch = 2,
                           double max_seconds = 600.0);

#endif // __ANDROID__

/** Human-readable name of the decoder backend in use ("mediacodec", "ffmpeg", "slate"). */
std::string decoder_backend_name();

} // namespace ms
