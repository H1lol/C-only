package com.motionstudio.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.model.NativeClip
import com.motionstudio.app.model.NativeTrack
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts
import kotlin.math.abs
import kotlin.math.roundToInt

private val TubeHeight = 40.dp
private val RulerHeight = 22.dp
private val HeaderWidth = 92.dp

/**
 * One horizontal scroll state drives the ruler and every lane, so the playhead
 * can never drift out of alignment with the clips underneath it.
 *
 * Clips can be dragged (move), edge-dragged (trim), tapped (select) and
 * long-pressed (delete); every gesture commits through the C++ engine so undo
 * stays authoritative.
 */
@Composable
fun TimelinePane(engine: EditorEngine, modifier: Modifier = Modifier) {
    val doc = engine.doc
    val duration = engine.duration
    var zoom by remember { mutableFloatStateOf(1f) }
    val scroll = rememberScrollState()

    BoxWithConstraints(modifier) {
        val laneViewport: Dp = (maxWidth - HeaderWidth).coerceAtLeast(1.dp)
        val contentWidth: Dp = laneViewport * zoom
        val secondsToDp: (Double) -> Dp = { seconds ->
            ((seconds / duration).toFloat() * contentWidth.value).dp
        }
        Row(Modifier.fillMaxWidth().fillMaxHeight()) {
            Column(Modifier.width(HeaderWidth).fillMaxHeight()) {
                Row(
                    Modifier.fillMaxWidth().height(RulerHeight).padding(start = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        "${doc.tracks.size} TRACKS",
                        color = MsColors.textMuted,
                        fontSize = 9.sp,
                        fontFamily = MsFonts.mono,
                        fontWeight = FontWeight.Bold,
                    )
                    Spacer(Modifier.weight(1f))
                    IconButton(
                        onClick = { engine.addTrack(video = true) },
                        modifier = Modifier.size(22.dp),
                    ) {
                        Icon(Icons.Filled.Add, "Add track", tint = MsColors.textTertiary)
                    }
                }
                doc.tracks.forEach { track ->
                    TrackHeader(
                        track = track,
                        onToggleMute = { engine.toggleTrackMuted(track.id) },
                        onDelete = { engine.removeTrack(track.id) },
                    )
                }
            }

            Box(Modifier.weight(1f).fillMaxHeight().horizontalScroll(scroll)) {
                Box(Modifier.width(contentWidth).fillMaxHeight()) {
                    Column(Modifier.fillMaxWidth()) {
                        Ruler(duration = duration, width = contentWidth)
                        if (doc.tracks.isEmpty()) {
                            Box(
                                Modifier.fillMaxWidth().height(72.dp),
                                contentAlignment = Alignment.Center,
                            ) {
                                Text(
                                    "Import media to start your timeline",
                                    color = MsColors.textMuted,
                                    fontSize = 12.sp,
                                )
                            }
                        }
                        doc.tracks.forEach { track ->
                            Lane(
                                track = track,
                                duration = duration,
                                contentWidth = contentWidth,
                                engine = engine,
                            )
                        }
                    }

                    // Playhead lives inside the scrolled content so it stays glued
                    // to the clips at any zoom or scroll offset.
                    val playheadX = secondsToDp(engine.time)
                    Box(
                        Modifier
                            .offset(x = playheadX - 1.dp)
                            .width(2.dp)
                            .fillMaxHeight()
                            .background(MsColors.accentSoft)
                    )
                }

                // Tap or drag empty timeline space to scrub.
                Box(
                    Modifier
                        .matchParentSize()
                        .pointerInput(contentWidth, duration) {
                            detectTapGestures { offset ->
                                engine.seek(secondsAt(offset.x, size.width, duration))
                            }
                        }
                        .pointerInput(contentWidth, duration) {
                            detectDragGestures { change, _ ->
                                engine.seek(secondsAt(change.position.x, size.width, duration))
                            }
                        }
                )
            }
        }
    }
}

private fun secondsAt(x: Float, widthPx: Int, duration: Double): Double {
    if (widthPx <= 0) return 0.0
    val ratio = (x / widthPx).coerceIn(0f, 1f)
    return ratio * duration
}

@Composable
private fun Ruler(duration: Double, width: Dp) {
    val step = niceStep(duration)
    Box(
        Modifier.width(width).height(RulerHeight).background(MsColors.surface),
        contentAlignment = Alignment.CenterStart,
    ) {
        var t = 0.0
        while (t <= duration + 0.001) {
            val x = ((t / duration).toFloat() * width.value)
            Box(
                Modifier
                    .offset(x = x.dp)
                    .width(1.dp)
                    .fillMaxHeight()
                    .background(MsColors.border)
            )
            Text(
                text = formatSeconds(t),
                color = MsColors.textMuted,
                fontSize = 9.sp,
                fontFamily = MsFonts.mono,
                modifier = Modifier.offset(x = (x + 3).dp),
            )
            t += step
        }
    }
}

@Composable
private fun TrackHeader(track: NativeTrack, onToggleMute: () -> Unit, onDelete: () -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(TubeHeight)
            .padding(horizontal = 8.dp)
            .pointerInput(track.id, track.muted) {
                detectTapGestures(
                    onTap = { onToggleMute() },
                    onLongPress = { onDelete() },
                )
            },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier
                .width(3.dp)
                .height(18.dp)
                .clip(RoundedCornerShape(2.dp))
                .background(if (track.muted) MsColors.textMuted else trackColor(track))
        )
        Spacer(Modifier.width(6.dp))
        Column {
            Text(
                track.name,
                color = if (track.locked) MsColors.textMuted else MsColors.textSecondary,
                fontSize = 12.sp,
                maxLines = 1,
            )
            Text(
                when {
                    track.muted -> "MUTED"
                    track.type == 1 -> "AUDIO"
                    else -> "VIDEO"
                },
                color = if (track.muted) MsColors.warning else MsColors.textMuted,
                fontSize = 9.sp,
                fontFamily = MsFonts.mono,
            )
        }
    }
}

@Composable
private fun Lane(
    track: NativeTrack,
    duration: Double,
    contentWidth: Dp,
    engine: EditorEngine,
) {
    Box(
        Modifier
            .width(contentWidth)
            .height(TubeHeight)
            .background(if (track.visible) Color.Transparent else MsColors.surface.copy(alpha = 0.4f))
            .border(0.5.dp, MsColors.border.copy(alpha = 0.6f)),
    ) {
        track.clips.forEach { clip ->
            ClipBox(
                track = track,
                clip = clip,
                duration = duration,
                contentWidth = contentWidth,
                selected = clip.id == engine.selectedClipId,
                engine = engine,
            )
        }
    }
}

@Composable
private fun ClipBox(
    track: NativeTrack,
    clip: NativeClip,
    duration: Double,
    contentWidth: Dp,
    selected: Boolean,
    engine: EditorEngine,
) {
    var start by remember(clip.id, clip.start) { mutableFloatStateOf(clip.start.toFloat()) }
    var length by remember(clip.id, clip.duration) { mutableFloatStateOf(clip.duration.toFloat()) }
    var dragging by remember(clip.id) { mutableStateOf(false) }
    var trimMode by remember(clip.id) { mutableStateOf(false) }

    val startFraction = (start / duration).toFloat().coerceIn(0f, 1f)
    val widthFraction = (length / duration).toFloat().coerceIn(0.01f, 1f)

    Box(
        Modifier
            .offset(x = (contentWidth.value * startFraction).dp, y = 6.dp)
            .width((contentWidth.value * widthFraction).dp)
            .height(TubeHeight - 12.dp)
            .clip(RoundedCornerShape(5.dp))
            .background(trackColor(track).copy(alpha = if (track.visible) 0.35f else 0.15f))
            .border(
                width = if (selected || dragging) 1.5.dp else 0.5.dp,
                color = when {
                    dragging -> MsColors.accentSoft
                    selected -> MsColors.accentSoft
                    else -> trackColor(track)
                },
                shape = RoundedCornerShape(5.dp),
            )
            .pointerInput(clip.id) {
                detectTapGestures(
                    onTap = { engine.select(track.id, clip.id) },
                    onLongPress = {
                        engine.select(track.id, clip.id)
                        engine.removeSelectedClip()
                    },
                )
            }
            .pointerInput(clip.id, clip.start, clip.duration, duration) {
                detectDragGestures(
                    onDragStart = { offset ->
                        trimMode = offset.x > size.width * 0.78f
                        dragging = true
                        engine.select(track.id, clip.id)
                    },
                    onDragCancel = { dragging = false },
                    onDragEnd = {
                        dragging = false
                        if (trimMode) {
                            engine.trimClip(track.id, clip.id, start.toDouble(), length.toDouble())
                        } else {
                            engine.moveClip(track.id, clip.id, snap(start.toDouble(), clip, engine))
                        }
                    },
                ) { change, drag ->
                    change.consume()
                    if (size.width <= 0) return@detectDragGestures
                    val secondsPerPx =
                        if (size.width > 0) (length.toDouble() / size.width).toFloat() else 0f
                    val delta = drag.x * secondsPerPx
                    if (trimMode) {
                        length = (length + delta).coerceIn(0.05f, duration.toFloat())
                    } else {
                        val maxStart = (duration - length).toFloat().coerceAtLeast(0f)
                        start = (start + delta).coerceIn(0f, maxStart)
                    }
                }
            },
        contentAlignment = Alignment.CenterStart,
    ) {
        // Audio tracks get the clip's real peak envelope behind the label; the
        // bars come from decoded PCM over the clip's source range.
        if (track.type == 2) {
            val peaks = remember(track.id, clip.id, clip.duration, clip.mediaId) {
                engine.peaks(track.id, clip.id, 96)
            }
            if (peaks.isNotEmpty()) {
                Canvas(Modifier.matchParentSize().padding(horizontal = 1.dp, vertical = 2.dp)) {
                    val mid = size.height / 2f
                    val bar = size.width / peaks.size
                    peaks.forEachIndexed { index, value ->
                        val amp = value.coerceIn(0f, 1f) * mid * 0.92f
                        if (amp <= 0.2f) return@forEachIndexed
                        drawRect(
                            color = MsColors.audioTrack.copy(alpha = 0.55f),
                            topLeft = Offset(index * bar, mid - amp),
                            size = Size((bar * 0.72f).coerceAtLeast(0.6f), amp * 2f),
                        )
                    }
                }
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                clip.name.ifBlank { "Clip" },
                color = MsColors.textPrimary,
                fontSize = 11.sp,
                maxLines = 1,
                modifier = Modifier.padding(horizontal = 6.dp),
            )
            if (clip.effects.isNotEmpty()) {
                Text(
                    "FX ${clip.effects.count { it.enabled }}",
                    color = MsColors.accentSoft,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                )
            }
        }
    }
}

/** Snaps a dragged start to the playhead or a neighbouring clip edge. */
private fun snap(candidate: Double, clip: NativeClip, engine: EditorEngine): Double {
    val targets = buildList {
        add(engine.time)
        engine.doc.tracks.forEach { track ->
            track.clips.forEach { other ->
                if (other.id != clip.id) {
                    add(other.start)
                    add(other.end)
                }
            }
        }
    }
    val tolerance = 0.12
    val nearest = targets.minByOrNull { abs(it - candidate) } ?: return candidate
    return if (abs(nearest - candidate) <= tolerance) nearest.coerceAtLeast(0.0) else candidate
}

private fun trackColor(track: NativeTrack): Color = when {
    track.muted -> MsColors.textMuted
    track.type == 1 -> MsColors.audioTrack
    track.name.contains("overlay", ignoreCase = true) -> MsColors.overlayTrack
    else -> MsColors.videoTrack
}

private fun niceStep(duration: Double): Double = when {
    duration <= 5 -> 0.5
    duration <= 15 -> 1.0
    duration <= 60 -> 5.0
    duration <= 180 -> 10.0
    else -> 60.0
}

private fun formatSeconds(value: Double): String {
    if (value == 0.0) return "0s"
    if (value < 10 && value % 1.0 != 0.0) return "${(value * 10).roundToInt() / 10.0}s"
    return "${value.roundToInt()}s"
}
