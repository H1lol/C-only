package com.motionstudio.app.export

import android.graphics.Bitmap
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.MediaMuxer
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.native.NativeEngine
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Renders the timeline out of the C++ engine and encodes it to H.264 with the
 * device encoder, muxing the engine's mixed PCM as AAC — no ffmpeg CLI.
 *
 * Video frames and audio spans are fed from one loop. Both output formats must
 * be known before the muxer can start, so any sample produced earlier is
 * buffered briefly rather than dropped.
 */
class VideoExporter(private val engine: EditorEngine) {

    data class Result(
        val file: File?,
        val frames: Int,
        val hasAudio: Boolean,
        val error: String?,
    )

    private class Pending(val track: Int, val data: ByteArray, val ptsUs: Long, val flags: Int)

    fun export(outFile: File, onProgress: (Float) -> Unit = {}): Result {
        val width = (engine.doc.width / 2 * 2).coerceAtLeast(2)
        val height = (engine.doc.height / 2 * 2).coerceAtLeast(2)
        val fps = (if (engine.fps > 1.0) engine.fps else 30.0).toInt().coerceIn(1, 120)
        val duration = engine.duration.coerceAtLeast(0.1)
        val totalFrames = (duration * fps).toInt().coerceIn(1, 60 * 60 * fps)
        val sampleRate = 48_000
        val channels = 2

        val wasPlaying = engine.playing
        val resumeTime = engine.time
        if (wasPlaying) engine.togglePlay()

        var videoCodec: MediaCodec? = null
        var audioCodec: MediaCodec? = null
        var muxer: MediaMuxer? = null
        var videoTrack = -1
        var audioTrack = -1
        var muxerStarted = false
        var framesWritten = 0
        var audioMuxed = false
        val pending = ArrayList<Pending>()
        val info = MediaCodec.BufferInfo()

        try {
            val videoFormat = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height).apply {
                setInteger(
                    MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
                )
                setInteger(MediaFormat.KEY_BIT_RATE, bitrateFor(width, height, fps))
                setInteger(MediaFormat.KEY_FRAME_RATE, fps)
                setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
            }
            videoCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
            videoCodec.configure(videoFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            videoCodec.start()

            val audioFormat = MediaFormat.createAudioFormat(
                MediaFormat.MIMETYPE_AUDIO_AAC, sampleRate, channels,
            ).apply {
                setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC)
                setInteger(MediaFormat.KEY_BIT_RATE, 160_000)
                setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 32_768)
            }
            audioCodec = runCatching {
                MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC).also {
                    it.configure(audioFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
                    it.start()
                }
            }.getOrNull()

            muxer = MediaMuxer(outFile.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)

            val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            val pixels = IntArray(width * height)
            val yuv = ByteArray(width * height * 3 / 2)
            val frameDurationUs = 1_000_000L / fps
            val audioChunk = (sampleRate / fps).coerceAtLeast(256)

            fun writeSample(track: Int, codec: MediaCodec, index: Int) {
                val buffer = codec.getOutputBuffer(index)
                if (buffer == null || info.size <= 0) return
                if ((info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) return
                buffer.position(info.offset)
                buffer.limit(info.offset + info.size)
                if (muxerStarted) {
                    muxer.writeSampleData(track, buffer, info)
                } else {
                    val bytes = ByteArray(info.size)
                    buffer.get(bytes)
                    pending.add(Pending(track, bytes, info.presentationTimeUs, info.flags))
                }
            }

            /** Drains everything available. Returns true when end-of-stream was seen. */
            fun drain(codec: MediaCodec, isVideo: Boolean, track: () -> Int): Boolean {
                var eos = false
                while (true) {
                    val index = codec.dequeueOutputBuffer(info, 0L)
                    if (index == MediaCodec.INFO_TRY_AGAIN_LATER) break
                    if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                        if (!muxerStarted) {
                            val added = muxer.addTrack(codec.outputFormat)
                            if (isVideo) videoTrack = added else audioTrack = added
                        }
                        continue
                    }
                    if (index < 0) continue
                    writeSample(track(), codec, index)
                    if (info.size > 0) {
                        if (isVideo) framesWritten++ else audioMuxed = true
                    }
                    eos = eos || (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0
                    codec.releaseOutputBuffer(index, false)
                }
                return eos
            }

            fun startMuxerIfReady() {
                if (muxerStarted || videoTrack < 0) return
                if (audioCodec != null && audioTrack < 0) return
                muxer.start()
                muxerStarted = true
                for (p in pending) {
                    val direct = ByteBuffer.allocateDirect(p.data.size)
                    direct.put(p.data).flip()
                    val bi = MediaCodec.BufferInfo()
                    bi.set(0, p.data.size, p.ptsUs, p.flags)
                    muxer.writeSampleData(p.track, direct, bi)
                }
                pending.clear()
            }

            var framesQueued = 0
            var audioFed = 0L
            var audioEos = audioCodec == null
            var videoEosQueued = false
            var videoDone = false
            val pcm = FloatArray(audioChunk * channels)
            val audioBytes = ByteArray(audioChunk * channels * 2)

            while (!videoDone) {
                // --- video input
                if (framesQueued < totalFrames) {
                    val inIndex = videoCodec.dequeueInputBuffer(5_000L)
                    if (inIndex >= 0) {
                        val buffer = videoCodec.getInputBuffer(inIndex)!!
                        val t = framesQueued.toDouble() / fps
                        if (!renderInto(bitmap, pixels, t, width, height)) {
                            return Result(null, framesQueued, audioMuxed, "Engine could not render frame $framesQueued")
                        }
                        rgbaToNv12(pixels, width, height, yuv)
                        buffer.clear()
                        buffer.put(yuv)
                        videoCodec.queueInputBuffer(inIndex, 0, yuv.size, framesQueued * frameDurationUs, 0)
                        framesQueued++
                        onProgress(0.7f * framesQueued / totalFrames)
                    }
                } else if (!videoEosQueued) {
                    val inIndex = videoCodec.dequeueInputBuffer(5_000L)
                    if (inIndex >= 0) {
                        videoCodec.queueInputBuffer(
                            inIndex, 0, 0, framesQueued * frameDurationUs, MediaCodec.BUFFER_FLAG_END_OF_STREAM,
                        )
                        videoEosQueued = true
                    }
                }

                // --- audio input (one span per video frame)
                if (!audioEos && audioCodec != null) {
                    val startSec = audioFed.toDouble() / sampleRate
                    if (startSec >= duration) {
                        val aIn = audioCodec.dequeueInputBuffer(5_000L)
                        if (aIn >= 0) {
                            audioCodec.queueInputBuffer(
                                aIn, 0, 0, framesQueued * frameDurationUs, MediaCodec.BUFFER_FLAG_END_OF_STREAM,
                            )
                            audioEos = true
                        }
                    } else {
                        engine.seek(startSec)
                        val read = if (NativeEngine.nativeReadAudioPcm(pcm, audioChunk, channels)) audioChunk else 0
                        val aIn = audioCodec.dequeueInputBuffer(5_000L)
                        if (aIn >= 0 && read > 0) {
                            val sb = ByteBuffer.wrap(audioBytes).order(ByteOrder.LITTLE_ENDIAN)
                            var s = 0
                            while (s < read * channels) {
                                sb.putShort(s, (pcm[s].coerceIn(-1f, 1f) * 32767f).toInt().toShort())
                                s++
                            }
                            val ab = audioCodec.getInputBuffer(aIn)!!
                            ab.clear()
                            ab.put(audioBytes, 0, read * channels * 2)
                            audioCodec.queueInputBuffer(
                                aIn, 0, read * channels * 2, audioFed * 1_000_000L / sampleRate, 0,
                            )
                            audioFed += read
                        }
                    }
                }

                val vDone = drain(videoCodec, true) { videoTrack }
                if (audioCodec != null) drain(audioCodec, false) { audioTrack }
                startMuxerIfReady()
                if (vDone && videoEosQueued) videoDone = true
            }

            // Flush the audio encoder tail.
            if (audioCodec != null && !audioEos) {
                val aIn = audioCodec.dequeueInputBuffer(5_000L)
                if (aIn >= 0) {
                    audioCodec.queueInputBuffer(
                        aIn, 0, 0, framesQueued * frameDurationUs, MediaCodec.BUFFER_FLAG_END_OF_STREAM,
                    )
                    audioEos = true
                }
            }
            if (audioCodec != null && audioTrack >= 0) {
                var guard = 0
                while (guard++ < 500) {
                    val index = audioCodec.dequeueOutputBuffer(info, 5_000L)
                    if (index == MediaCodec.INFO_TRY_AGAIN_LATER) break
                    if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) continue
                    if (index < 0) continue
                    writeSample(audioTrack, audioCodec, index)
                    if (info.size > 0) audioMuxed = true
                    val eos = (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0
                    audioCodec.releaseOutputBuffer(index, false)
                    if (eos) break
                }
            }

            bitmap.recycle()
            return Result(outFile, framesWritten, audioMuxed, null)
        } catch (t: Throwable) {
            runCatching { outFile.delete() }
            return Result(null, framesWritten, audioMuxed, t.message ?: t.javaClass.simpleName)
        } finally {
            runCatching { videoCodec?.stop() }
            runCatching { videoCodec?.release() }
            runCatching { audioCodec?.stop() }
            runCatching { audioCodec?.release() }
            if (muxerStarted) runCatching { muxer?.stop() }
            runCatching { muxer?.release() }
            engine.seek(resumeTime)
            if (wasPlaying) engine.togglePlay()
        }
    }

    private fun renderInto(bitmap: Bitmap, pixels: IntArray, time: Double, w: Int, h: Int): Boolean {
        engine.seek(time)
        if (!NativeEngine.nativeRenderFrame()) return false
        if (!NativeEngine.nativeCopyFrame(bitmap)) return false
        bitmap.getPixels(pixels, 0, w, 0, 0, w, h)
        return true
    }

    private fun bitrateFor(w: Int, h: Int, fps: Int): Int {
        val pixels = w.toLong() * h.toLong() * fps.toLong()
        val target = (pixels * 0.09).toLong()
        return target.coerceIn(1_000_000L, 40_000_000L).toInt()
    }

    /** ARGB ints → NV12 (BT.601, full range). */
    private fun rgbaToNv12(pixels: IntArray, width: Int, height: Int, out: ByteArray) {
        val frameSize = width * height
        var yIndex = 0
        var uvIndex = frameSize
        var i = 0
        while (i < pixels.size) {
            val p = pixels[i]
            val r = (p shr 16) and 0xFF
            val g = (p shr 8) and 0xFF
            val b = p and 0xFF
            val y = ((66 * r + 129 * g + 25 * b + 128) shr 8) + 16
            out[yIndex++] = y.coerceIn(16, 235).toByte()
            if ((i / width) % 2 == 0 && (i % width) % 2 == 0 && uvIndex + 1 < out.size) {
                val u = ((-38 * r - 74 * g + 112 * b + 128) shr 8) + 128
                val v = ((112 * r - 94 * g - 18 * b + 128) shr 8) + 128
                out[uvIndex++] = u.coerceIn(0, 255).toByte()
                out[uvIndex++] = v.coerceIn(0, 255).toByte()
            }
            i++
        }
    }

}
