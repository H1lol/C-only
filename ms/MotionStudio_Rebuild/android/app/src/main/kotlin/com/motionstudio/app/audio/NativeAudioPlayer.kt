package com.motionstudio.app.audio

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import com.motionstudio.app.native.NativeEngine

/**
 * Pulls the engine's mixed timeline PCM on a worker thread and writes it to an
 * AudioTrack. The master insert chain is already applied by the engine.
 */
class NativeAudioPlayer {

    private var track: AudioTrack? = null
    private var worker: Thread? = null

    @Volatile
    private var running = false

    fun start(sampleRate: Int = 48_000, channels: Int = 2) {
        if (running) return
        val channelMask =
            if (channels == 1) AudioFormat.CHANNEL_OUT_MONO else AudioFormat.CHANNEL_OUT_STEREO
        val minBuffer = AudioTrack.getMinBufferSize(
            sampleRate,
            channelMask,
            AudioFormat.ENCODING_PCM_FLOAT,
        ).coerceAtLeast(sampleRate * channels * 4 / 5)

        val output = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelMask)
                    .build()
            )
            .setBufferSizeInBytes(minBuffer * 2)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()

        track = output
        running = true
        output.play()

        worker = Thread({
            val frames = 1024
            val buffer = FloatArray(frames * channels)
            while (running) {
                val ok = runCatching { NativeEngine.nativeReadAudioPcm(buffer, frames, channels) }
                    .getOrDefault(false)
                if (!ok) {
                    Thread.sleep(5)
                    continue
                }
                val written = output.write(buffer, 0, buffer.size, AudioTrack.WRITE_BLOCKING)
                if (written < 0) break
            }
        }, "ms-audio").apply {
            priority = Thread.MAX_PRIORITY
            start()
        }
    }

    fun stop() {
        running = false
        worker?.let { runCatching { it.join(250) } }
        worker = null
        track?.let { runCatching { it.pause() }; runCatching { it.flush() }; runCatching { it.release() } }
        track = null
    }
}
