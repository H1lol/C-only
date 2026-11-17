package com.motionstudio.app.export

import android.media.MediaFormat
import kotlin.math.roundToInt

/**
 * Everything the on-device encoder needs that the project document does not own.
 *
 * The project owns *what* the timeline is (canvas, clips, mix); this owns *how*
 * it gets delivered: output size, frame rate, bitrate, codec and range. Keeping
 * them apart means exporting a 60 fps preview of a 24 fps project never mutates
 * the document.
 */
data class ExportSettings(
    val width: Int = 1920,
    val height: Int = 1080,
    val fps: Double = 30.0,
    val bitrateKbps: Int = 12_000,
    val audioBitrateKbps: Int = 192,
    val codec: Codec = Codec.H264,
    val startSec: Double = 0.0,
    /** 0 (or anything at/below [startSec]) means "to the end of the timeline". */
    val endSec: Double = 0.0,
    val presetId: String = "custom",
) {
    enum class Codec(val label: String, val mime: String) {
        H264("H.264", MediaFormat.MIMETYPE_VIDEO_AVC),
        HEVC("HEVC", MediaFormat.MIMETYPE_VIDEO_HEVC),
    }

    /** Frame rates an NDV/delivery workflow actually uses, including the drop-frame ones. */
    val saneFps: Double get() = if (fps >= 1.0 && fps <= 240.0) fps else 30.0

    val evenWidth: Int get() = (width / 2 * 2).coerceIn(16, 7680)
    val evenHeight: Int get() = (height / 2 * 2).coerceIn(16, 4320)

    /** Clamped end for a timeline that ends at [timelineEnd]. */
    fun resolvedEnd(timelineEnd: Double): Double =
        if (endSec > startSec + 1e-6) endSec.coerceAtMost(timelineEnd) else timelineEnd

    fun durationSec(timelineEnd: Double): Double =
        (resolvedEnd(timelineEnd) - startSec).coerceAtLeast(1.0 / saneFps)

    fun frameCount(timelineEnd: Double): Int =
        (durationSec(timelineEnd) * saneFps).roundToInt().coerceAtLeast(1)

    /** Container overhead is small enough that bitrate × time is the honest estimate. */
    fun estimatedMegabytes(timelineEnd: Double): Double =
        (bitrateKbps + audioBitrateKbps) / 8.0 / 1024.0 * durationSec(timelineEnd)

    /** Same canvas, different scale, keeping the delivery aspect. */
    fun scaledToWidth(newWidth: Int): ExportSettings {
        val aspect = height.toDouble() / width.coerceAtLeast(1)
        return copy(width = newWidth, height = (newWidth * aspect).roundToInt())
    }

    companion object {
        /** A sensible delivery configuration for the current project canvas. */
        fun fromProject(width: Int, height: Int, fps: Double): ExportSettings {
            val w = (width / 2 * 2).coerceAtLeast(16)
            val h = (height / 2 * 2).coerceAtLeast(16)
            val rate = if (fps >= 1.0) fps else 30.0
            val pixels = w.toLong() * h.toLong() * rate.toLong()
            val bitrate = (pixels * 0.09).toLong().coerceIn(2_000_000L, 60_000_000L)
            return ExportSettings(
                width = w,
                height = h,
                fps = rate,
                bitrateKbps = (bitrate / 1000).toInt(),
            )
        }

        /** "1920×1080 29.97 fps 12.0 Mbps" — the one-line summary sheets show. */
        fun describe(s: ExportSettings): String =
            "${s.evenWidth}×${s.evenHeight} · ${formatFps(s.saneFps)} fps · " +
                "${"%.1f".format(s.bitrateKbps / 1000.0)} Mbps · ${s.codec.label}"

        fun formatFps(fps: Double): String {
            val whole = fps.roundToInt()
            return if (kotlin.math.abs(fps - whole) < 0.005) whole.toString()
            else "%.2f".format(fps)
        }
    }
}

/** Built-in delivery targets, the equivalent of a preset list in a desktop NLE. */
object ExportPresets {

    data class Delivery(
        val id: String,
        val label: String,
        val detail: String,
        val width: Int,
        val height: Int,
        val fps: Double,
        val bitrateKbps: Int,
        val codec: ExportSettings.Codec,
    ) {
        /** Presets with a 0 size keep the project canvas (e.g. Master 24p). */
        fun applyTo(base: ExportSettings): ExportSettings = base.copy(
            width = if (width > 0) width else base.width,
            height = if (height > 0) height else base.height,
            fps = fps,
            bitrateKbps = bitrateKbps,
            codec = codec,
            presetId = id,
        )
    }

    val delivery: List<Delivery> = listOf(
        Delivery("yt1080", "YouTube 1080p", "16:9 · 30p · H.264", 1920, 1080, 30.0, 12_000, ExportSettings.Codec.H264),
        Delivery("yt4k", "YouTube 4K", "16:9 · 60p · HEVC", 3840, 2160, 60.0, 45_000, ExportSettings.Codec.HEVC),
        Delivery("web720", "Web 720p", "16:9 · 30p · H.264", 1280, 720, 30.0, 6_000, ExportSettings.Codec.H264),
        Delivery("social1080", "Social 9:16", "1080×1920 · 30p", 1080, 1920, 30.0, 12_000, ExportSettings.Codec.H264),
        Delivery("square", "Square 1:1", "1080×1080 · 30p", 1080, 1080, 30.0, 10_000, ExportSettings.Codec.H264),
        Delivery("master24", "Master 24p", "project size · 24p · H.264", 0, 0, 24.0, 20_000, ExportSettings.Codec.H264),
        Delivery("proxy", "Proxy", "960×540 · 30p · light", 960, 540, 30.0, 2_500, ExportSettings.Codec.H264),
    )

    /** Frame rates offered as chips; any other value can still be typed in. */
    val frameRates: List<Double> = listOf(23.976, 24.0, 25.0, 29.97, 30.0, 50.0, 59.94, 60.0)

    fun masterApply(base: ExportSettings): ExportSettings = base.copy(presetId = "master")
}
